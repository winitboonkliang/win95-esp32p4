/* B82: Doom on the Win95 P4 screen (lab:LEAD/B82/REPORT.md). Owner 21 Nov: "when the game opens it takes every resource; on exit give
 * it back to Windows (= reopen Windows)".
 *  - Windows side: the Doom shortcut (DOOM.B76 via the B76 resident, or console "doom go ...") writes an RTC flag and restarts the board;
 *  - Doom boot (esp_main.c b82_boot): panel + SD + touch + USB keys + this engine only. No emulator, no presenter: all PSRAM, both cores,
 *    the whole PSRAM bus. 8-bit frame -> RGB565 (RV32 asm kernel, C oracle) -> PPA scale + rotate 90 into the DPI back buffer -> flip;
 *  - Quit / engine error / end of a timedemo -> flag cleared, restart -> Windows resumes from its golden snapshot.
 * Internal RAM: the engine's globals are PSRAM (.bss mapping + tools/b82_data2psram.py); the Doom boot uses internal RAM freely (transient). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_heap_caps_init.h"
#include "esp_cache.h"
#include "esp_attr.h"
#include "esp_system.h"
#include "hal/efuse_hal.h"
#include "hal/axi_icm_ll.h"
#include "soc/mipi_dsi_bridge_struct.h"
#include "doomgeneric.h"
#include "doomkeys.h"
#include "i_video.h"
#include "doomstat.h"
/* tic gate (B82 176dbf4): gametic/timingdemo of the launched engine via E->api */
#include <sys/stat.h>
#include "esp_rom_sys.h"
#include "b85.h"

bool b29_ppa_block_rot(const void *src_pic, size_t pic_w, size_t pic_h, size_t x, size_t y, size_t w, size_t h, void *dst, size_t dst_w, size_t dst_h, size_t dst_size, float scale);
void *lcd_back_fb(void); void *lcd_fb(int i); void lcd_swap_present(void); void lcd_wait_flip(void); int lcd_panel_ready(void);
void lcd_wait_refresh_after(uint32_t n); void *lcd_fbn(int i); void lcd_show(int i); extern volatile uint32_t lcd_refresh_n;

#define W DOOMGENERIC_RESX
#define H DOOMGENERIC_RESY
#define PW 800
#define PH 1280

/* ---- the hand-over between the boots (B83, lab:LEAD/B83/PARTITIONS.md): two retained LP store registers (kept over deep sleep and esp_restart, 0
 * after a power-up; IDF/ROM use STORE1..10). STORE14 = 0xD00D << 16 | engine (bit 0: 1 = Rust): the own bootloader
 * (esp/bootloader_components/main) then boots the doom app partition if it holds a valid image, else factory. STORE15 = td | vsync << 8
 * | kasm << 16 | pclk << 24. td bits 0-3: 0 = play, N = timedemo demoN; bits 4-5: WAD (wads[]); vsync bit0 = wait, bit1 = 3 DPI buffers,
 * bit2 = B82 pipe, bit3 = B83 nopar, bit4 = B83 noconv2; td bit 6: -nosound (B84 A/B), td bit 7: loud (B84 safety test) */
#include "soc/lp_system_reg.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#define B83_REG  LP_SYSTEM_REG_LP_STORE14_REG
#define B83_ARGS LP_SYSTEM_REG_LP_STORE15_REG
#define B83_MAGIC 0xD00Du
typedef struct { uint8_t td, vsync, kasm, pclk, engine; } B82Flag;
/* 0 = shareware (shipped), 1 = The Ultimate Doom, 2 = Doom II: the owner's own GOG WADs, put on our boards only, never in a kit */
static const char *const wads[3] = { "/sdcard/DOOM/doom1.wad", "/sdcard/DOOM/doom.wad", "/sdcard/DOOM/doom2.wad" };
static B82Flag flag;
static void flag_load(void) { uint32_t a = REG_READ(B83_ARGS); flag.td = a; flag.vsync = a >> 8; flag.kasm = a >> 16; flag.pclk = a >> 24; flag.engine = REG_READ(B83_REG) & 1; }
#if defined(B83_DOOM_APP)
int b82_solo_boot(void) { flag_load(); return 1; }   /* the doom partition only runs Doom */
#elif defined(B83_ENG_NONE)
int b82_solo_boot(void) { REG_WRITE(B83_REG, 0); return 0; }   /* Windows image of the Doom-app layout: never the Doom boot */
#else
int b82_solo_boot(void) { if ((REG_READ(B83_REG) >> 16) != B83_MAGIC) return 0; flag_load(); return 1; }   /* one-image layout */
#endif
/* B85 f716d76: esp_restart is a CPU reset; the USB OTG controller keeps polling a pad/keyboard and its DMA lands in internal RAM the
 * bootloader reuses (Illegal instruction on Doom quit, board A c4). Reset it in a shutdown handler before every restart, in both boots. */
#include "hal/usb_utmi_ll.h"
static void usb_off(void) { _usb_utmi_ll_reset_register(); }
void b82_usb_guard(void) { esp_register_shutdown_handler(usb_off); }
int b82_nfb(void) { return flag.vsync & 2 ? 3 : 2; }
int b82_pclk(void) { return flag.pclk ? flag.pclk : 48; }   /* 48 MHz = 41.6 Hz. 60 MHz (old HD port) flashed white on board B (owner 21 Nov): DPI FIFO underrun while the PPA fills the PSRAM bus */
/* lead 21 Nov: every Windows <-> Doom switch is a 200 ms deep sleep (PMU power-down reset: PSRAM re-initialised from power-down, which
 * avoids the P4 soft-reset PSRAM trap, memory reference_p4_softreset_psram_hang); LP_STORE14/15 survive it */
#include "esp_sleep.h"
static void usb_off(void);
void lcd_bl_off(void);
static void b83_switch(void)
{
	fflush(stderr); vTaskDelay(pdMS_TO_TICKS(50));
	usb_off();   /* B82 daa2104: no OTG DMA into internal RAM across the switch */
	lcd_bl_off();   /* B82 4995b0c: backlight low + pad hold against the white flash; deep sleep: the digital pad hold may not last on
	                 * v1.x silicon (SOC caps: single-IO hold in deep sleep from rev 3.0, DIG-399) -> verified by eye/cam, see REPORT */
	esp_sleep_enable_timer_wakeup(200000);
	esp_deep_sleep_start();
}
#ifdef B83_ENG_NONE   /* B84 sound is not in the Windows image; elsewhere strong refs (a weak one does not pull b84snd.o from libmain) */
static void b84_release(void) {} static void b84_bind(void) {} static void b84_volume(int v) { (void)v; } static void b84_loud(int on) { (void)on; } static void b84_capture(int s) { (void)s; }
#define B84_ON 0
#else
#define B84_ON 1
void b84_release(void), b84_bind(void), b84_volume(int v), b84_loud(int on), b84_capture(int secs);
#endif
#ifndef B83_SOUND
#define B83_SOUND 1   /* B84 7046fc1 (fmopl music, board B t7); -DB83_SOUND=0 = every launch -nosound */
#endif
static int solo;
/* B83: the doom partition check reads flash; the shortcut calls go() on a PSRAM stack where a flash read with the cache off asserts
 * (board B s6c: spi_flash_disable_interrupts_caches_and_other_cpu). So Windows checks once at boot (app_main, internal stack). */
static int doom_app_ok = -1;
static void b83_check_doom_app(void)
{
	const esp_partition_t *dp = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL); esp_app_desc_t ad;
	doom_app_ok = dp && esp_ota_get_partition_description(dp, &ad) == ESP_OK;
	if (doom_app_ok) fprintf(stderr, "[b83] Doom app %s %s in the doom partition\r\n", ad.project_name, ad.version);
	else fprintf(stderr, "[b83] no Doom app in the doom partition (icons will show a message)\r\n");
}
static int go(int td, int vsync, int kasm, int pclk, int engine)
{
#ifdef B83_ENG_NONE   /* the Doom app lives in the doom partition: never restart into an empty or broken one (checked at boot, see b82_give_bss) */
	if (doom_app_ok <= 0) { esp_rom_printf("[b83] no Doom app in the doom partition: staying in Windows\r\n"); return -3; }
#endif
	REG_WRITE(B83_ARGS, (uint32_t)(td & 255) | (uint32_t)(vsync & 255) << 8 | (uint32_t)(kasm & 255) << 16 | (uint32_t)(pclk & 255) << 24);
	REG_WRITE(B83_REG, B83_MAGIC << 16 | (engine & 1));
	esp_rom_printf("[b82] restart into Doom (%s engine, td %d vsync %d asm %d pclk %d)\r\n", engine ? "Rust" : "C", td, vsync, kasm, pclk ? pclk : 48);   /* ROM printf: i386_main stack */
	b83_switch();
	return 0;
}
static void back_to_windows(void) { b85_quit(); if (solo && B84_ON) b84_release(); REG_WRITE(B83_REG, 0); b83_switch(); }   /* B84: codec fade + PA off first */

/* ---- called by the B76 resident (Rust) on the Windows side */
int b82_open(int arg)   /* B83: bits 0-7 WAD, bit 8 engine (0 C, 1 Rust: the .dr1/.dru/.dr2 icons) */
{
	int wad = arg & 255, engine = (arg >> 8) & 1;
	if (wad < 0 || wad > 2) wad = 0;
	struct stat sb;   /* stat + ROM printf (fix from B85 4e9b873): fopen + vfprintf on i386_main overflowed its 4 KB stack when Doom was reopened after a resume */
	if (stat(wads[wad], &sb)) { esp_rom_printf("[b82] %s missing\r\n", wads[wad]); return -2; }
	return go(wad << 4, 3, 1, 0, engine);   /* play: vsync + 3 buffers */
}
/* the window path of the first B82 design (Doom beside Windows) is not used any more: the Doom boot owns the machine */
void b82_focus(int on) { (void)on; }
void b82_close(void) {}
int b82_take_quit(void) { return 0; }
void b82_dib(uint32_t bmi, uint32_t dib) { (void)bmi; (void)dib; }

/* ================= everything below runs in the Doom boot only ================= */
static uint16_t *stage, *stg[2]; static EXT_RAM_BSS_ATTR uint16_t lut[256];
#include "freertos/queue.h"
static QueueHandle_t q_free, q_ready;   /* staging slots: engine (core 0) converts into one while core 1 scales + rotates the other */
static uint32_t nfr, conv_us, ppa_us, wait_us, copy_us, underruns; static int64_t stat_t0;
/* smoothness (owner 21 Nov): frame-to-frame present times of each 5 s window -> p50 / p99 / max in the [b82] fps line */
#define FTN 512
static EXT_RAM_BSS_ATTR uint16_t ft[FTN]; static int nft; static int64_t last_show;
static int cmpu16(const void *a, const void *b) { return (int)*(const uint16_t *)a - (int)*(const uint16_t *)b; }
static void ft_add(int64_t now) { if (last_show && nft < FTN) { int64_t d = (now - last_show) / 100; ft[nft++] = (uint16_t)(d > 65535 ? 65535 : d); } last_show = now; }   /* 0.1 ms units */
static void ft_report(float *p50, float *p99, float *mx) { *p50 = *p99 = *mx = 0; if (!nft) return; qsort(ft, nft, 2, cmpu16); *p50 = ft[nft / 2] / 10.f; *p99 = ft[(nft * 99) / 100] / 10.f; *mx = ft[nft - 1] / 10.f; nft = 0; }
__attribute__((weak)) uint32_t dg_render_us;   /* HD tree r_main.c sets it; the 320 tree has none */
/* B83: B85's engine names (b83api.h) per engine; b83_api follows the chosen engine (fallback: dummy storage + no-op calls, so B85 code that
 * runs before the engine is picked or in the Windows image never dereferences a null table) */
#define B83_API_IMPL
#include "b83api.h"
#include "m_config.h"
#include "m_controls.h"
#include "p_saveg.h"
extern int messageToPrint;
static boolean d_bool; static int d_int[16]; static gamestate_t d_gs; static char *d_dir; static player_t d_pl[MAXPLAYERS];
static void d_post(event_t *e) { (void)e; } static void d_void(void) {} static void d_save(int a, char *b) { (void)a; (void)b; }
static void d_load(char *a) { (void)a; } static int d_num(char *a) { (void)a; return -1; } static void *d_cache(int a, int b) { (void)a; (void)b; return NULL; }
static int d_len(unsigned int a) { (void)a; return 0; } static void *d_zm(int a, int b, void *u) { (void)a; (void)b; (void)u; return NULL; } static void d_new(skill_t a, int b, int c) { (void)a; (void)b; (void)c; } static char *d_file(int a) { (void)a; return ""; }
static const B83Api api_dummy = { &d_bool, &d_int[0], d_pl, &d_int[1], &d_gs, &d_dir, &d_int[2], &d_int[3], &d_int[4], &d_int[5], &d_int[6], &d_int[7], &d_int[8],
	&d_int[9], &d_int[10], &d_int[11], &d_int[12], &d_int[13], &d_int[14], &d_int[15], &d_int[11], &d_bool, &d_bool, d_post, d_void, d_save, d_load, d_new, d_file, d_num, d_num, d_cache, d_len, d_zm };
const B83Api *b83_api = &api_dummy;
#ifdef B83_ENG_C
extern boolean timingdemo;   /* g_game.c (no header declares it) */
static const B83Api api_c = { &menuactive, &messageToPrint, players, &consoleplayer, &gamestate, &savegamedir, &joybfire, &joybuse, &joybspeed, &joybstrafe, &joybprevweapon, &joybnextweapon, &joybmenu, &mousebfire, &mousebuse, &mousebstrafe, &mousebforward, &gameepisode, &gamemap, &leveltime, &gametic, &timingdemo, &demoplayback, D_PostEvent, M_SaveDefaults, G_SaveGame, G_LoadGame, G_DeferedInitNew, P_SaveGameFile, W_CheckNumForName, W_GetNumForName, W_CacheLumpNum, W_LumpLength, Z_Malloc };
#endif
#ifdef B83_ENG_RS
extern boolean rs_menuactive; extern int rs_messageToPrint; extern player_t rs_players[MAXPLAYERS]; extern int rs_consoleplayer; extern gamestate_t rs_gamestate;
extern char *rs_savegamedir; extern int rs_joybfire, rs_joybuse, rs_joybspeed, rs_joybstrafe, rs_joybprevweapon, rs_joybnextweapon, rs_joybmenu;
extern int rs_mousebfire, rs_mousebuse, rs_mousebstrafe, rs_mousebforward, rs_gameepisode, rs_gamemap, rs_leveltime, rs_gametic; extern boolean rs_timingdemo, rs_demoplayback;
void rs_D_PostEvent(event_t *); void rs_M_SaveDefaults(void); void rs_G_SaveGame(int, char *); void rs_G_LoadGame(char *);
void rs_G_DeferedInitNew(skill_t, int, int); char *rs_P_SaveGameFile(int);
int rs_W_CheckNumForName(char *); int rs_W_GetNumForName(char *); void *rs_W_CacheLumpNum(int, int); int rs_W_LumpLength(unsigned int);
void *rs_Z_Malloc(int, int, void *);
static const B83Api api_rs = { &rs_menuactive, &rs_messageToPrint, rs_players, &rs_consoleplayer, &rs_gamestate, &rs_savegamedir, &rs_joybfire, &rs_joybuse, &rs_joybspeed, &rs_joybstrafe, &rs_joybprevweapon, &rs_joybnextweapon, &rs_joybmenu, &rs_mousebfire, &rs_mousebuse, &rs_mousebstrafe, &rs_mousebforward, &rs_gameepisode, &rs_gamemap, &rs_leveltime, &rs_gametic, &rs_timingdemo, &rs_demoplayback, rs_D_PostEvent, rs_M_SaveDefaults, rs_G_SaveGame, rs_G_LoadGame, rs_G_DeferedInitNew, rs_P_SaveGameFile, rs_W_CheckNumForName, rs_W_GetNumForName, rs_W_CacheLumpNum, rs_W_LumpLength, rs_Z_Malloc };
#endif
/* B83 engines: the C one (doomp4 C sources, plain names) and/or the Rust one (libdoomrs_p.a, every symbol rs_*); the Doom app holds both
 * and takes the one the launch asked for (flag.engine), else the other one */
typedef struct { const char *name; void (*create)(int, char **); void (*tick)(void); void (*data_init)(void), (*info_init)(void), (*sounds_init)(void), (*config_init)(void);
	pixel_t **sb; boolean *palchg; struct color *cols; uint32_t *render_us; int *par, *peak, *split; const B83Api *api; } Eng;
#ifdef B83_ENG_C
void doomgeneric_Create(int, char **), doomgeneric_Tick(void), b82_data_init(void), b82_info_init(void), b82_sounds_init(void), b82_config_init(void);
#endif
#ifdef B83_ENG_RS
void rs_doomgeneric_Create(int, char **), rs_doomgeneric_Tick(void), rs_b82_data_init(void), rs_b82_info_init(void), rs_b82_sounds_init(void), rs_b82_config_init(void);
extern pixel_t *rs_DG_ScreenBuffer; extern boolean rs_palette_changed; extern struct color rs_colors[256]; extern uint32_t rs_dg_render_us;
extern int rs_b83_par, rs_b83_par_peak, rs_b83_par_split;
#endif
static const Eng engines[2] = {
#ifdef B83_ENG_C
	{ "C", doomgeneric_Create, doomgeneric_Tick, b82_data_init, b82_info_init, b82_sounds_init, b82_config_init, &DG_ScreenBuffer, &palette_changed, colors, &dg_render_us, NULL, NULL, NULL, &api_c },
#else
	{ NULL },
#endif
#ifdef B83_ENG_RS
	{ "Rust", rs_doomgeneric_Create, rs_doomgeneric_Tick, rs_b82_data_init, rs_b82_info_init, rs_b82_sounds_init, rs_b82_config_init, &rs_DG_ScreenBuffer, &rs_palette_changed, rs_colors, &rs_dg_render_us, &rs_b83_par, &rs_b83_par_peak, &rs_b83_par_split, &api_rs },
#else
	{ NULL },
#endif
};
static const Eng *E = &engines[0];

static int msync_whole;
static void msync(const void *p, size_t n, int fl)
{
	/* 32 KB pieces: one esp_cache_msync is one critical section; a 512 KB one per frame could hold off the scan-out DMA restart ISR */
	uintptr_t a = (uintptr_t)p & ~(uintptr_t)127, e = ((uintptr_t)p + n + 127) & ~(uintptr_t)127;
	if (msync_whole) { esp_cache_msync((void *)a, e - a, fl); return; }   /* ablation "mw" (lead 21 Nov: measure what the pieces cost) */
	for (; a < e; a += 32768) esp_cache_msync((void *)a, (e - a) < 32768 ? e - a : 32768, fl);
}

uint8_t *b82_zone(int *size)
{
	/* the whole machine is ours: up to 16 MB zone (the old ports' size) from the PSRAM heap, 1 MB left for USB/SD/stdio */
	size_t big = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
	size_t n = big > 17u * 1024 * 1024 ? 16u * 1024 * 1024 : big - 1024 * 1024;
	uint8_t *z = heap_caps_malloc(n, MALLOC_CAP_SPIRAM);
	*size = z ? (int)n : 0;
	fprintf(stderr, "[b82] zone %u B, PSRAM free after %u, internal free %u\r\n", (unsigned)n, (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM), (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
	return z;
}

/* ---- keys: a ring the engine drains in DG_GetKey (producers: USB task, touch task, console) */
static EXT_RAM_BSS_ATTR volatile uint16_t kr[64]; static volatile uint8_t kw_, kr_;
static void post(int down, int k) { if ((uint8_t)(kw_ - kr_) < 63) { kr[kw_ & 63] = (uint16_t)(k | (down ? 0x100 : 0)); kw_++; if (down) b85_key_mark(); } }
void b82_post(int down, int k) { post(down, k); }
int b85_solo(void) { return solo; }

/* Linux key codes (usb_input.c) -> doom keys. Windows boot: 0 = not ours, the guest gets it */
int b82_key(int down, int lc)
{
	if (!solo) return 0;
	if (b85_key(down, lc)) return 1;
	static const struct { uint8_t lc, k; } m[] = {
		{1, KEY_ESCAPE}, {28, KEY_ENTER}, {96, KEY_ENTER}, {57, KEY_USE}, {15, KEY_TAB}, {14, KEY_BACKSPACE},
		{103, KEY_UPARROW}, {108, KEY_DOWNARROW}, {105, KEY_LEFTARROW}, {106, KEY_RIGHTARROW},
		{29, KEY_FIRE}, {97, KEY_FIRE}, {42, KEY_RSHIFT}, {54, KEY_RSHIFT}, {56, KEY_RALT}, {100, KEY_RALT}, {12, KEY_MINUS}, {13, KEY_EQUALS},
		{59, KEY_F1}, {60, KEY_F2}, {61, KEY_F3}, {62, KEY_F4}, {63, KEY_F5}, {64, KEY_F6}, {65, KEY_F7}, {66, KEY_F8},
		{67, KEY_F9}, {68, KEY_F10}, {87, KEY_F11}, {88, KEY_F12}, {119, KEY_PAUSE},
	};
	for (unsigned i = 0; i < sizeof m / sizeof m[0]; i++) if (m[i].lc == lc) { post(down, m[i].k); return 1; }
	static const char *r1 = "1234567890", *r2 = "qwertyuiop", *r3 = "asdfghjkl", *r4 = "zxcvbnm";
	if (lc >= 2 && lc <= 11) post(down, r1[lc - 2]);
	else if (lc >= 16 && lc <= 25) post(down, r2[lc - 16]);
	else if (lc >= 30 && lc <= 38) post(down, r3[lc - 30]);
	else if (lc >= 44 && lc <= 50) post(down, r4[lc - 44]);
	return 1;
}

/* touch: b85.c (visible overlay, analog stick, drag to turn) */

/* ---- doomgeneric platform layer */
void DG_Init(void) {}
void DG_SetWindowTitle(const char *t) { (void)t; }
uint32_t DG_GetTicksMs(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
void DG_SleepMs(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms ? ms : 1)); }
int DG_GetKey(int *pressed, unsigned char *key)
{
	if (kr_ == kw_) return 0;
	uint16_t v = kr[kr_ & 63]; kr_++;
	*pressed = v >> 8; *key = (unsigned char)v; return 1;
}
/* doom_joy_get (i_input.c, once per tic): b85.c posts the pad / mouse / touch events */

void b82_conv_asm(const uint8_t *src, uint16_t *dst, const uint16_t *lut, int n);   /* b82_pie.S */
static void b82_conv_c(const uint8_t *src, uint16_t *dst, const uint16_t *lut, int n)   /* oracle */
{
	const uint32_t *s = (const uint32_t *)src; uint32_t *d = (uint32_t *)dst;
	for (int i = 0; i < n / 4; i++) { uint32_t v = s[i]; d[2 * i] = lut[v & 255] | (uint32_t)lut[(v >> 8) & 255] << 16; d[2 * i + 1] = lut[(v >> 16) & 255] | (uint32_t)lut[v >> 24] << 16; }
}
static int kasm = 1, vsync = 1, tb, tb_last, pipe, tic_gate = 1; static uint32_t tb_q[3];
static uint8_t *snap[2]; static uint16_t *slut[2];   /* pipe mode: 8-bit frame + its palette, handed from core 0 to core 1 */
bool ppa_rotate_90cw_scaled_async(const void *src, size_t src_w, size_t src_h, size_t src_stride, void *dst, size_t dst_pic_w, size_t dst_pic_h, size_t dst_size, size_t dst_off_x, size_t dst_off_y, float scale_x, float scale_y);
bool ppa_scaled_wait_done(uint32_t timeout_ms);
static TaskHandle_t par_task;   /* B83: core-1 worker (Rust engine builds): replays half of the 3D view, converts half of the frame */
void b83_par_run(void (*f)(int));
static int checked;
static const uint8_t *cv_src; static uint16_t *cv_dst; static const uint16_t *cv_lut; static int cv_rows = H;
static void conv_half(int h)   /* B83: rows 0..199 on core 0, 200..399 on core 1 */
{
	size_t n = (size_t)W * cv_rows / 2;
	if (kasm) b82_conv_asm(cv_src + h * n, cv_dst + h * n, cv_lut, n); else b82_conv_c(cv_src + h * n, cv_dst + h * n, cv_lut, n);
}
static void conv(const uint8_t *src, uint16_t *dst, const uint16_t *lut)
{
	if (checked && par_task && (cv_rows & 1) == 0 && !(flag.vsync & 16)) { cv_src = src; cv_dst = dst; cv_lut = lut; b83_par_run(conv_half); return; }
	if (!checked) {   /* once: the asm kernel must equal the oracle on a real frame (first 4096 px) */
		uint16_t *c = heap_caps_malloc(8192, MALLOC_CAP_SPIRAM); checked = 1;
		b82_conv_asm(src, dst, lut, 4096); if (c) b82_conv_c(src, c, lut, 4096);
		int bad = c ? 0 : -1; for (int i = 0; c && i < 4096; i++) bad += dst[i] != c[i];
		heap_caps_free(c);
		fprintf(stderr, "[b82] conv self-test: asm vs C differ %d of 4096\r\n", bad); if (bad) kasm = 0;
	}
	if (kasm) b82_conv_asm(src, dst, lut, W * cv_rows); else b82_conv_c(src, dst, lut, W * cv_rows);
}
/* B83 status-bar skip (owner "optimize to the max": fewer bytes on the PSRAM bus): when the bottom bar rows of the 8-bit frame have not
 * changed for 3 frames (no palette change, no B85 touch overlay drawn), both staging slots and all (2-3) panel buffers already hold that
 * bar, so only the view rows are converted and scaled+rotated. Console/launch "nobarskip" = full frames (A/B). */
#define BAR_Y (H - 64)
static EXT_RAM_BSS_ATTR uint8_t prevbar[W * 64]; static int bar_same; static int slot_rows[2] = { H, H }; static uint32_t bar_skips;
extern volatile int b85_ov_drawn;
static int frame_rows(const uint8_t *fb, int palchg)
{
	if (!(flag.vsync & 32)) return H;   /* opt-in ("barskip"): board B s9 it never engaged (barskip 0) and its compare cost ~1 fps */
	if (palchg || b85_ov_drawn || memcmp(fb + W * BAR_Y, prevbar, sizeof prevbar)) { memcpy(prevbar, fb + W * BAR_Y, sizeof prevbar); bar_same = 0; return H; }
	if (++bar_same < 3) return H;
	bar_skips++; return BAR_Y;
}

void DG_DrawFrame(void)
{
	/* present only frames that belong to a new game tic (Doom has 35 tics/s): on cheap screens the loop drew ~39 identical frames/s,
	 * pure PSRAM traffic that beat against the 41.6 Hz scan-out into white flashes (owner video, B85 analysis). Timedemos are not gated. */
	{ static int last_tic = -1; if (tic_gate && !*E->api->timingdemo && *E->api->gametic == last_tic) { vTaskDelay(1); return; } last_tic = *E->api->gametic; }
	{ extern volatile int lcd_shot_hold; while (lcd_shot_hold) vTaskDelay(1); }   /* card service "shot panel" reads the front buffer */
	int pchg = *E->palchg;
	if (*E->palchg) {
		*E->palchg = false;
		for (int i = 0; i < 256; i++) lut[i] = (uint16_t)(((E->cols[i].r & 0xF8) << 8) | ((E->cols[i].g & 0xFC) << 3) | (E->cols[i].b >> 3));
	}
	int idx; xQueueReceive(q_free, &idx, portMAX_DELAY);
	b85_frame_built(idx);
	int64_t t0 = esp_timer_get_time();
	if (pipe) { memcpy(snap[idx], *E->sb, W * H); memcpy(slut[idx], lut, sizeof lut); copy_us += esp_timer_get_time() - t0; }   /* core 1 converts it */
	else { stage = stg[idx]; cv_rows = slot_rows[idx] = frame_rows(*E->sb, pchg); conv(*E->sb, stage, lut); b85_overlay(stage, W, H); msync(stage, (size_t)W * cv_rows * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M); conv_us += esp_timer_get_time() - t0; }
	xQueueSend(q_ready, &idx, portMAX_DELAY);   /* core 1 presents it while the engine runs the next tic */
}

/* B83: console "doom ppat" -> next present measures the PPA: scale 2 + rotate 90 (640x400 landscape, today) vs scale 2 only (400x640
 * portrait = a transposed frame), 10 jobs each into the back buffer (one frame of garbage on the panel) */
bool b83_ppa_scale(const void *src_pic, size_t pic_w, size_t pic_h, void *dst, size_t dst_w, size_t dst_h, size_t dst_size, float scale);
static volatile int ppat_req;
static void ppa_bench(const void *src)
{
	ppat_req = 0; void *dst = lcd_back_fb(); int64_t t0 = esp_timer_get_time();
	for (int i = 0; i < 10; i++) b29_ppa_block_rot(src, W, H, 0, 0, W, H, dst, PW, PH, PW * PH * 2, (float)PH / W);
	int64_t t1 = esp_timer_get_time();
	for (int i = 0; i < 10; i++) b83_ppa_scale(src, H, W, dst, PW, PH, PW * PH * 2, (float)PW / H);
	int64_t t2 = esp_timer_get_time();
	fprintf(stderr, "[b83] ppat rot90+scale %lld us, scale only %lld us (per job, pclk %d)\r\n", (t1 - t0) / 10, (t2 - t1) / 10, b82_pclk());
}

/* core 1: wait for a free panel buffer, PPA scale + rotate 90 into it, flip. Overlaps the engine's next frame (old HD port: 34-37 fps) */
static void present_task(void *a)
{
	(void)a;
	int j = 0, pend = -1; int64_t pend_in = 0;   /* pipe: stage slot for this frame, panel buffer the running PPA job fills (-1 none), its input time */
	for (;;) {
		int idx; xQueueReceive(q_ready, &idx, portMAX_DELAY);
		if (ppat_req) ppa_bench(stg[idx]);
		int64_t tin = b85_take_in(idx);
		if (pipe) {   /* convert frame N while the PPA still scales + rotates frame N-1 (async), then hand N to the PPA */
			int64_t c0 = esp_timer_get_time();
			conv(snap[idx], stg[j], slut[idx]); b85_overlay(stg[j], W, H); msync(stg[j], W * H * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
			xQueueSend(q_free, &idx, portMAX_DELAY);
			int64_t c1 = esp_timer_get_time(); conv_us += c1 - c0;
			if (pend >= 0) { ppa_scaled_wait_done(200); if (tb) { lcd_show(pend); tb_q[pend] = lcd_refresh_n; } else lcd_swap_present(); b85_frame_shown(pend_in); }
			int64_t c2 = esp_timer_get_time(); ppa_us += c2 - c1;   /* = PPA time not hidden behind the convert */
			void *dst;
			if (tb) { int t = (tb_last + 1) % 3; if (vsync) lcd_wait_refresh_after(tb_q[(t + 1) % 3]);   /* sleep on the refresh-done semaphore (a vTaskDelay(0) spin at prio 6 starved core 1, B83) */ dst = lcd_fbn(t); tb_last = t; pend = t; }
			else { if (vsync) lcd_wait_flip(); dst = lcd_back_fb(); pend = 0; }
			int64_t c3 = esp_timer_get_time(); wait_us += c3 - c2;
			if (!ppa_rotate_90cw_scaled_async(stg[j], W, H, W, dst, PW, PH, PW * PH * 2, 0, 0, (float)PH / W, (float)PH / W)) pend = -1;
			pend_in = tin;
			j ^= 1; nfr++;
			if (c3 - stat_t0 > 5000000) {
				if (stat_t0) fprintf(stderr, "[b82] fps %.1f pipe copy %lu us conv %lu us ppa-wait %lu us flip-wait %lu us render %lu us\r\n", nfr * 1e6 / (double)(c3 - stat_t0),
					(unsigned long)(copy_us / nfr), (unsigned long)(conv_us / nfr), (unsigned long)(ppa_us / nfr), (unsigned long)(wait_us / nfr), (unsigned long)*E->render_us);
				stat_t0 = c3; nfr = conv_us = ppa_us = wait_us = copy_us = 0;
			}
			continue;
		}
		int64_t t1 = esp_timer_get_time();
		void *dst;
		if (tb) {   /* 3 buffers (FOR_DOOM: 44 vs 26.6 fps): the target was on screen 2 swaps ago; free once the swap after it took effect */
			int t = (tb_last + 1) % 3;
			if (vsync) lcd_wait_refresh_after(tb_q[(t + 1) % 3]);   /* sleep on the refresh-done semaphore (a vTaskDelay(0) spin at prio 6 starved core 1, B83) */
			dst = lcd_fbn(t); tb_last = t;
		} else { if (vsync) lcd_wait_flip(); dst = lcd_back_fb(); }   /* 2 buffers: the previous flip took effect */
		int64_t t2 = esp_timer_get_time();
		b29_ppa_block_rot(stg[idx], W, H, 0, 0, W, slot_rows[idx], dst, PW, PH, PW * PH * 2, (float)PH / W);   /* B83: view rows only when the bar is unchanged */
		if (tb) { lcd_show(tb_last); tb_q[tb_last] = lcd_refresh_n; } else lcd_swap_present();
		b85_frame_shown(tin);
		xQueueSend(q_free, &idx, portMAX_DELAY);
		int64_t t3 = esp_timer_get_time();
		wait_us += t2 - t1; ppa_us += t3 - t2; nfr++; ft_add(t3);
		if (MIPI_DSI_BRIDGE.int_raw.underrun_int_raw) { underruns++; MIPI_DSI_BRIDGE.int_clr.underrun_int_clr = 1; }   /* DPI FIFO ran dry this frame = the white flash */
		if (t3 - stat_t0 > 5000000) {
			float p50, p99, mx; ft_report(&p50, &p99, &mx);
			if (stat_t0) fprintf(stderr, "[b82] fps %.1f conv %lu us wait %lu us ppa %lu us render %lu us frametime p50 %.1f p99 %.1f max %.1f ms underruns %lu gs %d demo %d gametic %d\r\n", nfr * 1e6 / (double)(t3 - stat_t0),
				(unsigned long)(conv_us / nfr), (unsigned long)(wait_us / nfr), (unsigned long)(ppa_us / nfr), (unsigned long)*E->render_us, p50, p99, mx, (unsigned long)underruns, (int)*E->api->gamestate, (int)*E->api->demoplayback, *E->api->gametic); underruns = 0;   /* gs/demo: attract state (owner 21 Nov: board B attract looked frozen) */
			if (stat_t0 && E->peak) fprintf(stderr, "[b83] par %d split %d list peak %d barskip %lu\r\n", *E->par, *E->split, *E->peak, (unsigned long)bar_skips);
			stat_t0 = t3; nfr = conv_us = ppa_us = wait_us = 0;
		}
	}
}

/* ---- B83: dual-core 3D view (Rust engine, rust/doomrs/own/par.rs): core 1 replays its half of the recorded column/span list */
static TaskHandle_t par_caller; static void (*volatile par_fn)(int);
static void par_worker(void *a)
{
	(void)a;
	for (;;) { ulTaskNotifyTake(pdTRUE, portMAX_DELAY); par_fn(1); xTaskNotifyGive(par_caller); }
}
void b83_par_run(void (*f)(int))
{
	if (!par_task) { f(0); f(1); return; }
	par_fn = f; par_caller = xTaskGetCurrentTaskHandle();
	xTaskNotifyGive(par_task);
	f(0);
	ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}

/* ---- exit paths (i_system.c hooks): Windows comes back */
void b82_quit(void) { fprintf(stderr, "[b82] quit -> Windows\r\n"); back_to_windows(); }
void b82_error(const char *m) { fprintf(stderr, "[b82] I_Error: %s -> Windows\r\n", m); back_to_windows(); }

static void task(void *a)
{
	(void)a;
	static char a0[] = "doom", a1[] = "-iwad", a3[] = "-timedemo", d[] = "demo1", ns[] = "-nosound";
	char *argv[6] = { a0, a1, (char *)wads[((flag.td >> 4) & 3) % 3], a3, d }; int argc = 3;
	if (flag.td & 15) { d[4] = (char)('0' + (flag.td & 15)); argc = 5; }
	if (!B83_SOUND || (flag.td & 64)) argv[argc++] = ns;   /* B83: -nosound until B84 sends its board B t2 fix (OPL step starved core 1) */
	if (B84_ON && B83_SOUND) b84_bind();   /* B83: the sound module before the engine reads it (B84 never called it) */
	E->data_init(); E->info_init(); E->sounds_init(); E->config_init();
	{ if (B84_ON) { extern int b84_loud_boot; b84_loud_boot = (flag.td & 128) != 0; if (b84_loud_boot) fprintf(stderr, "[b84] worst-case loudness ON from the start (limiter pinned at -3 dBFS)\r\n"); } }   /* engine globals: flash -> PSRAM (components/doomp4/hooks) */
	b85_engine_init(argv[2]);
	fprintf(stderr, "[b82] engine start%s%s\r\n", argc == 5 ? " -timedemo " : "", argc == 5 ? d : "");
	E->create(argc, argv);
	fprintf(stderr, "[b82] engine up\r\n");
	for (;;) E->tick();
}

void b82_solo_main(void)
{
	/* owner 21 Nov: no white flashes. The flash = DPI underrun: the scan-out DMA (DW-GDMA) starves while the PPA (DMA2D) and the CPU fill the
	 * PSRAM bus. AXI arbiter QoS (all 0 after reset): scan-out reads first, CPU/cache next, the PPA last. Ablation: doom go ... noqos */
	if (!(flag.kasm & 2)) {
		axi_icm_ll_set_dw_gdma_qos_arbiter_prio(0, 15, 15); axi_icm_ll_set_dw_gdma_qos_arbiter_prio(1, 15, 15);
		axi_icm_ll_set_cache_qos_arbiter_prio(8, 8); axi_icm_ll_set_cpu_qos_arbiter_prio(8, 8);
		{ int q = ((flag.kasm >> 5) & 3) * 4; axi_icm_ll_set_dma2d_qos_arbiter_prio(q, q); }   /* ablation pq4/pq8 (default 0) */
	}
	MIPI_DSI_BRIDGE.int_clr.underrun_int_clr = 1;
	tic_gate = !(flag.kasm & 4);
	{ extern int lcd_full_flip; lcd_full_flip = (flag.kasm >> 3) & 1; } msync_whole = (flag.kasm >> 4) & 1;
	solo = 1; vsync = flag.vsync & 1; tb = (flag.vsync & 2) != 0; pipe = (flag.vsync & 4) != 0; kasm = flag.kasm & 1;
	int td = flag.td;
	REG_WRITE(B83_REG, 0);   /* a crash or a reset in Doom must not loop: the next boot is Windows */
	E = &engines[flag.engine & 1]; if (!E->create) E = &engines[!(flag.engine & 1)];
	if (!E->create) { fprintf(stderr, "[b83] no engine in this image\r\n"); back_to_windows(); }
	b83_api = E->api;   /* B85 input/saves/settings -> this engine */
	fprintf(stderr, "[b83] engine %s (asked %s)\r\n", E->name, flag.engine & 1 ? "Rust" : "C");
	while (!lcd_panel_ready()) vTaskDelay(pdMS_TO_TICKS(10));
	for (int i = 0; i < 2; i++) stg[i] = heap_caps_aligned_alloc(128, W * H * 2, MALLOC_CAP_SPIRAM);
	for (int i = 0; i < 2; i++) { snap[i] = heap_caps_aligned_alloc(128, W * H, MALLOC_CAP_SPIRAM); slut[i] = heap_caps_malloc(512, MALLOC_CAP_SPIRAM); }
	stage = stg[0]; q_free = xQueueCreate(2, sizeof(int)); q_ready = xQueueCreate(2, sizeof(int));
	for (int i = 0; i < 2; i++) xQueueSend(q_free, &i, 0);
	xTaskCreatePinnedToCore(present_task, "b82_present", 4096, NULL, 6, NULL, 1);
	if (!stg[0] || !stg[1] || !snap[0] || !snap[1] || !slut[0] || !slut[1]) { fprintf(stderr, "[b82] no PSRAM for the stage\r\n"); back_to_windows(); }
	for (int i = 0; i < b82_nfb(); i++) msync(lcd_fbn(i), PW * PH * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_INVALIDATE);
	{	/* owner 21 Nov: one image, the kernel path is picked per chip revision. The 8->565 convert is a per-pixel LUT gather: PIE has no gather
		 * load on any P4 revision (v1.x or v3.x), so every revision runs the scalar RV32 asm kernel; the C oracle check below decides asm vs C. */
		unsigned rev = efuse_hal_chip_revision();
		fprintf(stderr, "[b82] chip v%u.%u: conv kernel = RV32 asm (PIE not applicable: no gather), C oracle check at the first frame\r\n", rev / 100, rev % 100);
	}
	fprintf(stderr, "[b82] Doom boot: %s td %d vsync %d fbs %d pipe %d asm %d qos %d ppaq %d ff %d mw %d ticgate %d pclk %d, PSRAM free %u, internal free %u\r\n", wads[((td >> 4) & 3) % 3], td & 15, vsync, b82_nfb(), pipe, kasm, !(flag.kasm & 2), ((flag.kasm >> 5) & 3) * 4, (flag.kasm >> 3) & 1, msync_whole, tic_gate, b82_pclk(),
		(unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM), (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
	/* internal stack (transient: this boot has no emulator); BSP recursion + the renderer's locals */
	size_t big = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT), st = big > 57344 ? 49152 : big > 28672 ? (big - 4096) & ~1023 : 0;
	if (st) { void *pr = heap_caps_malloc(st, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); if (!pr || ((uintptr_t)pr >> 20) == 0x501) st = 0; heap_caps_free(pr); }   /* B83: never the LP/RTC SRAM block (in the internal heap, ~38x slower, B82 "doom where") -> PSRAM then */   /* B83: merged Doom app had 31.7 KB largest -> stack fell to PSRAM (slower render); 27 KB internal is enough (pre-merge ran on 26 KB) */
	BaseType_t ok = st ? xTaskCreatePinnedToCore(task, "doom", st, NULL, 5, NULL, 0)
	                   : xTaskCreatePinnedToCoreWithCaps(task, "doom", 65536, NULL, 5, NULL, 0, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	fprintf(stderr, "[b82] engine stack %u B %s (internal largest %u)\r\n", st ? (unsigned)st : 65536u, st ? "internal" : "PSRAM", (unsigned)big);
	if (ok != pdPASS) { fprintf(stderr, "[b82] no stack\r\n"); back_to_windows(); }
	if (E->par) {   /* Rust engine: core-1 worker (replay + convert halves), after the engine stack took the largest internal block
		 * (board B s2: created first, it pushed the engine stack to PSRAM); its own 4 KB stack falls back to PSRAM */
		*E->par = !(flag.vsync & 8);   /* bit 3: nopar (bit 2 = B82 pipe) */
		if (*E->par && xTaskCreatePinnedToCore(par_worker, "b83_par", 4096, NULL, 5, &par_task, 1) != pdPASS)
			xTaskCreatePinnedToCoreWithCaps(par_worker, "b83_par", 4096, NULL, 5, &par_task, 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
		fprintf(stderr, "[b83] Rust engine, dual-core view %s, worker %s\r\n", *E->par ? "on" : "off", par_task ? "up" : "none");
	}
}

/* console. Windows boot: "doom go [demo1..3] [nov] [c] [pclk60]" = restart into Doom (benchmark launcher; nov = no vsync wait,
 * c = C kernel instead of asm). Doom boot: "doom" (state), "doom quit", "doomkey <keys>" ('<' '>' '^' 'v' arrows, '!' fire, ' ' use,
 * '|' enter, '~' esc, letters as is) */
static void key_task(void *a)
{
	for (const char *c = (const char *)a; *c; c++) {
		int k = *c == '<' ? KEY_LEFTARROW : *c == '>' ? KEY_RIGHTARROW : *c == '^' ? KEY_UPARROW : *c == 'v' ? KEY_DOWNARROW : *c == '!' ? KEY_FIRE :
			*c == ' ' ? KEY_USE : *c == '|' ? KEY_ENTER : *c == '~' ? KEY_ESCAPE : *c;
		post(1, k); vTaskDelay(pdMS_TO_TICKS(120)); post(0, k); vTaskDelay(pdMS_TO_TICKS(250));
	}
	vTaskDelete(NULL);
}
void b82_console(const char *l)
{
	static EXT_RAM_BSS_ATTR char kb[64];
	if (b85_console(l)) return;
	if (!strncmp(l, "doomkey ", 8)) { if (solo) { strncpy(kb, l + 8, sizeof kb - 1); xTaskCreatePinnedToCore(key_task, "doomkey", 3072, kb, 2, NULL, 1); } return; }
	if (!strncmp(l, "doom go", 7) && !solo) {
		char b[64]; int td = 0, vs = 3, ka = 1, pc = 0, en = 1; strncpy(b, l + 7, sizeof b - 1); b[sizeof b - 1] = 0;
		for (char *t = strtok(b, " "); t; t = strtok(NULL, " ")) {
			if (!strncmp(t, "demo", 4)) td |= t[4] - '0';
			else if (!strncmp(t, "wad", 3)) td |= (t[3] - '0') << 4;   /* wad0 shareware, wad1 Ultimate, wad2 Doom II */
			else if (!strcmp(t, "nov")) vs &= ~1;
			else if (!strcmp(t, "tb")) vs |= 2;
			else if (!strcmp(t, "fb2")) vs &= ~2;
			else if (!strcmp(t, "pipe")) vs |= 4;   /* convert on core 1 beside an async PPA: board B 21 Nov 33.3 vs 34.7 fps (render 15 -> 25 ms: PSRAM bus) */
			else if (!strcmp(t, "c")) ka &= ~1;
			else if (!strcmp(t, "noqos")) ka |= 2;   /* B82 6a4f0d3: AXI QoS off (A/B) */
			else if (!strcmp(t, "nocap")) ka |= 4;   /* B82: no tic gate (ablation) */
			else if (!strcmp(t, "nopar")) vs |= 8;   /* B83: one-core view (A/B) */
			else if (!strcmp(t, "engc")) en = 0;   /* B83: C engine (default: Rust) */
			else if (!strcmp(t, "noconv2")) vs |= 16;   /* B83: convert on core 0 only (A/B) */
			else if (!strcmp(t, "barskip")) vs |= 32;   /* B83: status-bar skip, opt-in (not engaging yet: debug) */
			else if (!strcmp(t, "nosnd")) td |= 64;   /* B84: fps A/B without sound */
			else if (!strcmp(t, "loud")) td |= 128;   /* B84 safety test: mix boosted 16x from the start = limiter pinned at -3 dBFS */
			else if (!strcmp(t, "ff")) ka |= 8;      /* full-frame flip (old draw_bitmap) */
			else if (!strcmp(t, "mw")) ka |= 16;     /* msync in one piece */
			else if (!strcmp(t, "pq4")) ka |= 32;    /* PPA QoS 4 */
			else if (!strcmp(t, "pq8")) ka |= 64;    /* PPA QoS 8 */
			else if (!strncmp(t, "pclk", 4)) pc = atoi(t + 4);
		}
		if (go(td, vs, ka, pc, en) < 0) fprintf(stderr, "[b83] doom go refused\r\n");
		return;
	}
	if (!strcmp(l, "doom quit") && solo) { back_to_windows(); return; }
	if (!strcmp(l, "doom ppat") && solo) { ppat_req = 1; return; }
	if (!strcmp(l, "doom crash") && solo) { fprintf(stderr, "[b83] test crash (lead fallback check)\r\n"); abort(); }   /* next boot must be Windows */
	if (!strncmp(l, "doomvol ", 8) && solo && B84_ON) { b84_volume(atoi(l + 8)); return; }   /* B84: codec volume 0-70 (default 30) */
	if (!strncmp(l, "doomloud ", 9) && solo && B84_ON) { b84_loud(atoi(l + 9)); return; }
	if (!strncmp(l, "doomcap ", 8) && solo && B84_ON) { b84_capture(atoi(l + 8)); return; }   /* B84: N s of the I2S stream -> /sdcard/DOOM/b84cap.wav */
	if (!strcmp(l, "doom where")) {   /* lead 21 Nov (B84 trap): RTC/LP fast memory is in the INTERNAL heap (ALLOW_RTC_FAST_MEM_AS_HEAP=y) and ~38x slower; where do hot stacks live? */
		static const char *nm[] = { "i386_main", "vga_task", "b82_present", "doom", "b29_present", "b76", "usb_task", "i2s_task", "cardsvc", "usb_boot", "wifi", "IDLE0", "IDLE1" };
		for (unsigned i = 0; i < sizeof nm / sizeof nm[0]; i++) {
			TaskHandle_t h = xTaskGetHandle(nm[i]); if (!h) continue;
			uintptr_t a = (uintptr_t)pxTaskGetStackStart(h);
			esp_rom_printf("[b82] where %s stack %08x %s\r\n", nm[i], (unsigned)a, (a >> 20) == 0x501 ? "LP-SRAM (slow!)" : (a >> 24) == 0x4f ? "HP-L2MEM" : (a >> 24) == 0x48 ? "PSRAM" : "?");
		}
		heap_caps_print_heap_info(MALLOC_CAP_INTERNAL); return;
	}
	if (!strcmp(l, "doom ur")) {   /* DPI underrun latch (either boot): print and clear; Windows has no per-frame counter, so poll it */
		unsigned u = MIPI_DSI_BRIDGE.int_raw.underrun_int_raw; MIPI_DSI_BRIDGE.int_clr.underrun_int_clr = 1;
		esp_rom_printf("[b82] dpi underrun latch %u (cleared)\r\n", u); return;
	}
	fprintf(stderr, "[b82] solo %d vsync %d asm %d pclk %d PSRAM free %u largest %u internal free %u\r\n", solo, vsync, kasm, b82_pclk(),
		(unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM), (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

/* Windows boot: the engine's PSRAM .bss/COMMON (~590 KB, unused here) goes back to the heap at once. Without it the B61 golden save
 * ran short of PSRAM (board B 21 Nov: result=7, next boot cold). The Doom boot never calls this: startup zeroed the ranges for the engine. */
/* B83: C engine ranges b82*, Rust engine ranges b83* (linker_rs.lf); weak: an image without that engine has none */
extern char _b82bss_start[] __attribute__((weak)), _b82bss_end[] __attribute__((weak)), _b82com_start[] __attribute__((weak)), _b82com_end[] __attribute__((weak));
extern char _b83bss_start[] __attribute__((weak)), _b83bss_end[] __attribute__((weak)), _b83com_start[] __attribute__((weak)), _b83com_end[] __attribute__((weak));
void b82_give_bss(void)
{
#ifdef B83_ENG_NONE
	b83_check_doom_app();   /* Windows boot, app_main (internal stack) */
#endif
	uintptr_t r[4][2] = { { (uintptr_t)_b82bss_start, (uintptr_t)_b82bss_end }, { (uintptr_t)_b82com_start, (uintptr_t)_b82com_end },
	                      { (uintptr_t)_b83bss_start, (uintptr_t)_b83bss_end }, { (uintptr_t)_b83com_start, (uintptr_t)_b83com_end } };
	for (int i = 0; i < 4; i++) {
		if (!r[i][0]) continue;
		uintptr_t a = (r[i][0] + 15) & ~(uintptr_t)15, e = r[i][1] & ~(uintptr_t)15;
		if (e > a + 4096) { esp_err_t x = heap_caps_add_region(a, e); fprintf(stderr, "[b82] engine %s %u B -> heap (%d)\r\n", i & 1 ? "common" : "bss", (unsigned)(e - a), (int)x); }
	}
}

/* B83: the Rust engine is translated against oracle/fakelibc (plain declarations): its stdio/errno handles come from here */
#include <errno.h>
FILE *b83_stdout(void) { return stdout; }
FILE *b83_stderr(void) { return stderr; }
int *b83_errno(void) { return &errno; }
void b83_assert_fail(const char *e, const char *f, int l) { fprintf(stderr, "[b83] assert %s %s:%d\r\n", e, f, l); b82_error("assert"); }

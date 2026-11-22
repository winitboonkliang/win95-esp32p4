/* APPS B76: board glue of the native-app contract (lab:APPS/B76/CONTRACT.md). The app is Rust (lib/libb76pad.a);
 * this file gives it guest memory, the GFX95 surface, time, the F12 doorbell and the log. Called on the emulator task only. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "../../pc.h"
#include "../../i8042.h"
#include "../../gfxcard.h"

void b76_trap(uint32_t op_area); void b76_set_font(const uint8_t *f); uint32_t b76_mem_size(void); void b76_init(uint8_t *m);
static PC *g_pc; static long b76_traps; static int64_t b76_us;

static int gmem(uint32_t lin, uint8_t *buf, uint32_t n, int wr);
/* the emulator task (i386_main) has a 4 KB internal stack, so every trap runs in its own core-1 task with a 32 KB PSRAM stack;
 * i386_main waits for it. Not esp_execute_shared_stack_function: it moves the TCB stack end, and the RISC-V port finds the lazy
 * FPU save area from that end, so a presenter preemption stored the FPU registers to 0xa5a5a5a5 and hung core 1 (lead 20 Nov). */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
static SemaphoreHandle_t b76_go, b76_done; static TaskHandle_t b76_task; static uint32_t b76_val; static int b76_state;
#define B76_STACK 32768
static void b76_setup(void)
{
	size_t n = b76_mem_size();
	uint8_t *m = heap_caps_calloc(1, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);   /* PSRAM, never internal RAM */
	uint8_t *f = heap_caps_calloc(1, 4096, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (!m || !f) { printf("[b76] no PSRAM for %u B\n", (unsigned) n); b76_state = -1; return; }
	b76_init(m);
	FILE *ff = fopen("/sdcard/vgabios.bin", "rb");   /* 8x16 glyphs of our VGA BIOS (until the asset-pack font) */
	if (ff) { fseek(ff, 29312, SEEK_SET); fread(f, 1, 4096, ff); fclose(ff); }
	b76_set_font(f); heap_caps_free(f);
	printf("[b76] host app ready: %u B PSRAM, font %s\n", (unsigned) n, ff ? "ok" : "missing");
	b76_state = 1;
}
/* B61 golden: the host registration of the resident stub (Rust Glob in PSRAM) is saved with the snapshot and put back on resume.
 * The restore is DEFERRED to the next trap: b76_snap_io runs inside the snapshot load on i386_main's 4 KB internal stack, and b76_setup
 * (fopen + Rust init) there overran it (BLUE B61g: Instruction access fault MEPC 0 right after resume, 2 of 3 boots, Notepad dead). */
void b76_state_io(uint32_t *v, int wr);
static uint32_t b76_pv[6]; static volatile int b76_pend;
static void b76_run(void)
{
	if (!b76_state) b76_setup();
	if (b76_state > 0 && b76_pend) { b76_state_io(b76_pv, 1); b76_pend = 0; }   /* on the 32 KB PSRAM trap stack */
	if (b76_state > 0) b76_trap(b76_val);
}
static void b76_worker(void *a) { for (;;) { xSemaphoreTake(b76_go, portMAX_DELAY); b76_run(); xSemaphoreGive(b76_done); } }
void b76_snap_io(uint32_t *v, int wr)
{
	if (!wr) { memset(v, 0, 24); if (b76_state > 0) b76_state_io(v, 0); else if (b76_pend) memcpy(v, b76_pv, 24); return; }
	memcpy(b76_pv, v, 24); b76_pend = 1;   /* no allocation, no file I/O, no Rust call here */
}
void b76_out(void *o, u32 val)
{
	g_pc = o;
	if (!b76_task && b76_state >= 0) {
		b76_go = xSemaphoreCreateBinary(); b76_done = xSemaphoreCreateBinary();
		if (!b76_go || !b76_done || xTaskCreatePinnedToCoreWithCaps(b76_worker, "b76", B76_STACK, NULL, 4, &b76_task, 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) b76_state = -1;
	}
	int64_t t0 = esp_timer_get_time();
#ifdef B76_TRACE
	{ uint32_t h[4] = {0}; gmem(0x403000 + (val & ~0xfffu), (uint8_t *) h, 16, 0);   /* key/focus trace only (light) */
	  if ((val & 0xfff) == 2 && (h[1] == 7 || h[1] == 8 || (h[1] >= 0x100 && h[1] <= 0x105) || h[1] == 0x10 || h[1] == 0x2)) printf("[b76t] hwnd=%lx msg=%lx wp=%lx lp=%lx\n", (unsigned long) h[0], (unsigned long) h[1], (unsigned long) h[2], (unsigned long) h[3]); }
#endif
	if (b76_state >= 0) { b76_val = val; xSemaphoreGive(b76_go); xSemaphoreTake(b76_done, portMAX_DELAY); }
	if (b76_state < 0) { uint32_t e[5] = {0xffffffffu, 1, 0, 0, 0}; gmem(0x403000 + (val & ~0xfffu) + 16, (uint8_t *) e, 20, 1); return; }   /* END 0 */
	b76_us += esp_timer_get_time() - t0;
	/* B82/B85 21 Nov: this runs on i386_main (4 KB stack). newlib printf with %lld went deep enough to hit the stack guard while the Run box
	 * was typed (board A, Guru in vfprintf -> uart_write -> lock_acquire); the ROM printf with 32-bit fields is stack-light. */
	if ((++b76_traps & 255) == 1) esp_rom_printf("[b76] traps=%ld host_ms=%lu last op=%lu\n", b76_traps, (unsigned long) (b76_us / 1000), (unsigned long) (val & 0xfff));
}
static int gmem(uint32_t lin, uint8_t *buf, uint32_t n, int wr)
{
	while (n) {
		uint32_t k = 4096 - (lin & 4095); if (k > n) k = n;
		long ph = cpui386_lin2phys(g_pc->cpu, lin, wr);
		if (ph < 0 || ph + k > g_pc->phys_mem_size) return -1;
		if (wr) memcpy(g_pc->phys_mem + ph, buf, k); else memcpy(buf, g_pc->phys_mem + ph, k);
		lin += k; buf += k; n -= k;
	}
	return 0;
}
int b76g_read(uint32_t lin, uint8_t *d, uint32_t n) { return gmem(lin, d, n, 0); }
int b76g_write(uint32_t lin, const uint8_t *s, uint32_t n) { return gmem(lin, (uint8_t *) s, n, 1); }
uint16_t *b76g_fb(int *w, int *h) { *w = gfx.w; *h = gfx.h; return (uint16_t *) g_pc->vga_mem; }
void b76g_dirty(int y, int x0, int x1) { gfx_lfb_dirty((uint32_t)(y * gfx.w + x0) * 2, (x1 - x0) * 2); }
uint32_t b76g_time(void) { return (uint32_t) time(NULL); }
static void kp(int k, int down);
void b76g_doorbell(void) { kp(88, 1); kp(88, 0); }   /* F12 through the same ring */   /* F12 */
void b76g_log(const uint8_t *s, uint32_t n) { printf("[b76] t_us=%lld cyc=%ld %.*s\n", (long long) esp_timer_get_time(), g_pc ? cpui386_get_cycle(g_pc->cpu) : 0L, (int) n, s); }

/* console "b76keys <text>": types through the PS/2 path like a USB keyboard (board test driver). '^' = Ctrl+Esc, '|' = Enter,
 * '~' = Alt+F4; '`' + u/d/l/r = arrows, `e = Esc, `w/`W = 0.8/2.5 s pause (B87); other chars US layout. Logs t_us at every Enter so open time = first paint t_us - Enter t_us. */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static char b76_kbuf[256];
/* i8042 is not thread-safe (no THREAD_SAFE): the key task only fills this ring, the emulator task (b76_poll) feeds the i8042.
 * The board-only "keys stop after the first key" bug (board B mx4/mx5, board A c1) was this race. */
static volatile uint16_t kring[256]; static volatile uint8_t kw, kr;
static void kp(int k, int down) { kring[kw] = (uint16_t)(k | (down << 15)); kw = kw + 1; }
/* B87: the key task waits while the ring is full (it lapped the reader while the emulator was stalled and dropped key-ups: stuck Ctrl/Alt);
 * not in kp() itself: b76g_doorbell calls kp on the emulator task that drains the ring */
static void kpw(int k, int down) { while ((uint8_t)(kw + 1) == kr) vTaskDelay(1); kp(k, down); }
void b76_poll(void *o) { PC *pc = o; while (kr != kw) { uint16_t v = kring[kr]; kr = kr + 1; if (pc->kbd) ps2_put_keycode(pc->kbd, v >> 15, v & 0x7fff); } }
static void tap(int k, int sh) { if (sh) kpw(42, 1); kpw(k, 1); vTaskDelay(pdMS_TO_TICKS(40)); kpw(k, 0); if (sh) kpw(42, 0); vTaskDelay(pdMS_TO_TICKS(60)); }
static int code(char c, int *sh)
{
	static const char *r1 = "1234567890-=", *r2 = "qwertyuiop[]", *r3 = "asdfghjkl;'", *r4 = "zxcvbnm,./", *s3 = ":\"", *s4 = "<>?";
	const char *p; *sh = 0;
	if (c >= 'A' && c <= 'Z') { *sh = 1; c += 32; }
	if ((p = strchr(r1, c))) return 2 + (p - r1);
	if ((p = strchr(r2, c))) return 16 + (p - r2);
	if ((p = strchr(r3, c))) return 30 + (p - r3);
	if ((p = strchr(r4, c))) return 44 + (p - r4);
	if ((p = strchr(s3, c))) { *sh = 1; return 39 + (p - s3); }
	if ((p = strchr(s4, c))) { *sh = 1; return 51 + (p - s4); }
	if (c == '(') { *sh = 1; return 10; }
	if (c == ')') { *sh = 1; return 11; }
	if (c == ' ') return 57;
	if (c == '\\') return 43;
	return -1;
}
static void b76_keys_task(void *a)
{
	for (char *c = b76_kbuf; *c; c++) {
		int sh, k;
		if (*c == '^') { kpw(29, 1); kpw(1, 1); vTaskDelay(pdMS_TO_TICKS(50)); kpw(1, 0); kpw(29, 0); vTaskDelay(pdMS_TO_TICKS(1500)); }
		else if (*c == '|') { printf("[b76] t_us=%lld ENTER\n", (long long) esp_timer_get_time()); tap(28, 0); vTaskDelay(pdMS_TO_TICKS(300)); }
		else if (*c == '~') { kpw(56, 1); tap(62, 0); kpw(56, 0); vTaskDelay(pdMS_TO_TICKS(500)); }
		else if (*c == '`' && c[1]) {   /* B87 (owner tour of the Start menu): `u `d `l `r arrows (Linux codes), `e Esc, `w 0.8 s, `W 2.5 s pause */
			c++;
			if (*c == 'u') tap(103, 0); else if (*c == 'd') tap(108, 0); else if (*c == 'l') tap(105, 0); else if (*c == 'r') tap(106, 0);
			else if (*c == 'e') tap(1, 0); else if (*c == 'w') vTaskDelay(pdMS_TO_TICKS(800)); else if (*c == 'W') vTaskDelay(pdMS_TO_TICKS(2500));
		}
		else if ((k = code(*c, &sh)) >= 0) tap(k, sh);
		if (c == b76_kbuf + 1 && b76_kbuf[0] == '^') vTaskDelay(pdMS_TO_TICKS(1500));   /* the Run box after 'r' */
	}
	printf("[b76] keys done\n"); vTaskDelete(NULL);
}
void b76_keys(const char *s) { strncpy(b76_kbuf, s, sizeof b76_kbuf - 1); xTaskCreate(b76_keys_task, "b76keys", 3072, NULL, 3, NULL); }
/* rust_eh_personality: not referenced by the riscv build */

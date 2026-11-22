/* B84: Doom sound in the Doom-only boot (lab:LEAD/B84/REPORT.md). Mixer core = b84mix.c (portable, host A/B in tools/b84).
 *  - ES8311 + I2S come up in i2s.c as in the Windows boot, but in the Doom boot with 4 x 128-frame DMA (11.6 ms), auto-clear, codec
 *    volume 0 and the PA (GPIO20) off until the mixer has streamed silence: no pop at power-up;
 *  - mixer task: core 1 (the present task there sleeps on the PPA most of a frame; core 0 renders), priority 7 > present (6), all its
 *    buffers + the OPL3 chip state in internal RAM (this boot has no emulator). One 128-frame block = voices (under a short spinlock) +
 *    OPL music (Nuked, rendered per block in this task) + EQ/limiter, then a blocking i2s_channel_write paces the loop;
 *  - PA off after 1 s of digital silence, on at the first non-silent block (the DMA queue gives it ~9 ms before the sound arrives);
 *  - quit / error -> b84_release(): 20 ms master fade, silence flushed, PA off, codec volume 0, channel disabled. The Windows boot then
 *    initialises the codec from scratch (B81 owns it there).
 * Static internal RAM: one pointer in PSRAM .bss; everything else is allocated in the Doom boot only. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_attr.h"
#include "b84mix.h"
#include "doomtype.h"
#include "i_sound.h"
#include "w_wad.h"
#include "z_zone.h"
#include "deh_str.h"
#include "b83api.h"   /* B83: W_* / Z_Malloc of the launched engine (C or Rust) */

i2s_chan_handle_t i2s_tx(void); void i2s_codec_vol(int v);   /* i2s.c */
void OPL_ESP_Render(int16_t *stereo, unsigned int n); int OPL_ESP_IsActive(void);

#define PA_IO 20
#define FADE (B84_SR / 50)            /* 20 ms master fade at start and quit */
#define PA_IDLE (B84_SR / B84_BLK)    /* ~1 s of silent blocks */
#define MUSB 512                      /* OPL render block (frames, 11.6 ms) */
#define MUSF (4 * MUSB)               /* music ring: 46 ms ahead of the mixer */

typedef struct {
	b84_mix_t m;
	float acc[B84_BLK];
	int16_t out[2 * B84_BLK];
	int16_t *ring; uint32_t rwr, rrd;   /* music ring (stereo, PSRAM: 176 KB/s is nothing; t4: L2MEM DMA heap is the SD card's) */
	TaskHandle_t opl; uint32_t opl_us, opl_miss;
	uint64_t bytes, bytes0;                       /* I2S bytes accepted (stats: bytes/s) */
	int16_t *cap; uint32_t capn, capw; volatile int capst;   /* doomcap: the exact samples sent to I2S (left), PSRAM */
	portMUX_TYPE mux;
	volatile uint32_t underrun; uint32_t werr;
	uint32_t blocks, busy_us, max_us, under0; int64_t t0;
	int pa, idle, vol;
	volatile int quit; TaskHandle_t waiter;
} snd_t;
static EXT_RAM_BSS_ATTR snd_t *S;
EXT_RAM_BSS_ATTR int b84_loud_boot;   /* doom go ... loud: boost from the first block (safety test without the console) */

EXT_RAM_BSS_ATTR int use_libsamplerate; EXT_RAM_BSS_ATTR float libsamplerate_scale;   /* m_config binds them (i_sdlsound.c is not built) */

static bool on_ovf(i2s_chan_handle_t h, i2s_event_data_t *e, void *u) { (void)h; (void)e; (void)u; S->underrun++; return false; }

static void pa(int on) { if (S->pa != on) { gpio_set_level(PA_IO, on); S->pa = on; } }

static void flush_silence(i2s_chan_handle_t tx, int blocks)
{
	size_t bw; memset(S->out, 0, sizeof S->out);
	for (int i = 0; i < blocks; i++) i2s_channel_write(tx, S->out, sizeof S->out, &bw, pdMS_TO_TICKS(50));
}



/* OPL music: low priority on core 1 (below present 6 and the mixer 7): a slow render can only cost music, never frames or SFX.
 * Renders MUSB frames whenever the ring has room; the mixer wakes it after each block it takes. */
static void opl_task(void *a)
{
	(void)a;
	/* (board B t5/t6 bench, 128 samples at 360 MHz: Nuked OPL3 3.2-4.0 ms (L2MEM or PSRAM), 115 ms (LP SRAM); fmopl 0.29-0.37 ms. Not run at start any
	 * more: fmopl tables are shared, a bench beside the real chip init raced them.) */
	for (;;) {
		uint32_t wr = S->rwr, rd = __atomic_load_n(&S->rrd, __ATOMIC_ACQUIRE);
		if (OPL_ESP_IsActive() && wr - rd <= MUSF - MUSB) {
			int64_t t = esp_timer_get_time();
			OPL_ESP_Render(&S->ring[2 * (wr % MUSF)], MUSB);
			S->opl_us += (uint32_t)(esp_timer_get_time() - t);
			__atomic_store_n(&S->rwr, wr + MUSB, __ATOMIC_RELEASE);
			continue;
		}
		ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20));
	}
}

static void mix_task(void *a)
{
	(void)a;
	i2s_chan_handle_t tx = i2s_tx();
	i2s_event_callbacks_t cb = { .on_send_q_ovf = on_ovf };
	esp_err_t e0 = i2s_channel_disable(tx);   /* esp_codec_dev_open may have left it running: callbacks register only while stopped */
	esp_err_t e1 = i2s_channel_register_event_callback(tx, &cb, NULL);
	pa(0);
	esp_err_t e2 = i2s_channel_enable(tx);
	fprintf(stderr, "[b84] i2s disable %d callback %d enable %d\r\n", (int)e0, (int)e1, (int)e2);
	flush_silence(tx, 8);          /* clocks + codec settle on silence, then the volume */
	i2s_codec_vol(S->vol);
	portENTER_CRITICAL(&S->mux); b84_fade(&S->m, 1, FADE); portEXIT_CRITICAL(&S->mux);
	S->t0 = esp_timer_get_time();
	for (;;) {
		int64_t t0 = esp_timer_get_time();
		portENTER_CRITICAL(&S->mux); b84_voices(&S->m, S->acc); portEXIT_CRITICAL(&S->mux);
		const int16_t *mus = NULL;
		uint32_t rd = S->rrd, wr = __atomic_load_n(&S->rwr, __ATOMIC_ACQUIRE);
		if (wr - rd >= B84_BLK) mus = &S->ring[2 * (rd % MUSF)]; else if (OPL_ESP_IsActive()) S->opl_miss++;
		b84_finish(&S->m, S->acc, mus, S->out);
		if (mus) { __atomic_store_n(&S->rrd, rd + B84_BLK, __ATOMIC_RELEASE); xTaskNotifyGive(S->opl); }
		int loud = 0;
		for (int i = 0; i < 2 * B84_BLK; i += 8) if (S->out[i] > 16 || S->out[i] < -16) { loud = 1; break; }
		if (loud) { S->idle = 0; pa(1); } else if (++S->idle > PA_IDLE) pa(0);
		int64_t t3 = esp_timer_get_time();
		uint32_t us = (uint32_t)(t3 - t0); S->busy_us += us; if (us > S->max_us) S->max_us = us;
		size_t bw = 0;
		if (i2s_channel_write(tx, S->out, sizeof S->out, &bw, pdMS_TO_TICKS(100)) != ESP_OK || !bw) { S->werr++; vTaskDelay(1); }   /* never spin at prio 7 */
		S->bytes += bw;
		if (S->capst == 1) {
			for (int i = 0; i < B84_BLK && S->capw < S->capn; i++) S->cap[S->capw++] = S->out[2 * i];
			if (S->capw >= S->capn) S->capst = 2;   /* cap_task writes the file */
		}
		if (++S->blocks % (5 * B84_SR / B84_BLK) == 0) {
			double el = (double)(t3 - S->t0);
			fprintf(stderr, "[b84] snd i2s %.0f B/s (44100 x 2 ch x 2 B = 176400) mix cpu %.2f%% opl cpu %.2f%% max %lu us/blk (budget %d) underruns %lu (+%lu) music misses %lu starts %lu steals %lu pa %d vol %d werr %lu dma free %u largest %u\r\n",
				(double)(S->bytes - S->bytes0) * 1e6 / el, 100.0 * S->busy_us / el, 100.0 * S->opl_us / el, (unsigned long)S->max_us, (int)(1000000LL * B84_BLK / B84_SR),
				(unsigned long)S->underrun, (unsigned long)(S->underrun - S->under0), (unsigned long)S->opl_miss, (unsigned long)S->m.starts, (unsigned long)S->m.steals, S->pa, S->vol, (unsigned long)S->werr,
				(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
			S->t0 = t3; S->busy_us = S->opl_us = S->max_us = 0; S->under0 = S->underrun; S->bytes0 = S->bytes;
		}
		if (S->quit && !S->m.mn) {   /* faded out: release the codec cleanly */
			flush_silence(tx, 4); pa(0); i2s_codec_vol(0); i2s_channel_disable(tx);
			fprintf(stderr, "[b84] codec released (underruns %lu)\r\n", (unsigned long)S->underrun);
			if (S->opl) vTaskDelete(S->opl);
			S->quit = 2; if (S->waiter) xTaskNotifyGive(S->waiter);
			vTaskDelete(NULL);
		}
	}
}

int b84_up(void)
{
	if (S) return 1;
	size_t hf0 = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA), hl0 = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
	snd_t *s = heap_caps_calloc(1, sizeof *s, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);   /* not LP SRAM (heap has RTC fast mem): ~40x slower, t3 */
	if (!s || !i2s_tx()) { fprintf(stderr, "[b84] no sound (%s)\r\n", s ? "no I2S" : "no RAM"); free(s); return 0; }
	b84_init(&s->m, 1); if (b84_loud_boot) s->m.boost = 16; s->mux = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED; s->pa = -1; s->vol = 30;   /* owner 21 Nov: heard 50 %, "30 % is enough" (cap 70, never 100) */
	S = s;
	s->ring = heap_caps_calloc(2 * MUSF, sizeof *s->ring, MALLOC_CAP_SPIRAM);
	if (!s->ring) { fprintf(stderr, "[b84] no PSRAM for the music ring\r\n"); return 0; }
	/* stacks: L2MEM (not LP SRAM, not PSRAM) but small - the SD card's DMA bounce buffers come from the same heap (t4: sdmmc read failed) */
	if (xTaskCreatePinnedToCoreWithCaps(opl_task, "b84opl", 4096, NULL, 3, &s->opl, 1, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA) != pdPASS ||
	    xTaskCreatePinnedToCoreWithCaps(mix_task, "b84mix", 4096, NULL, 7, NULL, 1, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA) != pdPASS) { fprintf(stderr, "[b84] no task\r\n"); return 0; }
	fprintf(stderr, "[b84] mixer up: %d Hz, %d-frame blocks, core 1 prio 7, codec vol %d, state %u B internal\r\n", B84_SR, B84_BLK, s->vol, (unsigned)sizeof *s);
	fprintf(stderr, "[b84] HP internal (INTERNAL|DMA) free %u -> %u, largest %u -> %u (state + 2 stacks; the OPL chip comes with the music init)\r\n",
		(unsigned)hf0, (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA), (unsigned)hl0, (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
	return 1;
}

void b84_release(void)
{
	if (!S || S->quit) return;
	S->waiter = xTaskGetCurrentTaskHandle();
	portENTER_CRITICAL(&S->mux); b84_fade(&S->m, 0, FADE); S->quit = 1; portEXIT_CRITICAL(&S->mux);
	ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200));
	if (S->quit != 2) { gpio_set_level(PA_IO, 0); i2s_codec_vol(0); fprintf(stderr, "[b84] release timeout: PA off\r\n"); }
}

void b84_loud(int on) { if (S) { S->m.boost = on ? 16 : 1; fprintf(stderr, "[b84] worst-case loudness %s (limiter pinned at -3 dBFS)\r\n", on ? "ON" : "off"); } }
/* doomcap N: record N s (1..10) of the mixer output exactly as written to I2S -> /sdcard/DOOM/b84cap.wav (44.1 kHz 16-bit mono = left) */
static void cap_task(void *a)
{
	(void)a;
	while (S->capst != 2) vTaskDelay(pdMS_TO_TICKS(50));
	FILE *f = fopen("/sdcard/DOOM/b84cap.wav", "wb");
	if (f) {
		uint32_t n = S->capw * 2, h[11] = { 0x46464952, 36 + n, 0x45564157, 0x20746d66, 16, 0x00010001, B84_SR, B84_SR * 2, 0x00100002, 0x61746164, n };
		fwrite(h, 4, 11, f); fwrite(S->cap, 2, S->capw, f); fclose(f);
	}
	fprintf(stderr, "[b84] capture %s: %lu samples -> /sdcard/DOOM/b84cap.wav (errno %d)\r\n", f ? "written" : "FAILED", (unsigned long)S->capw, f ? 0 : errno);
	heap_caps_free(S->cap); S->cap = NULL; S->capst = 0;
	vTaskDelete(NULL);
}
void b84_capture(int secs)
{
	if (!S || S->capst) return;
	secs = secs < 1 ? 1 : secs > 10 ? 10 : secs;
	S->cap = heap_caps_malloc((size_t)secs * B84_SR * 2, MALLOC_CAP_SPIRAM);
	if (!S->cap) { fprintf(stderr, "[b84] capture: no PSRAM\r\n"); return; }
	S->capn = secs * B84_SR; S->capw = 0; S->capst = 1;
	xTaskCreatePinnedToCore(cap_task, "b84cap", 4096, NULL, 2, NULL, 0);
	fprintf(stderr, "[b84] capture %d s started\r\n", secs);
}
void b84_volume(int v) { if (S) { S->vol = v < 0 ? 0 : v > 70 ? 70 : v; i2s_codec_vol(S->vol); fprintf(stderr, "[b84] codec vol %d\r\n", S->vol); } }

/* ---- doom sound module */
static boolean use_prefix;

static b84_sfx_t *cache(sfxinfo_t *sfx)
{
	if (sfx->driver_data) return sfx->driver_data;
	if (sfx->lumpnum < 0) return NULL;
	b84_sfx_t *c = Z_Malloc(sizeof *c, PU_STATIC, NULL);
	b84_sfx_set(c, W_CacheLumpNum(sfx->lumpnum, PU_STATIC), W_LumpLength(sfx->lumpnum));
	sfx->driver_data = c;
	return c->len ? c : NULL;
}
static void name(sfxinfo_t *sfx, char *nm) { snprintf(nm, 16, use_prefix ? "ds%s" : "%s", DEH_String(sfx->name)); }

static boolean snd_Init(boolean pre) { use_prefix = pre; fprintf(stderr, "[b84] snd_Init\r\n"); return b84_up() ? true : false; }
static void snd_Shutdown(void) {}
static int snd_GetSfxLumpNum(sfxinfo_t *sfx) { char nm[16]; if (sfx->link) sfx = sfx->link; name(sfx, nm); return W_GetNumForName(nm); }
static void snd_Update(void) {}
static void snd_UpdateSoundParams(int ch, int vol, int sep)
{
	(void)sep;   /* one speaker */
	portENTER_CRITICAL(&S->mux); b84_param(&S->m, ch, vol); portEXIT_CRITICAL(&S->mux);
}
static int snd_StartSound(sfxinfo_t *sfx, int ch, int vol, int sep)
{
	(void)sep;
	b84_sfx_t *c = cache(sfx);
	if (!c || !c->len) return -1;
	portENTER_CRITICAL(&S->mux); int r = b84_start(&S->m, ch, c, vol); portEXIT_CRITICAL(&S->mux);
	return r;
}
static void snd_StopSound(int ch) { portENTER_CRITICAL(&S->mux); b84_stop(&S->m, ch); portEXIT_CRITICAL(&S->mux); }
static boolean snd_SoundIsPlaying(int ch) { portENTER_CRITICAL(&S->mux); int r = b84_playing(&S->m, ch); portEXIT_CRITICAL(&S->mux); return r ? true : false; }
static void snd_CacheSounds(sfxinfo_t *sounds, int n)
{
	int ok = 0; char nm[16];
	for (int i = 0; i < n; i++) {   /* all up front (PSRAM zone): StartSound never reads the SD card mid-game */
		if (sounds[i].link) continue;
		name(&sounds[i], nm);
		int l = W_CheckNumForName(nm);
		if (l < 0) continue;
		sounds[i].lumpnum = l;
		if (cache(&sounds[i])) ok++;
	}
	fprintf(stderr, "[b84] precached %d/%d sfx\r\n", ok, n);
}

/* the module lives in PSRAM .bss and is filled by b84_bind() before the engine starts: initialised data would sit in internal .data in
 * the Windows boot too (budget +300 B) */
static const snddevice_t devs[] = { SNDDEVICE_SB, SNDDEVICE_PAS, SNDDEVICE_GUS, SNDDEVICE_WAVEBLASTER, SNDDEVICE_SOUNDCANVAS, SNDDEVICE_AWE32 };
EXT_RAM_BSS_ATTR sound_module_t DG_sound_module;
void b84_bind(void)
{
	DG_sound_module = (sound_module_t){ (snddevice_t *)devs, sizeof devs / sizeof devs[0], snd_Init, snd_Shutdown, snd_GetSfxLumpNum, snd_Update,
		snd_UpdateSoundParams, snd_StartSound, snd_StopSound, snd_SoundIsPlaying, snd_CacheSounds };
	use_libsamplerate = 0; libsamplerate_scale = 0.65f;
}

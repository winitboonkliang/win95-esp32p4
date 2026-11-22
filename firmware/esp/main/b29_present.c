/* BLUE B29a: damage-driven present of the linear 1280x800 RGB565 guest surface (Rust LinearPresenter, B20 core) + host-side pointer.
 *  - present task on core 1 (same core as the emulator: no L1 coherence issue), runs independent of guest speed;
 *  - damage rects come from the guest driver through the card (gfx_damage_hook), pointer position/shape from gfx + the HID callback;
 *  - b29_mouse_in(): the USB HID callback entry. The host pointer moves at once (gfx_pred_move); Windows gets coalesced
 *    accel-compensated deltas at <= 30 Hz and an exact flush before every button change. */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_async_memcpy.h"
#include "freertos/semphr.h"
#include "common.h"
#include "../../pc.h"
#include "../../i8042.h"
#include "../../gfxcard.h"

typedef struct LinearPresenter LinearPresenter;
typedef void (*ppa_cb_t)(int, int, int, int, uint16_t *);
size_t b29l_state_size(void);
LinearPresenter *b29l_init(uint8_t *mem, int nfb);
void b29l_damage(LinearPresenter *p, int x0, int y0, int x1, int y1);
void b29l_damage_all(LinearPresenter *p);
void b29l_set_mode(LinearPresenter *p, int w, int h);
int b29l_scale4(LinearPresenter *p);
void b29l_cursor_shape(LinearPresenter *p, int w, int h, int hx, int hy, const uint32_t *and_, const uint32_t *xor_, int x, int y, int visible);
void b29l_cursor_move(LinearPresenter *p, int x, int y, int visible);
int b29l_present(LinearPresenter *p, const uint16_t *surf, uint16_t *back, ppa_cb_t ppa, uint16_t *rects_out, uint32_t *src_px);
bool b29_ppa_block_rot(const void *src_pic, size_t pic_w, size_t pic_h, size_t x, size_t y, size_t w, size_t h, void *dst, size_t dst_w, size_t dst_h, size_t dst_size, float scale);
void *lcd_back_fb(void);
void *lcd_fb(int i);
int b29l_cursor_fast(LinearPresenter *p, const uint16_t *surf, uint16_t *buf0, uint16_t *buf1, uint16_t *rects_out);
void lcd_swap_present(void);
void lcd_wait_flip(void);
int lcd_wait_flip_ms(int ms);
void lcd_flip_timeout(void);
int lcd_panel_ready(void);
extern int pc_vga_refresh_gate;
extern volatile unsigned long lcd_present_count;
extern volatile unsigned long b29_refresh_events, b29_flip_timeouts;
volatile unsigned long b29_vga_redraws;   /* counted in the team build only */
volatile unsigned long b29_base_msync_err, b29_base_msync_n, b29_base_ppa_fail, b29_base_ppa_n;   /* base presenter counters (not wired in the merge tree: stay 0) */

#define SW 1280
#define SH 800
#define PWID 800
#define PHGT 1280

static LinearPresenter *lp;
static const uint16_t *surf;
static TaskHandle_t ptask;
static volatile int lin_on;
volatile long b29_applied_moves;     /* pred_moves value that the last finished present contained */
volatile unsigned long b29_fast, b29_presents, b29_cursor_only, b29_rects, b29_src_px, b29_pres_us, b29_pres_n;

/* ---- damage ring (emulator core -> presenter task, same core, single producer/consumer) */
#define DR 512
static volatile uint32_t dw, dr_;
static int16_t dring[DR][4];
static volatile int dr_overflow;
extern volatile unsigned long b29_pushed;
void b29_damage_push(int l, int t, int r, int b)
{
	if (!lin_on) return;
	uint32_t w = dw;
	if (w - dr_ >= DR) { dr_overflow = 1; return; }
	b29_pushed++;
	int16_t *e = dring[w % DR]; e[0] = l; e[1] = t; e[2] = r; e[3] = b;
	dw = w + 1;
}

/* esp_cache_msync needs a 128 B aligned start AND size (it returned ESP_ERR_INVALID_ARG on the unaligned guest surface base, so the source rows
 * were never written back and the PPA read stale pixels).  Align the range outward by absolute address. */
static void msync128(const void *p, size_t n, int flags)
{
	uintptr_t a = (uintptr_t)p & ~(uintptr_t)127, e = ((uintptr_t)p + n + 127) & ~(uintptr_t)127;
	esp_err_t r = esp_cache_msync((void *)a, e - a, flags);
	if (r != ESP_OK) { static int shown; if (shown++ < 3) { fprintf(stderr, "b29: msync fail %d a=%p n=%u\r\n", (int)r, (void *)a, (unsigned)(e - a)); } }
}

int b29_ppa_fail;
extern int b29_ppa_lasterr, b29_ppa_lastrect[4]; extern unsigned long b29_ppa_contend;
extern volatile int card_quiet;   /* cardsvc.c: set while a screenshot streams: console text inside a frame corrupts it */
/* card service "shot": the GFX95 guest surface (tightly packed RGB565, stride = width) while the card is active */
int b29_guest_surface(const uint8_t **p, int *w, int *h);
static const int mode_w = 1280, mode_h = 800, mode_s4 = 4;   /* guest surface width (= stride in pixels) and panel scale in quarters, set on every mode change */
static void ppa_blk(int x0, int y0, int x1, int y1, uint16_t *back)
{
	/* Destination lines first: an earlier CPU-rotated rect (or the pointer) may have left DIRTY cache lines that share a 128 B line with this
	 * rect.  Written back after the PPA they would overwrite its fresh pixels with stale ones (ghosts, smear).  Write them back and drop them
	 * BEFORE the DMA runs; later CPU reads of these lines then fetch the PPA result.  Panel row = scaled source x, 1600 B per panel row. */
	{
		size_t a = ((size_t)(x0 * mode_s4 / 4) * PWID * 2) & ~(size_t)127, e = (((size_t)(x1 * mode_s4 / 4) * PWID * 2) + 127) & ~(size_t)127;
		if (e > (size_t)PWID * PHGT * 2) { e = (size_t)PWID * PHGT * 2; }
		msync128((uint8_t *)back + a, e - a, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_INVALIDATE);
	}
	/* guest-written source rows -> memory (C2M); the guest surface base is not 128 B aligned, msync128 aligns outward */
	msync128((const void *)(surf + (size_t)y0 * mode_w), (size_t)(y1 - y0) * mode_w * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
	if (!b29_ppa_block_rot(surf, mode_w, mode_h, x0, y0, x1 - x0, y1 - y0, back, PWID, PHGT, (size_t)PWID * PHGT * 2, mode_s4 / 4.0f)) { b29_ppa_fail++; }
}

static void sync_panel_rect(uint16_t *back, const uint16_t *r)
{
	size_t a = (size_t)r[1] * PWID * 2, b = (size_t)r[3] * PWID * 2;
	a &= ~(size_t)127; b = (b + 127) & ~(size_t)127;
	if (b > (size_t)PWID * PHGT * 2) b = (size_t)PWID * PHGT * 2;
	msync128((uint8_t *)back + a, b - a, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_INVALIDATE);
}

/* ---- pointer shape from the card (MSB-first bytes, 32 rows x 4) -> rows of u32, bit i = pixel i */
static uint32_t rev32(uint32_t v) { v = ((v >> 1) & 0x55555555u) | ((v & 0x55555555u) << 1); v = ((v >> 2) & 0x33333333u) | ((v & 0x33333333u) << 2);
	v = ((v >> 4) & 0x0f0f0f0fu) | ((v & 0x0f0f0f0fu) << 4); return __builtin_bswap32(v); }
static long shape_seen = -1;
static void sync_pointer(void)
{
	int x = gfx.pred_valid ? gfx.px : gfx.cx, y = gfx.pred_valid ? gfx.py : gfx.cy, vis = gfx.cvis;
	if (gfx.shapes != shape_seen) {
		shape_seen = gfx.shapes;
		const uint8_t *m = (const uint8_t *)&gfx.shape[4];
		uint32_t a[32], xo[32];
		for (int j = 0; j < 32; j++) {
			uint32_t av = (uint32_t)m[j * 4] << 24 | m[j * 4 + 1] << 16 | m[j * 4 + 2] << 8 | m[j * 4 + 3];
			uint32_t xv = (uint32_t)m[128 + j * 4] << 24 | m[128 + j * 4 + 1] << 16 | m[128 + j * 4 + 2] << 8 | m[128 + j * 4 + 3];
			a[j] = rev32(av); xo[j] = rev32(xv);
		}
		b29l_cursor_shape(lp, gfx.shape[2], gfx.shape[3], gfx.shape[0], gfx.shape[1], a, xo, x, y, vis);
	} else b29l_cursor_move(lp, x, y, vis);
}

/* ---- mouse: host pointer at once, Windows coalesced */
static int win_f(int d) { return (d > 2 || d < -2) ? 2 * d : d; }
static int pick(int w) { if (w >= -2 && w <= 2) return w; int a = w < 0 ? -w : w, s = a >= 6 ? a / 2 : 2; return w < 0 ? -s : s; }
static portMUX_TYPE mm = portMUX_INITIALIZER_UNLOCKED;
static int wantx, wanty, lastb; static int64_t last_us, last_in_us;
#define COAL_US 33000
static void deliver_locked(int b)
{
	int sx = pick(wantx), sy = pick(wanty);
	if (sx || sy || b != lastb) { ps2_mouse_event(globals.mouse, sx, sy, 0, b); wantx -= win_f(sx); wanty -= win_f(sy); }
	lastb = b; last_us = esp_timer_get_time();
}
void b45_host_rel(uint32_t t, int dx, int dy, int b);
void b29_present_kick(void) { if (ptask) xTaskNotifyGive(ptask); }
void b29_mouse_in(int dx, int dy, int b)
{
	if (!globals.mouse) return;
	{ extern int ptrdev_enabled(void); if (ptrdev_enabled()) { b45_host_rel((uint32_t)esp_timer_get_time(), dx, dy, b); return; } } /* B45: one pointer path once P4PTR.DRV runs */
	if (!gfx_active()) { ps2_mouse_event(globals.mouse, dx, dy, 0, b); lastb = b; return; }
	gfx_pred_move(dx, dy);
	taskENTER_CRITICAL(&mm);
	last_in_us = esp_timer_get_time();
	wantx += win_f(dx); wanty += win_f(dy);
	if (b != lastb) { for (int i = 0; i < 64 && (wantx || wanty); i++) { int px = wantx, py = wanty; deliver_locked(lastb); if (px == wantx && py == wanty) break; } deliver_locked(b); }
	else if (esp_timer_get_time() - last_us >= COAL_US) deliver_locked(b);
	taskEXIT_CRITICAL(&mm);
	if (ptask) xTaskNotifyGive(ptask);
}
static void mouse_tick(void)
{
	if (!gfx_active() || !(wantx || wanty)) return;
	taskENTER_CRITICAL(&mm);
	int64_t now = esp_timer_get_time();
	if ((wantx || wanty) && now - last_us >= COAL_US) deliver_locked(lastb);
	/* at rest the unreachable remainder (3, 4, 5 px ...) is flushed exactly with up to 4 small packets, so a click lands where the pointer is drawn */
	if ((wantx || wanty) && now - last_in_us > 80000) { for (int i = 0; i < 4 && (wantx || wanty); i++) { int px = wantx, py = wanty; deliver_locked(lastb); if (px == wantx && py == wanty) break; } }
	taskEXIT_CRITICAL(&mm);
}

#define PEND 48
extern unsigned long cpu_halt_sleeps;
static int16_t pend[PEND][4], again[PEND][4]; static int npend, nagain, again_all; static int64_t pend_t; static unsigned long last_halts;
int b29_dbg_pend(void) { return npend * 1000 + nagain; }

volatile int b29_force_all;
volatile unsigned long b29_forced, b29_ovf, b29_merge, b29_rel, b29_pushed;

extern int b29_ppa_fail;
/* B29V: compare the guest surface with both panel buffers (tile 16x16, cursor area excluded) -> names presenter vs guest as the source of debris */
void *lcd_front_fb(void); int b29_dbg_pend(void);
/* DMA view: copy a DPI buffer with the AXI-DMA memcpy engine (reads PSRAM exactly like the DPI scan-out does, no CPU cache involved) into a scratch buffer, then compare that
 * copy with the guest frame and with the CPU view (after an invalidate).  A difference between "DMA view" and "CPU view" = cache staleness; a difference between DMA view
 * and guest = the buffer never received the damage (or received it late). */
static async_memcpy_handle_t am_h; static SemaphoreHandle_t am_sem; static uint8_t *am_scratch; static int am_state;   /* 0 none, 1 ok, -1 failed */
static bool am_cb(async_memcpy_handle_t h, async_memcpy_event_t *e, void *u) { BaseType_t hp = pdFALSE; xSemaphoreGiveFromISR(am_sem, &hp); return hp == pdTRUE; }
static int dma_copy(const void *src, void *dst, size_t n)
{
	const size_t chunk = 16384;
	for (size_t o = 0; o < n; o += chunk) {
		size_t c = n - o < chunk ? n - o : chunk;
		if (esp_async_memcpy(am_h, (uint8_t *)dst + o, (void *)((const uint8_t *)src + o), c, am_cb, NULL) != ESP_OK) { return -1; }
		if (xSemaphoreTake(am_sem, pdMS_TO_TICKS(200)) != pdTRUE) { return -2; }
	}
	return 0;
}
static void dmaview_check(const char *tag)
{
	if (am_state == 0) {
		async_memcpy_config_t cfg = ASYNC_MEMCPY_DEFAULT_CONFIG();
		am_sem = xSemaphoreCreateBinary();
		am_scratch = heap_caps_aligned_alloc(128, (size_t)PWID * PHGT * 2, MALLOC_CAP_SPIRAM);
		esp_err_t e = esp_async_memcpy_install_gdma_axi(&cfg, &am_h);
		am_state = (e == ESP_OK && am_scratch && am_sem) ? 1 : -1;
		if (am_state < 0) { fprintf(stderr, "B29D dma memcpy unavailable (%d)\r\n", (int)e); }
	}
	if (am_state < 0) { return; }
	for (int b = 0; b < 2; b++) {
		const uint16_t *P = (const uint16_t *)lcd_fb(b);
		msync128(am_scratch, (size_t)PWID * PHGT * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_INVALIDATE);
		int r = dma_copy(P, am_scratch, (size_t)PWID * PHGT * 2);
		msync128(am_scratch, (size_t)PWID * PHGT * 2, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
		msync128(P, (size_t)PWID * PHGT * 2, ESP_CACHE_MSYNC_FLAG_DIR_M2C);   /* CPU view = memory too */
		const uint16_t *D = (const uint16_t *)am_scratch;
		int dg = 0, dc = 0;
		for (int ty = 0; ty < 50; ty++) for (int tx = 0; tx < 80; tx++) {
			int a = 0, c = 0;
			for (int y = ty * 16; y < ty * 16 + 16; y++) for (int x = tx * 16; x < tx * 16 + 16; x++) {
				uint16_t dv = D[x * 800 + (799 - y)];
				if (dv != surf[(size_t)y * SW + x]) { a = 1; }
				if (dv != P[x * 800 + (799 - y)]) { c = 1; }
			}
			dg += a; dc += c;
		}
		fprintf(stderr, "B29D %s buf%d dma_rc=%d dmaview_vs_guest_tiles=%d dmaview_vs_cpuview_tiles=%d base_msync_err=%lu/%lu base_ppa_fail=%lu/%lu\r\n", tag, b, r, dg, dc, b29_base_msync_err, b29_base_msync_n, b29_base_ppa_fail, b29_base_ppa_n);
	}
}
static void verify(const char *tag)
{
	const uint16_t *S = surf;
	for (int w = 0; w < 2; w++) {
		const uint16_t *P = (const uint16_t *)(w ? lcd_back_fb() : lcd_front_fb());
		esp_cache_msync((void *)P, 800 * 1280 * 2, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
		int bad = 0, shown = 0;
		for (int ty = 0; ty < 50; ty++) for (int tx = 0; tx < 80; tx++) {
			int d = 0, fx = 0, fy = 0, nd = 0; uint16_t pv = 0, sv = 0;
			for (int y = ty * 16; y < ty * 16 + 16; y++) for (int x = tx * 16; x < tx * 16 + 16; x++)
				if (P[x * 800 + (799 - y)] != S[y * 1280 + x]) { if (!d) { d = 1; fx = x; fy = y; pv = P[x * 800 + (799 - y)]; sv = S[y * 1280 + x]; } nd++; }
			if (d) { bad++; if (shown < 16) { fprintf(stderr, "B29V %s %s tile %d,%d first(%d,%d) panel=%04x guest=%04x ndiff=%d\r\n", tag, w ? "back" : "front", tx, ty, fx, fy, pv, sv, nd); shown++; } }
		}
		fprintf(stderr, "B29V refresh_events=%lu flip_timeouts=%lu ", b29_refresh_events, b29_flip_timeouts); fprintf(stderr, "B29V ppa_fail=%d forced=%lu overflow=%lu merge=%lu released=%lu pushed=%lu ", b29_ppa_fail, b29_forced, b29_ovf, b29_merge, b29_rel, b29_pushed); fprintf(stderr, "B29V %s %s bad_tiles=%d (pointer tiles included) depth=%d pend/again=%d\r\n", tag, w ? "back" : "front", bad, (int)gfx.depth, b29_dbg_pend());
	}
	dmaview_check(tag);
}
/* ASCII art of a guest rectangle next to the same rectangle read back from the FRONT panel buffer (what the DPI DMA scans), dark = '#' */
static volatile int areq_x, areq_y, areq_w, areq_h, adone; static const char *volatile areq_tag;
void b29_ascii_req(const char *tag, int x, int y, int w, int h) { adone = 0; areq_x = x; areq_y = y; areq_w = w; areq_h = h; areq_tag = tag; while (!adone) vTaskDelay(pdMS_TO_TICKS(20)); }
static void ascii_dump(void)
{
	const uint16_t *P = (const uint16_t *)lcd_front_fb();
	msync128(P, (size_t)PWID * PHGT * 2, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
	int x0 = areq_x, y0 = areq_y, w = areq_w > 120 ? 120 : areq_w, h = areq_h, diff = 0;
	for (int y = y0; y < y0 + h; y++) {
		char g[124], p[124];
		for (int x = 0; x < w; x++) {
			uint16_t gv = surf[(size_t)y * SW + x0 + x], pv = P[(size_t)(x0 + x) * PWID + (PWID - 1 - y)];
			g[x] = gv < 0x8410 ? '#' : '.'; p[x] = pv < 0x8410 ? '#' : '.'; if (gv != pv) { diff++; }
		}
		g[w] = 0; p[w] = 0;
		fprintf(stderr, "A29 %s %3d G %s\r\n", areq_tag, y, g);
		fprintf(stderr, "A29 %s %3d P %s\r\n", areq_tag, y, p);
	}
	fprintf(stderr, "A29 %s rect %d,%d %dx%d guest!=panel pixels %d\r\n", areq_tag, x0, y0, w, h, diff);
}
static const char *volatile vreq; static volatile int vdone;
void b29_verify_req(const char *tag) { vdone = 0; vreq = tag; while (!vdone) vTaskDelay(pdMS_TO_TICKS(20)); }

static int fresh_on;
/* panel test pattern (display-path probe): while on, the presenter stops and the panel shows 1-px lines: left half = single scan lines (panel rows) every 4th row,
 * right half = single pixel columns every 4th column, top/bottom thirds with phase 0 / 1; photographed with the webcam to see which thin features survive scan-out */
static volatile int pat_req = -1, pat_on, pat_done;
void b29_pattern_req(int on) { pat_done = 0; pat_req = on; while (!pat_done) vTaskDelay(pdMS_TO_TICKS(20)); }
static void pattern_draw(void)
{
	lcd_wait_flip();
	uint16_t *b = (uint16_t *)lcd_back_fb();
	for (int py = 0; py < PHGT; py++) {
		for (int px = 0; px < PWID; px++) {
			int ph = py < PHGT / 2 ? 0 : 1;
			int on = px < PWID / 2 ? ((py & 3) == ph) : ((px & 3) == ph);
			b[(size_t)py * PWID + px] = on ? 0xFFFF : 0x0000;
		}
	}
	msync128(b, (size_t)PWID * PHGT * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_INVALIDATE);
	lcd_swap_present();
}
static long last_sig = -1;
static long cur_sig(void) { return (long)(gfx.pred_valid ? gfx.px : gfx.cx) * 7919 + (long)(gfx.pred_valid ? gfx.py : gfx.cy) * 104729 + gfx.cvis * 3 + gfx.shapes * 1000003; }
/* POINTER FAST PATH: a pointer-only change is written into both panel buffers at once (final pixels, each exactly once) and does not wait for a refresh, a flip or
 * the dirty-tile present; the scanned buffer may be written while on screen (a sprite-sized area, one scan pass long at most) */
static void pointer_fast(void)
{
	long seq = gfx.pred_moves, sig = cur_sig();
	if (sig == last_sig) return;
	sync_pointer();
	uint16_t fr[8];
	int fn = b29l_cursor_fast(lp, surf, (uint16_t *)lcd_fb(0), (uint16_t *)lcd_fb(1), fr);
	if (fn > 0) {
		for (int i = 0; i < fn; i++) { sync_panel_rect((uint16_t *)lcd_fb(0), &fr[i * 4]); sync_panel_rect((uint16_t *)lcd_fb(1), &fr[i * 4]); }
		{ extern void b45_presented(long, long, int64_t); b45_presented(b29_applied_moves, seq, esp_timer_get_time()); }   /* B45 latency: pointer in both panel buffers */
		b29_fast++; b29_applied_moves = seq; last_sig = sig;
	}
}

/* B29d: show only finished frames.  Guest damage is collected but not presented while the guest is still writing the LFB (gfx_lfb_wr moving):
 * a present waits until the writes were quiet for b29d_settle_us, or the oldest unpresented damage is b29d_cap_us old (so a long paint / a
 * drag still advances, at about one frame per cap).  settle 0 = old behaviour.  /sdcard/b29d.txt: "settle_us cap_us" (A/B without a rebuild).
 * Costs no memory and no copy; the pointer fast path above is not gated. */
extern long gfx_lfb_wr;
static int b29d_settle_us = 30000, b29d_cap_us = 150000;
volatile unsigned long b29d_held, b29d_capped, b29d_settled;
/* trace ring (console "b29dtrace" arms/dumps via b29drag): t_ms, LFB bytes written since the previous sample, event 1 held / 2 settled-present / 3 capped-present, 4 other present */
typedef struct { uint32_t t, wr; uint8_t ev; } B29dTr;
static B29dTr *b29d_tr; static volatile int b29d_ntr, b29d_tron; static long b29d_trw;
static inline void b29d_rec(int64_t now, long wr, int ev) { if (b29d_tron && b29d_tr && b29d_ntr < 5000 && (ev || wr != b29d_trw)) { B29dTr *e = &b29d_tr[b29d_ntr]; e->t = (uint32_t)(now / 1000); e->wr = (uint32_t)(wr - b29d_trw); e->ev = ev; b29d_ntr++; } b29d_trw = wr; }
void b29d_trace(int on) { if (on) { if (!b29d_tr) b29d_tr = heap_caps_malloc(5000 * sizeof(B29dTr), MALLOC_CAP_SPIRAM); if (!b29d_tr) return; b29d_ntr = 0; b29d_tron = 1; } else { b29d_tron = 0; for (int i = 0; i < b29d_ntr; i++) fprintf(stderr, "B29T %lu %lu %u\r\n", (unsigned long)b29d_tr[i].t, (unsigned long)b29d_tr[i].wr, (unsigned)b29d_tr[i].ev); fprintf(stderr, "B29T end %d\r\n", b29d_ntr); } }
static void b29d_cfg(void)
{
	FILE *f = fopen("/sdcard/b29d.txt", "r"); int a, b;
	if (f) { if (fscanf(f, "%d %d", &a, &b) == 2 && a >= 0 && b >= a) { b29d_settle_us = a; b29d_cap_us = b; } fclose(f); }
	fprintf(stderr, "[b29d] settle %d us cap %d us\r\n", b29d_settle_us, b29d_cap_us);
}

void b29d_set(const char *a) { int x, y; if (sscanf(a, "%d %d", &x, &y) == 2 && x >= 0 && y >= x) { b29d_settle_us = x; b29d_cap_us = y; } fprintf(stderr, "[b29d] settle %d us cap %d us\r\n", b29d_settle_us, b29d_cap_us); }

static void present_task(void *arg)
{
	(void)arg;
	while (!lcd_panel_ready()) vTaskDelay(pdMS_TO_TICKS(50));
	static uint16_t rects[24 * 4];
	int was_on = 0, held = 0; long last_wr = 0; int64_t last_wr_t = 0, first_dirty_t = 0;
	b29d_cfg();
	for (;;) {
		ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(held ? 2 : 8));
		int on = gfx_active();
		if (on && !was_on) { fresh_on = 1; pc_vga_refresh_gate = 0; lin_on = 1; b29l_damage_all(lp); }   /* linear mode: skip the VBE->fb conversion */
		/* one panel, one mode (owner 16 Nov): the guest surface is always 1280x800 (1:1); 1024x640 / 640x400 scaling paths are cut */
		if (!on && was_on) { pc_vga_refresh_gate = 1; lin_on = 0; }
		was_on = on;
		if (!on) continue;
		mouse_tick();
		{	/* one status line per 10 s so a hand-drag window leaves numbers in the console log */
			static int64_t last_stat; int64_t tn = esp_timer_get_time();
			if (tn - last_stat > 10000000 && !card_quiet) { last_stat = tn; fprintf(stderr, "[gfx95] vga_redraws %lu mode %dx%d fast %lu presents %lu avg_present_us %lu avg_src_px %lu rects %lu moves %ld mismatch %ld pred %ld refresh %lu flip_timeouts %lu ppa_fail %d contend %lu lasterr 0x%x held %lu settled %lu capped %lu rect %d,%d %dx%d\r\n", b29_vga_redraws, gfx.w, gfx.h, b29_fast, b29_presents, b29_pres_n ? b29_pres_us / b29_pres_n : 0, b29_pres_n ? b29_src_px / b29_pres_n : 0, b29_rects, gfx.moves, gfx.mismatch, gfx.pred_moves, b29_refresh_events, b29_flip_timeouts, b29_ppa_fail, b29_ppa_contend, b29_ppa_lasterr, b29d_held, b29d_settled, b29d_capped, b29_ppa_lastrect[0], b29_ppa_lastrect[1], b29_ppa_lastrect[2], b29_ppa_lastrect[3]); }
		}
		if (dr_overflow) { b29_ovf++; dr_overflow = 0; dr_ = dw; b29l_damage_all(lp); }
		/* The driver reports a rect when GDI STARTS drawing (BeginAccess / ExtTextOut entry); the pixels land later.  Presenting at once
		 * showed half-drawn rects that were never repainted (stale streaks).  Hold rects until the guest is idle (HLT) or 80 ms old,
		 * and repaint every rect released early once more at the next idle. */
		if (vreq) { const char *t = vreq; verify(t); vreq = NULL; vdone = 1; }
		if (areq_tag) { ascii_dump(); areq_tag = NULL; adone = 1; }
		if (pat_req >= 0) { int r = pat_req; pat_req = -1; pat_on = r; if (r) { pattern_draw(); } else { fresh_on = 1; b29l_damage_all(lp); } pat_done = 1; }
		if (pat_on) { continue; }
		if (b29_force_all) { b29_force_all--; b29l_damage_all(lp); fresh_on = 1; }
		/* Damage = what the guest actually wrote: the emulator marks every LFB write (pc.c iomem_write*), so no GDI path, XOR outline or direct write
		 * can be missed.  Tiles are cleared before the pixels are read; a write during the present re-marks its tile for the next one.  The driver's
		 * reported rects (ring) are only counted now. */
		dr_ = dw; dr_overflow = 0;
		int nd = 0;
		{
			static int16_t dt[24][4];
			int k = gfx_take_dirty(dt, 24);
			for (int i = 0; i < k; i++) { b29l_damage(lp, dt[i][0], dt[i][1], dt[i][2] + 1, dt[i][3] + 1); nd++; }
			b29_rel += k;
		}
		if (b29d_settle_us) {
			int64_t now = esp_timer_get_time(); long wr = gfx_lfb_wr;
			if (wr != last_wr) { last_wr = wr; last_wr_t = now; }
			if (nd && !held) { held = 1; first_dirty_t = now; }
			if (held && !fresh_on) {
				if (now - last_wr_t < b29d_settle_us && now - first_dirty_t < b29d_cap_us) { b29d_held++; b29d_rec(now, wr, 1); pointer_fast(); continue; }
				if (now - last_wr_t >= b29d_settle_us) { b29d_settled++; b29d_rec(now, wr, 2); } else { b29d_capped++; b29d_rec(now, wr, 3); }
			}
		}
		if (!held) { b29d_rec(esp_timer_get_time(), gfx_lfb_wr, 0); }
		int rel = held; held = 0;
		long seq = gfx.pred_moves;
		/* nothing to show -> no present, no wait.  Otherwise wait for the previous select to take effect (refresh-done) before the back buffer
		 * is written, so at most one present per refresh and never a write into the buffer that is being scanned. */
		pointer_fast();
		long sig = cur_sig();
		int work = nd || rel || (sig != last_sig) || fresh_on;
		if (!work) { continue; }
		last_sig = sig; fresh_on = 0;
		/* wait for the previous select while still serving the pointer every ~1 ms: a window drag keeps the presenter in this wait most of the time */
		for (int64_t t0 = esp_timer_get_time(); !lcd_wait_flip_ms(1); ) {
			pointer_fast(); if (esp_timer_get_time() - t0 > 70000) { lcd_flip_timeout(); break; }
		}
		sync_pointer();
		uint16_t *back = (uint16_t *)lcd_back_fb();
		uint32_t px = 0;
		int64_t tp0 = esp_timer_get_time();
		int n = b29l_present(lp, surf, back, ppa_blk, rects, &px);
		if (n > 0) { b29_pres_us += (unsigned long)(esp_timer_get_time() - tp0); b29_pres_n++; }
		if (n > 0) {
			for (int i = 0; i < n; i++) sync_panel_rect(back, &rects[i * 4]);
			lcd_swap_present();
			b29_presents++; b29_rects += n; b29_src_px += px; if (!nd) b29_cursor_only++;
			{ extern void b45_presented(long, long, int64_t); b45_presented(b29_applied_moves, seq, esp_timer_get_time()); }
			b29_applied_moves = seq;
		}
	}
}

void b29_present_start(PC *pc)
{
	static uint8_t *mem;
	mem = heap_caps_aligned_alloc(8, b29l_state_size(), MALLOC_CAP_SPIRAM);
	lp = b29l_init(mem, 2);
	surf = (const uint16_t *)(void *)pc->vga_mem;
	xTaskCreatePinnedToCore(present_task, "b29_present", 6144, NULL, 5, &ptask, 1);
	fprintf(stderr, "b29: presenter task started (state %u B)\r\n", (unsigned)b29l_state_size());
}

int b29_guest_surface(const uint8_t **p, int *w, int *h)
{
	if (!gfx_active() || !surf) return 0;
	*p = (const uint8_t *)surf; *w = mode_w; *h = mode_h; return 1;
}

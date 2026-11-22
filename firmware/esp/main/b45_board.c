/* BLUE B45 board glue: host pointer (overlay) + pointer device + scripted touch replay through the console.
 *   console:  touch NAME   replay an embedded GSL3680 trace (tools/mkinc.py <- the traces folder) at its recorded timing, through
 *                          exactly the engine call sequence of the PC test (4 ms ticks between inputs, +1 s after the last).
 *             touchstat     counters (real GSL3680 reports, INT edges, device).
 * Output line per replay:  B45R name=.. records=.. ldown=.. lup=.. rdown=.. fnv=.. golden=.. MATCH|DIFF acc_ok=.. acc_bad=..
 *                          isr=.. instr_per_isr=.. instr_per_record=.. lat_us n/p50/p99/max (touch report -> overlay in the flipped
 *                          panel buffer) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "common.h"
#include "../../pc.h"
#include "../../gfxcard.h"
#include "../../ptrdev.h"

typedef struct { uint32_t t_us; uint16_t flags, px, py, x16, y16, pad; } B45Ev;
void b29_present_kick(void);
void ptrdev_reset(void);

typedef struct { const char *name; const char *data; const char *golden; } Trace;
#include "b45_traces.inc"

volatile int b45_replay_active;
/* B45R replay latency buffers: bench-only, 24 KB, kept out of internal RAM (MEM team 16 Nov: PSRAM; written ~100 Hz) */
static EXT_RAM_BSS_ATTR int64_t lat_t0[1024];
#define LATN 4096
static EXT_RAM_BSS_ATTR uint32_t lat[LATN]; static volatile int nlat;

/* host pointer: the overlay jumps to the engine position at once (B29a present task draws it) */
static void ovl_follow(void)
{
	static uint32_t seen;
	uint32_t sq, v = ptrdev_overlay(&sq);
	if (sq == seen) return;
	seen = sq;
	if (!gfx_active()) return;
	gfx_pred_abs(v & 0xffff, v >> 16);
	lat_t0[gfx.pred_moves & 1023] = esp_timer_get_time();
	b29_present_kick();
}
void b45_host_touch(uint32_t t, int down, int rx, int ry) { ptrdev_touch(t, down, rx, ry); ovl_follow(); }
void b45_host_tick(uint32_t t) { ptrdev_tick(t); ovl_follow(); }
void b45_host_rel(uint32_t t, int dx, int dy, int b) { ptrdev_rel(t, dx, dy, b); ovl_follow(); }
/* B70 entry (lead addendum): exact absolute pointer + buttons to Windows, host overlay follows at once */
void set_pointer_abs(int x, int y, int buttons) { ptrdev_set_abs((uint32_t)esp_timer_get_time(), x, y, buttons); ovl_follow(); }

/* present task: moves [from+1 .. to] are now in the flipped panel buffer */
void b45_presented(long from, long to, int64_t now)
{
	for (long s = from + 1; s <= to && s - from < 1024; s++) {
		int64_t t0 = lat_t0[s & 1023];
		if (t0) { lat_t0[s & 1023] = 0; if (nlat < LATN) lat[nlat++] = (uint32_t)(now - t0); }
	}
}

/* guest side, called by ptrdev in the emulator task with the device lock held */
static B45Ev last; static long n_rec, n_ld, n_lu, n_rd, acc_ok, acc_bad, pxerr_max; static uint64_t fnv;
static void fnv_add(const void *p, int n) { const uint8_t *b = p; for (int i = 0; i < n; i++) { fnv ^= b[i]; fnv *= 0x100000001b3ULL; } }
static void on_rec(const void *v)
{
	const B45Ev *e = v; last = *e; n_rec++;
	if (e->flags & 2) { n_ld++; }
	if (e->flags & 4) { n_lu++; }
	if (e->flags & 8) { n_rd++; }
	fnv_add(&e->t_us, 4); fnv_add(&e->flags, 2); fnv_add(&e->x16, 2); fnv_add(&e->y16, 2);
}
static void on_done(long instr)
{
	(void)instr;
	if (last.flags & 0x1e) {
		long ex = labs((long)gfx.cx - (long)last.px * gfx.w / 1280), ey = labs((long)gfx.cy - (long)last.py * gfx.h / 800), m = ex > ey ? ex : ey;   /* guest pixels (GFX95 mode 1:1 / 1.25 / 2) */
		if (m > pxerr_max) pxerr_max = m;
		if (!m) acc_ok++; else acc_bad++;
	}
}

static int cmpu(const void *a, const void *b) { uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b; return x < y ? -1 : x > y; }
static void wait_until(int64_t t) { for (;;) { int64_t d = t - esp_timer_get_time(); if (d <= 0) return; if (d > 2000) vTaskDelay(1); } }

static void replay(const Trace *tr)
{
	b45.log = on_rec; b45.done = on_done;
	b45_replay_active = 1;
	vTaskDelay(pdMS_TO_TICKS(100));
	ptrdev_reset();
	n_rec = n_ld = n_lu = n_rd = acc_ok = acc_bad = pxerr_max = 0; fnv = 0xcbf29ce484222325ULL; nlat = 0;
	long isr0 = b45.isr_n, ins0 = b45.isr_instr, pop0 = b45.popped;
	long c0 = cpui386_get_cycle(((PC *)globals.pc)->cpu);
	int64_t base = esp_timer_get_time();
	const char *p = tr->data; uint32_t prev = 0, nb = 0; int first = 1, lines = 0;
	while (*p) {
		char k = *p; int t, a, b, c;
		if ((k == 'T' || k == 'R' || k == 'A') && sscanf(p + 1, "%d %d %d %d", &t, &a, &b, &c) == 4) {
			uint32_t tin = (uint32_t)t * 1000;
			if (!first) for (; nb < tin; nb += 4000) { wait_until(base + nb); b45_host_tick(nb); }
			first = 0;
			wait_until(base + tin);
			if (k == 'T') b45_host_touch(tin, a, b, c); else if (k == 'A') { ptrdev_set_abs(tin, a, b, c); ovl_follow(); } else b45_host_rel(tin, a, b, c);
			prev = tin; nb = (tin / 4000 + 1) * 4000; lines++;
		}
		while (*p && *p != '\n') p++;
		if (*p) p++;
	}
	for (; nb < prev + 1000000; nb += 4000) { wait_until(base + nb); b45_host_tick(nb); }
	vTaskDelay(pdMS_TO_TICKS(300));   /* guest drains the last records */
	long instr = cpui386_get_cycle(((PC *)globals.pc)->cpu) - c0;
	b45_replay_active = 0;
	int n = nlat; qsort(lat, n, sizeof lat[0], cmpu);
	long isr = b45.isr_n - isr0, ins = b45.isr_instr - ins0, recs = b45.popped - pop0;
	char fx[20]; snprintf(fx, sizeof fx, "%016llx", (unsigned long long)fnv);
	fprintf(stderr, "B45R name=%s lines=%d wall=%.2fs records=%ld ldown=%ld lup=%ld rdown=%ld fnv=%s golden=%s %s acc_ok=%ld acc_bad=%ld pxerr_max=%ld "
		"isr=%ld instr_per_isr=%ld instr_per_record=%ld guest_instr_total=%ld lat_us n=%d p50=%lu p99=%lu max=%lu dropped=%ld\r\n",
		tr->name, lines, (esp_timer_get_time() - base) / 1e6, n_rec, n_ld, n_lu, n_rd, fx, tr->golden, strcmp(fx, tr->golden) ? "DIFF" : "MATCH",
		acc_ok, acc_bad, pxerr_max, isr, isr ? ins / isr : 0, recs ? ins / recs : 0, instr,
		n, (unsigned long)(n ? lat[n / 2] : 0), (unsigned long)(n ? lat[(n * 99) / 100] : 0), (unsigned long)(n ? lat[n - 1] : 0), b45.dropped);
}

static char want[32];
static void bench_task(void *a)
{
	(void)a;
	if (!strcmp(want, "all")) { for (int i = 0; i < (int)(sizeof traces / sizeof traces[0]); i++) replay(&traces[i]); }
	else {
		int hit = 0;
		for (int i = 0; i < (int)(sizeof traces / sizeof traces[0]); i++) if (!strcmp(traces[i].name, want)) { replay(&traces[i]); hit = 1; }
		if (!hit) { fprintf(stderr, "B45R unknown trace %s; have:", want); for (int i = 0; i < (int)(sizeof traces / sizeof traces[0]); i++) fprintf(stderr, " %s", traces[i].name); fprintf(stderr, "\r\n"); }
	}
	fprintf(stderr, "B45R done %s\r\n", want);
	vTaskDelete(NULL);
}
/* console (cardsvc listener, core 0): replay runs in its own task on core 1 (same core as the emulator and the presenter) */
void b45_console_touch(const char *name)
{
	if (b45_replay_active) { fprintf(stderr, "B45R busy\r\n"); return; }
	if (!globals.pc) { fprintf(stderr, "B45R no guest\r\n"); return; }
	snprintf(want, sizeof want, "%s", name);
	xTaskCreatePinnedToCore(bench_task, "b45_replay", 6144, NULL, 4, NULL, 1);
}
extern volatile unsigned long gsl_reports, gsl_errs, gsl_ints; extern volatile int gsl_ok;
void b45_console_stat(void)
{
	fprintf(stderr, "B45S gsl_ok=%d gsl_reports=%lu gsl_ints=%lu gsl_errs=%lu enabled=%d enables=%ld raises=%ld popped=%ld isr=%ld isr_instr=%ld dropped=%ld cur=%d,%d gfx_active=%d\r\n",
		gsl_ok, gsl_reports, gsl_ints, gsl_errs, b45.enabled, b45.enables, b45.raises, b45.popped, b45.isr_n, b45.isr_instr, b45.dropped, gfx.cx, gfx.cy, gfx_active());
}

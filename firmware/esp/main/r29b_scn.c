/* RED B29b board scenario runner (console "scn"; "nat 0|1" switches the native DDI executor; off the hot path, core 0).
 * Same key sequences as the PC script (lab:RED/B29b/tools/mkscr.py: S2 Start menu x5, S3 Control Panel, S4 Explorer, S5 Notepad keyboard
 * drag 40 steps, S6 File Open dialog), but paced by the guest instead of a fixed timeline: after every key the runner waits until the guest
 * has SETTLED (no LFB write for 800 ms and guest busy < 10 % in that window, max 120 s).  Per phase it prints wall ms from the first key to the
 * last LFB write, guest instructions and busy ms:  "[scn] S3 wall_ms=.. instr=.. busy_ms=.. lfb_bytes=.. native=.."  */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "../../pc.h"
#include "../../i8042.h"
#include "../../gfxcard.h"
#include "common.h"

extern int gfx_ddi_native;
extern volatile uint32_t x86_busy_pub_us;
static int64_t t_last_draw; static long lfb_seen; void b29d_trace(int);

/* drawing = more than 2 KB of LFB writes since the last 5 ms poll (a caret blink is ~60 B per 530 ms and must not count) */
static void watch(void) { long w = gfx_lfb_wr; if (w - lfb_seen > 2048) t_last_draw = esp_timer_get_time(); lfb_seen = w; }
__attribute__((unused)) static void settle(void)
{
	int64_t t0 = esp_timer_get_time(); uint32_t b0 = x86_busy_pub_us; int64_t tw = t0;
	for (;;) {
		vTaskDelay(pdMS_TO_TICKS(5)); watch();
		int64_t now = esp_timer_get_time();
		if (now - t0 > 60000000) { fprintf(stderr, "[scn] settle timeout\r\n"); return; }
		if (now - tw >= 800000) {
			uint32_t b1 = x86_busy_pub_us; int quiet = now - t_last_draw >= 800000, idle = (b1 - b0) < 80000;
			if (quiet && idle) return;
			tw = now; b0 = b1;
		}
	}
}
static void key(int code) { ps2_put_keycode(globals.kbd, 1, code); vTaskDelay(pdMS_TO_TICKS(40)); ps2_put_keycode(globals.kbd, 0, code); vTaskDelay(pdMS_TO_TICKS(40)); }
static void combo(int mod, int code) { ps2_put_keycode(globals.kbd, 1, mod); vTaskDelay(pdMS_TO_TICKS(50)); key(code); ps2_put_keycode(globals.kbd, 0, mod); vTaskDelay(pdMS_TO_TICKS(50)); }
static const char *SC = "abcdefghijklmnopqrstuvwxyz";
static const uint8_t SCC[26] = { 0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c };
static void typ(const char *s)
{
	for (; *s; s++) {
		const char *p = strchr(SC, *s);
		static const uint8_t DG[10] = { 0x0b, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a };
		if (*s >= 'A' && *s <= 'Z') { combo(0x2a, SCC[*s - 'A']); continue; }
		if (*s >= '0' && *s <= '9') { key(DG[*s - '0']); continue; }
		if (*s == ' ') { key(0x39); continue; }
		if (*s == '\n') { key(0x1c); continue; }
		if (*s == '!') { combo(0x2a, 0x02); continue; }
		if (*s == ',') { key(0x33); continue; }
		if (p) key(SCC[p - SC]); else if (*s == '.') key(0x34); else if (*s == '\\') key(0x2b); else if (*s == '*') combo(0x2a, 0x09); else if (*s == ':') combo(0x2a, 0x27);
	}
}
typedef struct { const char *n; int64_t t0; long c0, w0, nat0, fb0, fbi0; uint32_t b0; } Ph;
static long nat_total(void) { long n = 0; if (!gfx_ddi) return 0; for (int i = 0; i < GFX_DDI_N; i++) n += gfx_ddi[i].native; return n; }
/* GFX B29b: DIBENG (x86) fallback calls; fb_total excludes the pointer exports SetCursor/MoveCursor/CheckCursor (slots 33-35, B45/P4PTR path),
 * fb_instr = guest instructions spent inside those fallbacks */
static long fb_total(void) { long n = 0; if (!gfx_ddi) return 0; for (int i = 0; i < GFX_DDI_N; i++) if (i < 33 || i > 35) n += gfx_ddi[i].calls - gfx_ddi[i].native; return n; }
static long fb_instr(void) { long n = 0; if (!gfx_ddi) return 0; for (int i = 0; i < GFX_DDI_N; i++) if (i < 33 || i > 35) n += gfx_ddi[i].incl; return n; }
static void ph_begin(Ph *p, const char *n) { PC *pc = globals.pc; watch(); p->n = n; p->t0 = esp_timer_get_time(); p->c0 = cpui386_get_cycle(pc->cpu); p->w0 = gfx_lfb_wr; p->b0 = x86_busy_pub_us; p->nat0 = nat_total(); p->fb0 = fb_total(); p->fbi0 = fb_instr(); }
static void ph_end(Ph *p)
{
	PC *pc = globals.pc;
	fprintf(stderr, "[scn] %s wall_ms=%lld instr=%ld busy_ms=%lu lfb_bytes=%ld native_calls=%ld fb_calls=%ld fb_instr=%ld nat=%d\r\n", p->n, (long long)((t_last_draw > p->t0 ? t_last_draw : p->t0) - p->t0) / 1000,
		cpui386_get_cycle(pc->cpu) - p->c0, (unsigned long)((x86_busy_pub_us - p->b0) / 1000), gfx_lfb_wr - p->w0, nat_total() - p->nat0, fb_total() - p->fb0, fb_instr() - p->fbi0, gfx_ddi_native);
}
/* outside the measured phases: keep the opened window on the glass for the webcam (frames arrive ~5 s after a trigger) */
static void hold(const char *n) { fprintf(stderr, "[scn] HOLD %s\r\n", n); vTaskDelay(pdMS_TO_TICKS(9000)); }
/* v2 (lead 16 Nov): FIXED timeline per phase (no settle wait: a busy guest can no longer stall the run), measured inside the window:
 * wall_ms = first key -> last real draw (> 2 KB of LFB writes per 5 ms poll), busy_ms = guest busy in the window, instr.  Three repeats per
 * boot (rep 1 cold, 2-3 warm).  S5 = 40 keyboard move steps at 250 ms pace: steps/s = 40 / (last draw - first step key). */
static void wait_ms(int ms) { int64_t e = esp_timer_get_time() + (int64_t)ms * 1000; while (esp_timer_get_time() < e) { vTaskDelay(pdMS_TO_TICKS(5)); watch(); } }
static int scn_reps = 3;   /* console "scn N" (GFX B29b: 2 reps + paint fit one 20-min slot) */
static void scn_run(void)
{
	Ph p; wait_ms(1000);
	fprintf(stderr, "[scn] start v2 nat=%d\r\n", gfx_ddi_native);
	{ extern void gfx_ddi_report(FILE *); gfx_ddi_report(stderr); }
	for (int rep = 1; rep <= scn_reps; rep++) {
		fprintf(stderr, "[scn] rep %d\r\n", rep);
		ph_begin(&p, "S2_startmenu_x5"); for (int i = 0; i < 5; i++) { combo(0x1d, 0x01); wait_ms(1500); key(0x01); wait_ms(1500); } ph_end(&p);
		ph_begin(&p, "S3_controlpanel_open"); combo(0x1d, 0x01); wait_ms(1500); key(0x1f); wait_ms(1500); key(0x2e); wait_ms(75000); ph_end(&p);
		if (rep == 1) hold("S3");
		ph_begin(&p, "S3_close"); combo(0x38, 0x3e); wait_ms(5000); ph_end(&p);
		ph_begin(&p, "S4_explorer_open"); combo(0x1d, 0x01); wait_ms(1500); key(0x13); wait_ms(2000); typ("explorer"); key(0x1c); wait_ms(40000); ph_end(&p);
		if (rep == 1) hold("S4");
		ph_begin(&p, "S4_close"); combo(0x38, 0x3e); wait_ms(5000); ph_end(&p);
		ph_begin(&p, "notepad_open"); combo(0x1d, 0x01); wait_ms(1500); key(0x13); wait_ms(2000); typ("notepad"); key(0x1c); wait_ms(25000); ph_end(&p);
		combo(0x38, 0x39); wait_ms(1500); key(0x32); wait_ms(1500);
		ph_begin(&p, "S5_drag40"); int64_t td = esp_timer_get_time();
		for (int i = 0; i < 40; i++) { key(i & 1 ? 0xe04d : 0xe050); wait_ms(170); }
		wait_ms(3000);
		fprintf(stderr, "[scn] S5 steps_per_s=%.2f\r\n", t_last_draw > td ? 40e6 / (double)(t_last_draw - td) : 0.0);
		ph_end(&p); key(0x1c); wait_ms(3000);
		if (rep == 1) hold("S5");
		ph_begin(&p, "S6_fileopen"); combo(0x38, 0x21); wait_ms(1500); key(0x18); wait_ms(6000); typ("c:\windows\*.*"); key(0x1c); wait_ms(25000); ph_end(&p);
		if (rep == 1) hold("S6");
		ph_begin(&p, "S6_close"); key(0x01); wait_ms(3000); combo(0x38, 0x3e); wait_ms(5000); ph_end(&p);
		{ extern void gfx_ddi_report(FILE *); gfx_ddi_report(stderr); }
	}
	fprintf(stderr, "[scn] DONE\r\n");
}
/* owner demo video (lead 16 Nov): Start menu, Control Panel, Notepad typing - about 90 s */
static void demo_run(void)
{
	wait_ms(1000); fprintf(stderr, "[demo] start\r\n");
	for (int i = 0; i < 2; i++) { combo(0x1d, 0x01); wait_ms(2000); key(0x01); wait_ms(1500); }
	fprintf(stderr, "[demo] controlpanel\r\n"); combo(0x1d, 0x01); wait_ms(1500); key(0x1f); wait_ms(1500); b29d_trace(1); key(0x2e); wait_ms(6000); b29d_trace(0); wait_ms(24000);
	combo(0x38, 0x3e); wait_ms(3000);
	fprintf(stderr, "[demo] notepad\r\n"); combo(0x1d, 0x01); wait_ms(1500); key(0x13); wait_ms(2000); typ("notepad"); key(0x1c); wait_ms(8000);
	typ("Hello from Windows 95 on the ESP32 P4!\nTyping in Notepad, live on the board.\n"); wait_ms(1000);
	typ("Old body, new engine. 1995 to 2026\n"); wait_ms(5000);
	fprintf(stderr, "[demo] DONE\r\n");
}
/* B29b Paint bug (owner 19 Nov): Start > Run > pbrush, click a palette swatch (red), draw a line; HOLD lines let the runner take guest/panel shots */
static void ab(int x, int y, int b) { extern void set_pointer_abs(int, int, int); set_pointer_abs(x, y, b); vTaskDelay(pdMS_TO_TICKS(40)); }
static __attribute__((unused)) void mv(int dx, int dy, int b)
{
	extern int gfx_active(void); extern void b29_mouse_in(int, int, int);
	while (dx || dy) { int sx = dx > 20 ? 20 : dx < -20 ? -20 : dx, sy = dy > 20 ? 20 : dy < -20 ? -20 : dy; b29_mouse_in(sx, sy, b); dx -= sx; dy -= sy; vTaskDelay(pdMS_TO_TICKS(15)); }
	b29_mouse_in(0, 0, b); vTaskDelay(pdMS_TO_TICKS(15));
}
static void paint_run(void)
{
	wait_ms(1000); fprintf(stderr, "[paint] start nat=%d\r\n", gfx_ddi_native);
	Ph p; ph_begin(&p, "P_launch");
	combo(0x1d, 0x01); wait_ms(1500); key(0x13); wait_ms(2000); typ("pbrush"); key(0x1c); wait_ms(25000); ph_end(&p);
	fprintf(stderr, "[scn] HOLD P_launch\r\n"); wait_ms(45000);
	ph_begin(&p, "P_draw");
	ab(0, 0, 0); wait_ms(500); ab(70, 350, 0); wait_ms(500); ab(70, 350, 1); wait_ms(150); ab(70, 350, 0); wait_ms(1500);   /* red swatch */
	ab(120, 150, 0); wait_ms(500); ab(120, 150, 1); wait_ms(200); for (int i = 1; i <= 10; i++) ab(120 + 12 * i, 150 + 6 * i, 1); ab(240, 210, 0); wait_ms(3000);   /* line */
	ph_end(&p);
	fprintf(stderr, "[scn] HOLD P_drawn\r\n"); wait_ms(45000);
	{ extern void gfx_ddi_report(FILE *); gfx_ddi_report(stderr); }
	fprintf(stderr, "[paint] DONE\r\n");
}
static void paint_task(void *a) { (void)a; paint_run(); vTaskDelete(NULL); }
static void demo_task(void *a) { (void)a; demo_run(); vTaskDelete(NULL); }
static void scn_task(void *a) { (void)a; scn_run(); vTaskDelete(NULL); }
/* B29d: window drag at keyboard pace + open/close of two windows, with presenter counters before/after (console "b29drag") */
extern volatile unsigned long b29_presents, b29_refresh_events, b29_fast; void b29d_trace(int);
static void b29drag_run(void)
{
	wait_ms(1000); fprintf(stderr, "[b29drag] start\r\n");
	combo(0x1d, 0x01); wait_ms(1500); key(0x13); wait_ms(2000); typ("notepad"); key(0x1c); wait_ms(12000);
	combo(0x38, 0x39); wait_ms(1500); key(0x32); wait_ms(1500);
	unsigned long p0 = b29_presents, r0 = b29_refresh_events; int64_t t0 = esp_timer_get_time();
	for (int i = 0; i < 60; i++) { key(i & 1 ? 0xe04d : 0xe050); wait_ms(60); }
	int64_t t1 = esp_timer_get_time(); wait_ms(1500); key(0x1c); wait_ms(1500);
	fprintf(stderr, "[b29drag] drag60 ms=%lld presents=%lu refresh=%lu\r\n", (long long)(t1 - t0) / 1000, b29_presents - p0, b29_refresh_events - r0);
	for (int rep = 0; rep < 4; rep++) {
		p0 = b29_presents; r0 = b29_refresh_events;
		combo(0x1d, 0x01); wait_ms(1500); key(0x1f); wait_ms(1500); if (!rep) b29d_trace(1); key(0x2e); wait_ms(7000); if (!rep) b29d_trace(0);
		fprintf(stderr, "[b29drag] cpopen%d presents=%lu refresh=%lu\r\n", rep, b29_presents - p0, b29_refresh_events - r0);
		combo(0x38, 0x3e); wait_ms(3500);
	}
	fprintf(stderr, "[b29drag] DONE\r\n");
}
static void b29drag_task(void *a) { (void)a; b29drag_run(); vTaskDelete(NULL); }
void r29b_console(const char *l)
{
	if (!strcmp(l, "b29drag")) { if (globals.kbd) xTaskCreatePinnedToCore(b29drag_task, "scn", 6144, NULL, 2, NULL, 0); return; }
	if (!strcmp(l, "demo")) { if (globals.kbd) xTaskCreatePinnedToCore(demo_task, "scn", 6144, NULL, 2, NULL, 0); }
	else if (!strcmp(l, "paint")) { if (globals.kbd) xTaskCreatePinnedToCore(paint_task, "scn", 6144, NULL, 2, NULL, 0); }
	else if (!strncmp(l, "scn ", 4)) { scn_reps = atoi(l + 4); if (scn_reps < 1 || scn_reps > 3) scn_reps = 3; if (globals.kbd) xTaskCreatePinnedToCore(scn_task, "scn", 6144, NULL, 2, NULL, 0); }
	else if (!strcmp(l, "scn")) { if (globals.kbd) xTaskCreatePinnedToCore(scn_task, "scn", 6144, NULL, 2, NULL, 0); }
	else if (!strncmp(l, "ppa ", 4)) { extern int gfx_ppa_min_px; extern long gfx_ppa_fills; gfx_ppa_min_px = atoi(l + 4); fprintf(stderr, "[scn] ppa_min_px=%d fills=%ld\r\n", gfx_ppa_min_px, gfx_ppa_fills); }
	else if (!strcmp(l, "gfxbench")) { extern void gfx_bench(void); gfx_bench(); }
	else if (!strncmp(l, "gk ", 3)) { extern int gfx_k_pie; gfx_k_pie = l[3] - '0'; fprintf(stderr, "[scn] gk=%d\r\n", gfx_k_pie); }
	else if (!strcmp(l, "ddi")) { extern void gfx_ddi_report(FILE *); gfx_ddi_report(stderr); }
	else if (!strncmp(l, "nat ", 4)) { gfx_ddi_native = l[4] - '0';   /* 0 DIBENG, 1 all native, 2 native except ExtTextOut (B29b A/B) */ fprintf(stderr, "[scn] nat=%d\r\n", gfx_ddi_native); }
}

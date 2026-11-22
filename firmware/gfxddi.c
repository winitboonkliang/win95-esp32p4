/* RED B29b: host side of the GFX95 driver's DDI stubs (included by gfxcard.c; gfx95/driver/ddi.inc is the guest side).
 * Every exported DDI call reaches ddi_enter() (card index 0x50 = ordinal) with its pascal arguments on the guest stack.  The host
 * either executes it natively (status 1: stub returns at once with DX:AX = ddi_result) or lets the old body run (status 0, counted
 * fallback; ddi_exit() = 0x51 when it returns).  Per-ordinal counters: calls, inclusive/exclusive guest instructions, LFB bytes,
 * native calls.  Native executors live in gfxnat.c. */
#include <stdlib.h>
#include <limits.h>
#include "i386.h"
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#define DDI_ALLOC(n) heap_caps_calloc(1, (n), MALLOC_CAP_SPIRAM)   /* PSRAM: no new internal RAM (rule: internal needs lead approval) */
#else
#define DDI_ALLOC(n) calloc(1, (n))
#endif

GfxDdi *gfx_ddi; int gfx_ddi_n = GFX_DDI_N; long gfx_lfb_wr;
static int ddi_status, ddi_nargs; static uint32_t ddi_result;
int gfx_ddi_native = 1;      /* 0 = every call falls back (oracle / profile build), 1 = native where implemented */
int gfx_ddi_verify;          /* PC: compute natively into a scratch copy, let DIBENG draw, compare at exit (per-call oracle) */
static int ddi_log;          /* PC: env B29B_LOG=1 prints slow fallback calls */
static PC *dpc;
extern long *nat_rej;
static int ddi_slot(int ord) { if (ord >= 0 && ord <= 31) return ord; if (ord >= 101 && ord <= 104) return ord - 69; if (ord == 500) return 36; if (ord == 700) return 37; return 38; }

typedef struct { int ord, v; long c0, w0, child, wchild; uint8_t a[64]; int ps, pn; uint32_t pr; uint16_t pi; uint16_t rip, rcs; int rok; } DdiFrame;   /* rip:rcs = the caller's far return as the stub entered (B61j) */
/* Interrupt-time exports (MoveCursor/CheckCursor from USER's mouse/timer code) can enter the stub while an interrupted stub sits between
 * its index write and its data read, or between ddi_enter and reading status/nargs/result.  A fallback call saves that state at entry
 * (index as it was BEFORE its own 0x50 index write) and restores it at its exit event, so the interrupted stub reads its own values.
 * (Board P1 crash 16 Nov 14:40: the interrupted native BitBlt read nargs/result of the nested call -> wrong stack drop.) */
/* GFX B29b touch list: pages a native call needs that are not present (status 2 -> guest stub touches them in ring 3, asks again) */
typedef struct { uint16_t sel, fl; uint32_t off; } DdiTouch;
static DdiTouch ddi_tl[32]; static int ddi_tn, ddi_tr, ddi_trounds, ddi_touch_ok;   /* touch_ok: entered via index 0x5a (a driver that understands status 2) */ long gfx_touch_rounds, gfx_touch_pages;
static void ddi_touch_add(uint16_t sel, uint32_t off, int wr)
{
	off &= ~4095u;
	for (int i = 0; i < ddi_tn; i++) if (ddi_tl[i].sel == sel && ddi_tl[i].off == off) { ddi_tl[i].fl |= wr ? 3 : 1; return; }
	if (ddi_tn < 32) { ddi_tl[ddi_tn].sel = sel; ddi_tl[ddi_tn].off = off; ddi_tl[ddi_tn].fl = wr ? 3 : 1; ddi_tn++; }
}
long ddi_ret_fixed;   /* B61j: fallback returns the host put right (interrupt-time call inside the stub's D_PSTACK push) */
static uint16_t gfx_prev_index; static int ddi_open;   /* a native stub has not read its result yet (nat_rej[29] counts calls nested into it) */
static DdiFrame *ddi_stk; static int ddi_sp;   /* 64 frames, PSRAM */

/* ---- guest memory: linear address -> host byte pointer (valid to the end of its 4 KB page); NULL = not present / not RAM ---- */
static uint8_t *g_host(uword lin, int wr)
{
	long pl = cpui386_lin2phys_user(dpc->cpu, lin, wr);
	if (pl == -1) { if (nat_rej && cpui386_lin2phys(dpc->cpu, lin, wr) != -1) nat_rej[28]++; return NULL; }   /* 28: page DIBENG (ring 3) could not touch either */   /* -1 = fault; NOT "< 0": on the 32-bit board long is 32 bits and the LFB (>= 2 GB physical) is negative */
	uword p = (uword)pl;
	if (p < (uword)dpc->phys_mem_size) return (uint8_t *)dpc->phys_mem + p;
	uword o = p - dpc->pci_vga_ram_addr;
	if (p >= dpc->pci_vga_ram_addr && o < (uword)dpc->vga_mem_size) return (uint8_t *)dpc->vga_mem + o;
	return NULL;
}
static int g_read(uword lin, void *buf, int n)
{
	uint8_t *d = buf;
	while (n > 0) { uint8_t *h = g_host(lin, 0); if (!h) return 0; int k = 4096 - (lin & 4095); if (k > n) k = n; memcpy(d, h, k); d += k; lin += k; n -= k; }
	return 1;
}
static int g_lin(uint32_t fp, uword *lin, uword need)   /* far pointer sel:off -> linear; need = bytes that must lie inside the limit */
{
	uword base, limit; if (!cpui386_sel_info(dpc->cpu, fp >> 16, &base, &limit)) return 0;
	if ((fp & 0xffff) + (need ? need - 1 : 0) > limit) return 0;
	*lin = base + (fp & 0xffff); return 1;
}
static inline uint16_t rd16(const uint8_t *p) { return p[0] | (p[1] << 8); }
static inline uint32_t rd32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

#define NAT_MAXW 2048
static uint8_t *nat_rowbuf; extern long *nat_rej;
static void ddi_dev_str(uint32_t fp, char *s, int n);
#include "gfxnat.c"

static void ddi_enter(PC *pc, int ord)
{
	int ps = ddi_status, pn = ddi_nargs; uint32_t pr = ddi_result; uint16_t pi = gfx_prev_index;
	if (ddi_open && nat_rej) nat_rej[29]++;
	dpc = pc; ddi_status = 0; ddi_nargs = 0; ddi_result = 0;
	static int init; if (!init) { init = 1; gfx_ddi = DDI_ALLOC(GFX_DDI_N * sizeof *gfx_ddi); ddi_stk = DDI_ALLOC(64 * sizeof *ddi_stk); nat_rowbuf = DDI_ALLOC(NAT_MAXW * 2 + 8); nat_rej = DDI_ALLOC(64 * sizeof(long)); const char *e = getenv("B29B_LOG"); ddi_log = e ? atoi(e) : 0; e = getenv("B29B_NATIVE"); if (e) gfx_ddi_native = atoi(e); e = getenv("B29B_VERIFY"); if (e) gfx_ddi_verify = atoi(e); }
	if (!gfx_ddi || !ddi_stk || !nat_rowbuf) return;   /* no PSRAM: every call falls back, uncounted */
	GfxDdi *e = &gfx_ddi[ddi_slot(ord)]; e->calls++;
	uint32_t st[32]; cpui386_get_state(pc->cpu, st);
	uword sp = st[11] + ((st[4] + 12) & st[15]);   /* SS base + SP after the stub's 6 pushes: far return at +0, arguments at +4 */
	uint8_t a[64]; int have = g_read(sp + 4, a, sizeof a);
	if (have && (gfx_ddi_native || gfx_ddi_verify)) {
		long w0 = gfx_lfb_wr;
		ddi_tn = 0; ddi_tr = 0;
		int r = nat_call(ord, a, gfx_ddi_verify ? 1 : 0);
		if (r <= 0 && ddi_tn && ddi_touch_ok && ddi_trounds < 4 && gfx_ddi_native) {   /* status 2: touch and ask again (not a call of its own) */
			e->calls--; ddi_trounds++; gfx_touch_rounds++; gfx_touch_pages += ddi_tn; ddi_status = 2; return;
		}
		if (r <= 0 && ddi_tn && ddi_log >= 2) fprintf(stderr, "TOUCH GIVEUP ord %d pages %d sel %04x off %08x fl %d\n", ord, ddi_tn, ddi_tl[0].sel, (unsigned)ddi_tl[0].off, ddi_tl[0].fl);
		ddi_trounds = 0;
		if (r > 0 && !gfx_ddi_verify) { e->native++; e->native_wr += gfx_lfb_wr - w0; ddi_status = 1; ddi_nargs = r; ddi_open = 1; return; }
	}
	if (ddi_sp < 64) { DdiFrame *f = &ddi_stk[ddi_sp]; f->ord = ord; f->c0 = cpui386_get_cycle(pc->cpu); f->w0 = gfx_lfb_wr; f->child = f->wchild = 0; if (have) memcpy(f->a, a, 64); f->v = have; f->ps = ps; f->pn = pn; f->pr = pr; f->pi = pi; uint8_t r4[4]; f->rok = g_read(sp, r4, 4); f->rip = rd16(r4); f->rcs = rd16(r4 + 2); }
	ddi_sp++;
}

#ifdef ESP_PLATFORM
static void ddi_log_call(void *f, long d) { (void)f; (void)d; }
#else
static void ddi_log_call(DdiFrame *f, long d);
static void ddi_log_arg(DdiFrame *f, long d, uint32_t ret);
#endif
static void ddi_exit(PC *pc, int v)
{
	dpc = pc;
	if (!ddi_stk || ddi_sp <= 0) return;
	if (--ddi_sp >= 64) return;
	DdiFrame *f = &ddi_stk[ddi_sp];
	ddi_status = f->ps; ddi_nargs = f->pn; ddi_result = f->pr; gfx.index = f->pi;   /* state of an interrupted stub (see DdiFrame) */
	if (!v) return;
	/* B61j: the stub keeps the caller's return in its own D_PSTACK; its push (ddi.inc: store entry, then D_PSP += 4) is not atomic, so an
	 * interrupt-time call (CheckCursor/MoveCursor) landing between the two reuses the same slot and the interrupted call later returns to the
	 * NESTED caller. This host frame stack has the true return of every fallback call (read before the stub replaced it): ddi_ret is about to
	 * lret through its frame (ss:sp+10 ip, +12 cs at this exit event), so the host puts the true return there. */
	if (f->rok) { uint32_t st[32]; cpui386_get_state(pc->cpu, st); uword sp = st[11] + ((st[4] + 10) & st[15]); uint8_t g[4];
	  if (g_read(sp, g, 4) && (rd16(g) != f->rip || rd16(g + 2) != f->rcs)) {
		uint8_t t[4] = { (uint8_t) f->rip, (uint8_t)(f->rip >> 8), (uint8_t) f->rcs, (uint8_t)(f->rcs >> 8) }; int ok = 1;
		for (int k = 0; k < 4 && ok; k++) { uint8_t *h = g_host(sp + k, 1); if (h) *h = t[k]; else ok = 0; }
		ddi_ret_fixed++; if (ddi_ret_fixed <= 8) fprintf(stderr, "[gfx] DDI return repaired: ord %d stub had %04x:%04x, true %04x:%04x (%s)\r\n", f->ord, rd16(g + 2), rd16(g), f->rcs, f->rip, ok ? "fixed" : "page absent");
	  } }
	long d = cpui386_get_cycle(pc->cpu) - f->c0, w = gfx_lfb_wr - f->w0; GfxDdi *e = &gfx_ddi[ddi_slot(f->ord)];
	e->incl += d; e->excl += d - f->child; e->wr += w - f->wchild;
	if (ddi_sp > 0 && ddi_sp <= 64) { ddi_stk[ddi_sp - 1].child += d; ddi_stk[ddi_sp - 1].wchild += w; }
	if (gfx_ddi_verify && f->v) { uint32_t st[32]; cpui386_get_state(pc->cpu, st); uword sp = st[11] + (st[4] & st[15]); uint8_t rr[8]; g_read(sp, rr, 8); int m = nat_verify(f->ord, f->a, rd16(rr + 6) | (uint32_t)rd16(rr + 2) << 16);   /* ddi_ret frame: es, dx, bx, ax -> DX:AX (was BX:AX) */ if (m > 0) e->verified++; else if (m < 0) e->mismatch++; }
	if (ddi_log && f->v) ddi_log_call(f, d);
#ifndef ESP_PLATFORM
	if (ddi_log >= 4 && f->v) { uint32_t st[32]; cpui386_get_state(pc->cpu, st); uword sp = st[11] + (st[4] & st[15]); uint8_t rr[8] = {0}; g_read(sp, rr, 8); ddi_log_arg(f, d, rd16(rr + 6) | (uint32_t)rd16(rr + 4) << 16); }
#endif
}

#ifndef ESP_PLATFORM
/* ---- PC diagnostics: describe a device / bitmap far pointer ---- */
static void ddi_dev_str(uint32_t fp, char *s, int n)
{
	uword l; uint8_t h[0x30];
	if (!fp) { snprintf(s, n, "null"); return; }
	if (!g_lin(fp, &l, 0x20) || !g_read(l, h, 0x30)) { snprintf(s, n, "%08x?", (unsigned)fp); return; }
	if (rd16(h) == 0x5250) snprintf(s, n, "DIB%d %dx%d fl%04x", h[9], rd16(h + 2), rd16(h + 4), rd16(h + 0x1c));
	else snprintf(s, n, "BM%d.%d %dx%d t%04x", h[8], h[9], rd16(h + 2), rd16(h + 4), rd16(h));
}
/* GFX B29b: B29B_LOG=4 -> one line per fallback call of the not-yet-native exports: raw pascal args, pointed headers, DX:AX */
static void ddi_hex(const char *t, uint32_t fp, int n)
{
	uword l; uint8_t b[64]; if (n > 64) n = 64;
	if (!fp || !g_lin(fp, &l, n) || !g_read(l, b, n)) return;
	fprintf(stderr, " %s:", t); for (int j = 0; j < n; j++) fprintf(stderr, "%02x", b[j]);
}
static void ddi_log_arg(DdiFrame *f, long d, uint32_t ret)
{
	static int n[40]; int o = f->ord, k = ddi_slot(o); const uint8_t *a = f->a; char s1[64];
	if (o == 1 || o == 14 || o >= 101 || n[k]++ >= 60) return;
	static const int nb[32] = { 0, 32, 12, 0, 0, 0, 0, 0, 28, 16, 18, 0, 14, 0, 40, 0, 0, 0, 0, 26, 0, 32, 0, 0, 0, 0, 0, 40, 50, 0, 0, 0 };
	fprintf(stderr, "ARG ord %d instr %ld ret %08x a:", o, d, (unsigned)ret); for (int j = 0; j < (o < 32 && nb[o] ? nb[o] : 32); j++) fprintf(stderr, "%02x", a[j]);
	switch (o) {
	case 2: ddi_hex("phys", rd32(a), 8); ddi_dev_str(rd32(a + 8), s1, 64); fprintf(stderr, " dev[%s]", s1); break;
	case 8: ddi_dev_str(rd32(a + 24), s1, 64); fprintf(stderr, " dev[%s] style %d n %d", s1, rd16(a + 22), rd16(a + 20)); ddi_hex("pts", rd32(a + 16), 32); ddi_hex("pen", rd32(a + 12), 16); ddi_hex("brush", rd32(a + 8), 16); ddi_hex("dm", rd32(a + 4), 32); ddi_hex("clip", rd32(a), 8); break;
	case 9: ddi_dev_str(rd32(a + 12), s1, 64); fprintf(stderr, " dev[%s]", s1); ddi_hex("dm", rd32(a), 32); break;
	case 10: ddi_dev_str(rd32(a + 14), s1, 64); fprintf(stderr, " dev[%s] style %d", s1, (int16_t)rd16(a + 12)); ddi_hex("in", rd32(a + 8), 16); ddi_hex("out", rd32(a + 4), 64); break;
	case 19: ddi_dev_str(rd32(a + 22), s1, 64); fprintf(stderr, " bm[%s] get %d start %d scans %d", s1, rd16(a + 20), rd16(a + 18), rd16(a + 16)); ddi_hex("bmi", rd32(a + 8), 48); ddi_hex("xl", rd32(a), 8); break;
	case 21: ddi_dev_str(rd32(a + 28), s1, 64); fprintf(stderr, " dev[%s] xy %d,%d start %d scans %d", s1, (int16_t)rd16(a + 26), (int16_t)rd16(a + 24), rd16(a + 22), rd16(a + 20)); ddi_hex("clip", rd32(a + 16), 8); ddi_hex("bmi", rd32(a + 4), 48); ddi_hex("xl", rd32(a), 8); break;
	case 28: ddi_dev_str(rd32(a + 46), s1, 64); fprintf(stderr, " dev[%s] get %d dst %d,%d %dx%d src %d,%d %dx%d rop %08x", s1, rd16(a + 44), (int16_t)rd16(a + 42), (int16_t)rd16(a + 40), (int16_t)rd16(a + 38), (int16_t)rd16(a + 36),
		(int16_t)rd16(a + 34), (int16_t)rd16(a + 32), (int16_t)rd16(a + 30), (int16_t)rd16(a + 28), (unsigned)rd32(a + 12)); ddi_hex("bmi", rd32(a + 20), 40); ddi_hex("clip", rd32(a), 8); break;
	case 27: ddi_dev_str(rd32(a + 36), s1, 64); fprintf(stderr, " dst[%s]", s1); ddi_dev_str(rd32(a + 26), s1, 64); fprintf(stderr, " src[%s]", s1); break;
	case 12: ddi_dev_str(rd32(a + 10), s1, 64); fprintf(stderr, " dev[%s] x %d y %d col %08x style %d", s1, (int16_t)rd16(a + 8), (int16_t)rd16(a + 6), (unsigned)rd32(a + 2), rd16(a)); break;
	case 29: case 30: break;
	}
	fputc(10, stderr);
}
static void ddi_log_call(DdiFrame *f, long d)
{
	const uint8_t *a = f->a; char s1[64], s2[64];
	if (f->ord == 1 && d >= 2000) {
		ddi_dev_str(rd32(a + 28), s1, 64); ddi_dev_str(rd32(a + 20), s2, 64);
		uint8_t br[16] = {0}; uword l; uint32_t pb = rd32(a + 4); if (pb && g_lin(pb, &l, 16)) g_read(l, br, 16);
		fprintf(stderr, "BB %ld dst[%s] %d,%d src[%s] %d,%d ext %dx%d rop %06x br %02x%02x%02x%02x %02x%02x%02x%02x\n", d, s1, (int16_t)rd16(a + 26), (int16_t)rd16(a + 24),
			s2, (int16_t)rd16(a + 18), (int16_t)rd16(a + 16), (int16_t)rd16(a + 14), (int16_t)rd16(a + 12), (unsigned)rd32(a + 8),
			br[0], br[1], br[2], br[3], br[4], br[5], br[6], br[7]);
		static uint32_t seen[64]; static int ns; uint32_t key = (rd32(a + 8) >> 16) * 31 + (uint32_t)(s1[0] * 7 + s1[3]) * 131 + (uint32_t)(s2[0] * 7 + s2[3]);
		int k; for (k = 0; k < ns && seen[k] != key; k++) ;
		if (k == ns && ns < 64) {   /* first call of this kind: dump brush, draw mode, device headers + colour tables */
			seen[ns++] = key; uint8_t b[160];
			struct { const char *n; uint32_t fp; int len; } dd[] = { { "brush", pb, 112 }, { "drawmode", rd32(a), 48 }, { "dst", rd32(a + 28), 0x30 }, { "src", rd32(a + 20), 0x30 } };
			for (int i = 0; i < 4; i++) {
				if (!dd[i].fp || !g_lin(dd[i].fp, &l, dd[i].len) || !g_read(l, b, dd[i].len)) continue;
				fprintf(stderr, "  %s:", dd[i].n); for (int j = 0; j < dd[i].len; j++) fprintf(stderr, "%s%02x", j % 16 ? "" : " ", b[j]); fprintf(stderr, "\n");
				if (i >= 2 && rd16(b) == 0x5250 && rd32(b + 0x20)) {   /* deBitmapInfo: header + colour table */
					uint8_t c[0x28 + 64]; uword l2; if (g_lin(rd32(b + 0x20), &l2, sizeof c) && g_read(l2, c, sizeof c)) {
						fprintf(stderr, "  %s.bmi:", dd[i].n); for (int j = 0; j < (int)sizeof c; j++) fprintf(stderr, "%s%02x", j % 16 ? "" : " ", c[j]); fprintf(stderr, "\n"); }
				}
			}
		}
	} else if (f->ord == 14 && ddi_log >= 3) {   /* ExtTextOut(lpDestDev, x, y, lpClipRect, lpString, count, lpFontInfo, lpDrawMode, lpXform, lpDx, lpOpaqueRect, options) */
		static int n; if (n++ > 40) return;
		uword l; uint8_t fi[0xc0] = {0}, str[64] = {0}; int cnt = (int16_t)rd16(a + 22);
		ddi_dev_str(rd32(a + 36), s1, 64);
		if (rd32(a + 18) && g_lin(rd32(a + 18), &l, 0xc0)) g_read(l, fi, 0xc0);
		if (rd32(a + 24) && cnt > 0 && g_lin(rd32(a + 24), &l, cnt < 0 ? 0 : (cnt > 63 ? 63 : cnt))) g_read(l, str, cnt > 63 ? 63 : cnt);
		fprintf(stderr, "ETO %ld dst[%s] %d,%d cnt %d opt %04x clip %08x opaque %08x dx %08x str '%s' drawmode %08x\n", d, s1, (int16_t)rd16(a + 34), (int16_t)rd16(a + 32), cnt, rd16(a),
			(unsigned)rd32(a + 28), (unsigned)rd32(a + 2), (unsigned)rd32(a + 6), str, (unsigned)rd32(a + 14));
		fprintf(stderr, "  font:"); for (int j = 0; j < 0xc0; j++) fprintf(stderr, "%s%02x", j % 16 ? "" : " ", fi[j]); fputc(10, stderr);
		{ static int dumped; if (!dumped && rd16(fi) == 0 && rd32(a + 18)) { dumped = 1;   /* first raster font: raw 0x1200 bytes to font.bin */
			static uint8_t fb[0x1200]; FILE *ff = fopen(getenv("B29B_FONTDUMP") ? getenv("B29B_FONTDUMP") : "font.bin", "wb");
			for (int k = 0; ff && k < 0x1200 && g_lin(rd32(a + 18) + k, &l, 0x10) && g_read(l, fb + k, 0x10); k += 0x10) fwrite(fb + k, 1, 0x10, ff);   /* up to the segment limit */ if (ff) fclose(ff); } }
		uint8_t dm[0x20] = {0}, cr[8] = {0}, orr[8] = {0};
		if (rd32(a + 14) && g_lin(rd32(a + 14), &l, 0x20)) g_read(l, dm, 0x20);
		if (rd32(a + 28) && g_lin(rd32(a + 28), &l, 8)) g_read(l, cr, 8);
		if (rd32(a + 2) && g_lin(rd32(a + 2), &l, 8)) g_read(l, orr, 8);
		fprintf(stderr, "  dm:"); for (int j = 0; j < 0x20; j++) fprintf(stderr, "%02x", dm[j]);
		fprintf(stderr, " clip %d,%d,%d,%d opaque %d,%d,%d,%d\n", (int16_t)rd16(cr), (int16_t)rd16(cr + 2), (int16_t)rd16(cr + 4), (int16_t)rd16(cr + 6), (int16_t)rd16(orr), (int16_t)rd16(orr + 2), (int16_t)rd16(orr + 4), (int16_t)rd16(orr + 6));
	} else if (d >= 20000) fprintf(stderr, "DDI ord %d %ld instr\n", f->ord, d);
}
#endif
/* board console "ddi" / PC: per-export calls, native, fallback inclusive instr, reject reasons, pointer mode */
void gfx_ddi_report(FILE *f)
{
	extern int gfx_swcursor;
	if (!gfx_ddi) { fprintf(f, "[ddi] no table\r\n"); return; }
	for (int i = 0; i < GFX_DDI_N; i++) if (gfx_ddi[i].calls) fprintf(f, "[ddi] slot %d calls %ld native %ld fallback %ld fb_instr %ld\r\n", i, gfx_ddi[i].calls, gfx_ddi[i].native, gfx_ddi[i].calls - gfx_ddi[i].native, gfx_ddi[i].incl);
	fprintf(f, "[ddi] swcursor %d native_on %d rej", gfx_swcursor, gfx_ddi_native); for (int i = 1; i < 64 && nat_rej; i++) if (nat_rej[i]) fprintf(f, " %d:%ld", i, nat_rej[i]); fprintf(f, "\r\n");
}

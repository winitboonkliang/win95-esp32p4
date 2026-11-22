/* RED B29b: native DDI executors (included by gfxddi.c).
 * nat_call(ord, args, dry): >0 = executed (returns the pascal argument bytes, DX:AX in ddi_result), 0 = not handled (DIBENG fallback).
 * dry = 1 (PC verify): compute into a scratch buffer only; nat_verify() at the call's exit compares it with what DIBENG drew and with its
 * return value (1 equal, -1 different, 0 nothing checked).  Everything not understood falls back for the whole call (counted).
 *
 * Formats (learned from the calls the shell makes, checked call by call against DIBENG):
 *   DIBENGINE 'RP': +2 w, +4 h, +6 widthbytes, +9 bpp, +0xe deDeltaScan (signed), +0x16 deBits off32, +0x1a sel, +0x1c deFlags, +0x20 lpBitmapInfo
 *   BITMAP (bmType 0): +2 w, +4 h, +6 widthbytes, +8 planes, +9 bpp, +0xa bmBits (16:16), +0x16 bmSegmentIndex (huge: not handled)
 *   brush (DIB_Brush16): +1 bpp tag, +2 style (0 solid, 3 pattern), +0xe mono pattern 8 rows x 4 bytes, +0x4e colour bits
 *   DRAWMODE: +4 bkColor (physical, low word = 565), +8 TextColor */
extern int gfx_swcursor;   /* driver told us DIBENG draws the pointer into VRAM: no native screen drawing then */
int gfx_swcursor;

typedef struct { int bpp, w, h, wb; uword base; int32_t delta; int vram; uint32_t pal[16]; int npal; uint16_t sel; uword sbase; } Surf;   /* sel/sbase: selector of the bits + its linear base (touch list) */

static int g_lin32(uint16_t sel, uint32_t off, uword *lin, uword need)
{
	uword base, limit; if (!cpui386_sel_info(dpc->cpu, sel, &base, &limit)) return 0;
	if (off + (need ? need - 1 : 0) > limit || off + need < off) return 0;
	*lin = base + off; return 1;
}
static int surf_get(uint32_t fp, Surf *s)
{
	uword l; uint8_t h[0x30];
	memset(s, 0, sizeof *s);
	if (!fp || !g_lin(fp, &l, 0x24) || !g_read(l, h, 0x24)) return 0;
	s->w = rd16(h + 2); s->h = rd16(h + 4); s->wb = rd16(h + 6);
	if (s->w <= 0 || s->h <= 0 || s->w > NAT_MAXW) return 0;
	if (rd16(h) == 0x5250) {
		uint16_t fl = rd16(h + 0x1c);
		if (h[8] != 1) return 0;
		s->bpp = h[9]; s->delta = (int32_t)rd32(h + 0xe);
		if (fl & ~(0x0040 | 0x0004)) return 0;                     /* busy / banked / offscreen / palette xlat / not framebuffer: fallback */
		if (s->bpp == 16 && !(fl & 0x40)) return 0;                 /* 5-5-5 not handled */
		if (s->bpp != 16 && s->bpp != 4 && s->bpp != 1) return 0;
		uint32_t off = rd32(h + 0x16); uint16_t sel = rd16(h + 0x1a);
		int32_t lo = s->delta < 0 ? s->delta * (s->h - 1) : 0, span = (s->delta < 0 ? -s->delta : s->delta) * (s->h - 1) + s->wb;
		if (!g_lin32(sel, off + lo, &l, span)) return 0;
		s->base = l - lo; s->sel = sel; s->sbase = s->base - off;
		if (s->bpp != 16) {   /* colour table of the DIB */
			uword bl; uint8_t bh[0x28]; uint32_t bmi = rd32(h + 0x20);
			if (!bmi || !g_lin(bmi, &bl, 0x28) || !g_read(bl, bh, 0x28) || rd32(bh) != 0x28 || rd16(bh + 14) != s->bpp) return 0;
			int n = rd32(bh + 32); if (!n) n = 1 << s->bpp; if (n > (1 << s->bpp)) return 0;
			uint8_t c[64]; if (!g_read(bl + 0x28, c, n * 4)) return 0;
			for (int i = 0; i < n; i++) s->pal[i] = c[i * 4] | c[i * 4 + 1] << 8 | c[i * 4 + 2] << 16;
			s->npal = n;
		}
	} else if (rd16(h) == 0) {
		if (h[8] != 1 || h[9] != 1 || rd16(h + 0x16)) return 0;     /* mono, single segment only */
		s->bpp = 1; s->delta = s->wb;
		if (!g_lin(rd32(h + 0xa), &l, (uword)s->wb * s->h)) return 0;
		s->base = l; s->sel = rd32(h + 0xa) >> 16; s->sbase = l - (rd32(h + 0xa) & 0xffff);
	} else return 0;
	long p = cpui386_lin2phys(dpc->cpu, s->base, 0);
	s->vram = p != -1 && (uword)p >= dpc->pci_vga_ram_addr && (uword)p - dpc->pci_vga_ram_addr < (uword)dpc->vga_mem_size;
	return 1;
}

/* row byte span [x0, x0+n) -> host bytes; pages are resolved per 4 KB */
static int row_io(const Surf *s, int y, int x0, int n, uint8_t *buf, int *b0, int *nb, int wr)
{
	int bit0 = x0 * s->bpp, bit1 = (x0 + n) * s->bpp; *b0 = bit0 >> 3; *nb = ((bit1 + 7) >> 3) - *b0;
	uword lin = s->base + (int32_t)y * s->delta + *b0; int left = *nb; uint8_t *d = buf;
	while (left > 0) {
		uint8_t *hp = g_host(lin, wr); if (!hp) return 0;
		int k = 4096 - (lin & 4095); if (k > left) k = left;
		if (wr) { memcpy(hp, d, k); if (s->vram) gfx_lfb_dirty((uword)(hp - (uint8_t *)dpc->vga_mem), k); } else memcpy(d, hp, k);
		d += k; lin += k; left -= k;
	}
	return 1;
}
static int row_check(const Surf *s, int y, int x0, int n, int wr)   /* every page of the row present (and writable) */
{
	int b0 = (x0 * s->bpp) >> 3, nb = (((x0 + n) * s->bpp + 7) >> 3) - b0;
	uword lin = s->base + (int32_t)y * s->delta + b0, end = lin + nb;
	int ok = 1;   /* every missing page of the row goes on the touch list (the guest stub faults them in, then asks again) */
	for (uword p = lin & ~4095u; p < end; p += 4096) { uword q = p < lin ? lin : p; if (!g_host(q, wr)) { ok = 0; if (s->sel) ddi_touch_add(s->sel, q - s->sbase, wr); } }
	return ok;
}
#define rowbuf nat_rowbuf
/* direct host pointer to a 16 bpp row span when it is host-contiguous (the LFB always is; RAM DIBs when the span stays in one page or
 * the pages happen to be adjacent); little-endian host = guest pixel layout */
static uint16_t *row_ptr16(const Surf *s, int y, int x0, int n, int wr)
{
	uword lin = s->base + (int32_t)y * s->delta + x0 * 2, last = lin + n * 2 - 1;
	uint8_t *a = g_host(lin, wr); if (!a) return NULL;
	if ((lin >> 12) != (last >> 12)) { uint8_t *b = g_host(last, wr); if (b != a + (last - lin)) return NULL; }
	return (uint16_t *)a;
}
static int row_get(const Surf *s, int y, int x0, int n, uint16_t *px)
{
	if (s->bpp == 16) { uint16_t *p = row_ptr16(s, y, x0, n, 0); if (p) { memcpy(px, p, n * 2); return 1; } }
	int b0, nb; if (!row_io(s, y, x0, n, rowbuf, &b0, &nb, 0)) return 0;
	if (s->bpp == 16) { for (int i = 0; i < n; i++) px[i] = rowbuf[2 * i] | rowbuf[2 * i + 1] << 8; }
	else if (s->bpp == 4) { for (int i = 0; i < n; i++) { int x = x0 + i - (b0 << 1); px[i] = (rowbuf[x >> 1] >> (x & 1 ? 0 : 4)) & 15; } }
	else { for (int i = 0; i < n; i++) { int x = x0 + i - (b0 << 3); px[i] = (rowbuf[x >> 3] >> (7 - (x & 7))) & 1; } }
	return 1;
}
static int row_put(const Surf *s, int y, int x0, int n, const uint16_t *px)
{
	int b0, nb;
	if (s->bpp == 16) { uint16_t *p = row_ptr16(s, y, x0, n, 1); if (p) { memcpy(p, px, n * 2); if (s->vram) gfx_lfb_dirty((uword)((uint8_t *)p - (uint8_t *)dpc->vga_mem), n * 2); return 1; } }
	if (s->bpp == 16) { b0 = x0 * 2; nb = n * 2; for (int i = 0; i < n; i++) { rowbuf[2 * i] = (uint8_t)px[i]; rowbuf[2 * i + 1] = px[i] >> 8; } }
	else {
		if (!row_io(s, y, x0, n, rowbuf, &b0, &nb, 0)) return 0;   /* partial bytes keep their other pixels */
		if (s->bpp == 4) for (int i = 0; i < n; i++) { int x = x0 + i - (b0 << 1), sh = x & 1 ? 0 : 4; rowbuf[x >> 1] = (rowbuf[x >> 1] & ~(15 << sh)) | (px[i] & 15) << sh; }
		else for (int i = 0; i < n; i++) { int x = x0 + i - (b0 << 3), sh = 7 - (x & 7); rowbuf[x >> 3] = (rowbuf[x >> 3] & ~(1 << sh)) | (px[i] & 1) << sh; }
	}
	return row_io(s, y, x0, n, rowbuf, &b0, &nb, 1);
}

/* generic ROP3 on pixel values (bitwise, like DIBENG at every depth): rop bit (P<<2|S<<1|D) */
static inline uint32_t rop3(uint8_t r, uint32_t p, uint32_t s, uint32_t d)
{
	uint32_t o = 0;
	if (r & 0x01) { o |= ~p & ~s & ~d; }
	if (r & 0x02) { o |= ~p & ~s & d; }
	if (r & 0x04) { o |= ~p & s & ~d; }
	if (r & 0x08) { o |= ~p & s & d; }
	if (r & 0x10) { o |= p & ~s & ~d; }
	if (r & 0x20) { o |= p & ~s & d; }
	if (r & 0x40) { o |= p & s & ~d; }
	if (r & 0x80) { o |= p & s & d; }
	return o;
}

/* 16 bpp 5-6-5 -> nearest entry of a DIB colour table (method checked against DIBENG by the verify run) */
/* 5-6-5 -> nearest entry of a DIB colour table, the way DIBENG does it (found with the per-call verify, 161 distinct colours of S1-S6,
 * 0 mismatches): the colour goes through 5-5-5 (green loses its low bit), each 5-bit channel becomes the CENTRE of its 8-wide cell
 * (c * 8 + 4), then the smallest squared RGB distance wins (first index on a tie).  Plain 8-bit expansion and absolute distance both fail. */
static uint16_t nearest(const Surf *d, uint16_t c)
{
	int r = ((c >> 11) & 31) * 8 + 4, g = ((c >> 6) & 31) * 8 + 4, b = (c & 31) * 8 + 4;
	int best = 0; long bd = LONG_MAX;
	for (int i = 0; i < d->npal; i++) {
		int dr = r - (int)(d->pal[i] >> 16 & 255), dg = g - (int)(d->pal[i] >> 8 & 255), db = b - (int)(d->pal[i] & 255);
		long e = (long)dr * dr + (long)dg * dg + (long)db * db;
		if (e < bd) { bd = e; best = i; }
	}
	return best;
}

typedef struct { int ord, x, y, w, h; Surf d; uint16_t *px, *s0, *d0; uint32_t ret; uword mlin; uint8_t mexp[64]; int mlen, fullret; } NatVerify;   /* mlen: guest bytes the call must have written (ColorInfo, RealizeObject) */
#ifdef ESP_PLATFORM
static NatVerify *nv; static int nvn;   /* verify is PC-only */
#else
static NatVerify nv[8]; static int nvn;
#endif

/* BitBlt(lpDestDev, DestX, DestY, lpSrcDev, SrcX, SrcY, xExt, yExt, Rop3, lpPBrush, lpDrawMode) - 32 argument bytes */
static uint16_t *nat_tmp; static long nat_tmpn;
#ifdef ESP_PLATFORM
#define DEVSTR(fp, t) (void)(fp)
#else
#define DEVSTR(fp, t) ddi_dev_str((fp), (t), 64)
#endif
long *nat_rej;   /* 64 reject-reason counters (PSRAM), board console "ddi" and PC ix_dump */
#define REJ(n) (nat_rej ? nat_rej[n]++ : 0, 0)
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#include "esp_heap_caps.h"
#define GFX_PS EXT_RAM_BSS_ATTR
#else
#define GFX_PS
#endif
/* ---- GFX B29b: row kernels.  C references (PC, and the board self-test oracle) + PIE versions (esp/main/gfx_pie.S) ---- */
static void gfx_fill16_c(uint16_t *o, int n, const uint16_t *v8) { for (int i = 0; i < n; i++) o[i] = v8[i & 7]; }
static void gfx_xor16_c(uint16_t *o, int n, const uint16_t *v8) { for (int i = 0; i < n; i++) o[i] ^= v8[i & 7]; }
static void gfx_copy16_c(uint16_t *d, const uint16_t *s, int n) { memcpy(d, s, n * 2); }
static void gfx_and16_c(uint16_t *d, const uint16_t *s, int n) { for (int i = 0; i < n; i++) d[i] &= s[i]; }
static void gfx_xors16_c(uint16_t *d, const uint16_t *s, int n) { for (int i = 0; i < n; i++) d[i] ^= s[i]; }
static void gfx_rgb24_c(uint16_t *o, const uint8_t *r, int n) { for (int k = 0; k < n; k++) { const uint8_t *q = r + 3 * k; o[k] = (q[2] >> 3) << 11 | (q[1] >> 2) << 5 | q[0] >> 3; } }
typedef struct { void (*fill)(uint16_t *, int, const uint16_t *), (*xorv)(uint16_t *, int, const uint16_t *), (*copy)(uint16_t *, const uint16_t *, int),
	(*andd)(uint16_t *, const uint16_t *, int), (*xors)(uint16_t *, const uint16_t *, int), (*rgb24)(uint16_t *, const uint8_t *, int); } GfxK;
static GfxK gk = { gfx_fill16_c, gfx_xor16_c, gfx_copy16_c, gfx_and16_c, gfx_xors16_c, gfx_rgb24_c };
static uint16_t gk_v8[8] GFX_PS __attribute__((aligned(16)));   /* the 8-px vector handed to fill/xor */
#ifdef ESP_PLATFORM
void gfx_pie_fill16(uint16_t *o, int n, const uint16_t *v8); void gfx_pie_xor16(uint16_t *o, int n, const uint16_t *v8);
void gfx_pie_copy16(uint16_t *d, const uint16_t *s, int n); void gfx_pie_and16(uint16_t *d, const uint16_t *s, int n);
void gfx_pie_xors16(uint16_t *d, const uint16_t *s, int n); void gfx_rgb24_565(uint16_t *o, const uint8_t *s, int n);
static const GfxK gk_pie = { gfx_pie_fill16, gfx_pie_xor16, gfx_pie_copy16, gfx_pie_and16, gfx_pie_xors16, gfx_rgb24_565 };
static const GfxK gk_c = { gfx_fill16_c, gfx_xor16_c, gfx_copy16_c, gfx_and16_c, gfx_xors16_c, gfx_rgb24_c };
/* every kernel, PIE vs C, all 8 alignments of both pointers x lengths 0..40, random data; buffers in PSRAM (where the rows live) */
static uint16_t gst_a[96] EXT_RAM_BSS_ATTR __attribute__((aligned(16))), gst_b[96] EXT_RAM_BSS_ATTR __attribute__((aligned(16))), gst_s[96] EXT_RAM_BSS_ATTR __attribute__((aligned(16)));
static uint8_t gst_r[160] EXT_RAM_BSS_ATTR __attribute__((aligned(16)));
static int gfx_selftest(void)
{
	uint32_t r = 777; int bad[6] = {0};
	for (int t = 0; t < 1200; t++) {
		int k = t % 6, od = (t / 6) & 7, os = (t / 48) & 7, n = (t * 7 + 3) % 41;
		for (int i = 0; i < 96; i++) { r = r * 1103515245 + 12345; gst_a[i] = gst_b[i] = r >> 9; r = r * 1103515245 + 12345; gst_s[i] = r >> 11; }
		for (int i = 0; i < 160; i++) { r = r * 1103515245 + 12345; gst_r[i] = r >> 16; }
		for (int i = 0; i < 8; i++) gk_v8[i] = gst_s[i] ^ 0x5a5a;
		switch (k) {
		case 0: gk_pie.fill(gst_a + od, n, gk_v8); gk_c.fill(gst_b + od, n, gk_v8); break;
		case 1: gk_pie.xorv(gst_a + od, n, gk_v8); gk_c.xorv(gst_b + od, n, gk_v8); break;
		case 2: gk_pie.copy(gst_a + od, gst_s + 8 + os, n); gk_c.copy(gst_b + od, gst_s + 8 + os, n); break;
		case 3: gk_pie.andd(gst_a + od, gst_s + 8 + os, n); gk_c.andd(gst_b + od, gst_s + 8 + os, n); break;
		case 4: gk_pie.xors(gst_a + od, gst_s + 8 + os, n); gk_c.xors(gst_b + od, gst_s + 8 + os, n); break;
		default: gk_pie.rgb24(gst_a + od, gst_r + 4 * (os & 1) + os, n); gk_c.rgb24(gst_b + od, gst_r + 4 * (os & 1) + os, n);
		}
		if (memcmp(gst_a, gst_b, sizeof gst_a)) bad[k]++;
	}
	int sum = bad[0] + bad[1] + bad[2] + bad[3] + bad[4] + bad[5];
	printf("[gfx] PIE self-test %s: fill %d xor %d copy %d and %d xors %d rgb24 %d differ of 200 each\r\n", sum ? "FAIL - C kernels" : "ok", bad[0], bad[1], bad[2], bad[3], bad[4], bad[5]);
	return !sum;
}
#include "esp_timer.h"
#include "esp_cache.h"
extern int gfx_k_pie;   /* console "gk 0|1": 1 = PIE kernels (default after the self-test), 0 = C kernels (A/B) */
int gfx_k_pie = 1;
static void gk_init(void)
{
	static int done; if (done) return; done = 1;
	if (gfx_selftest()) gk = gk_pie; else { gk = gk_c; if (nat_rej) nat_rej[47]++; }
}
static inline void gk_sel(void) { gk_init(); gk = gfx_k_pie && !(nat_rej && nat_rej[47]) ? gk_pie : gk_c; }
/* console "gfxbench": C vs PIE vs PPA on PSRAM rows (1280-px stride like the LFB), microseconds per call */
extern bool ppa_fill_rgb565(uint16_t rgb565, void *dst, size_t dst_pic_w, size_t dst_pic_h, size_t dst_size, size_t off_x, size_t off_y, size_t fill_w, size_t fill_h);
void gfx_bench(void)
{
	gk_init();
	enum { W = 1280, H = 400 };
	uint16_t *buf = heap_caps_aligned_alloc(64, W * H * 2, MALLOC_CAP_SPIRAM), *src = heap_caps_aligned_alloc(64, W * (H + 1) * 2, MALLOC_CAP_SPIRAM);
	if (!buf || !src) { printf("[gfxbench] no PSRAM\r\n"); heap_caps_free(buf); heap_caps_free(src); return; }
	for (int i = 0; i < W * H; i++) src[i] = i * 2654435761u >> 16;
	static const int sz[][2] = { { 8, 8 }, { 16, 16 }, { 32, 32 }, { 64, 32 }, { 128, 64 }, { 256, 128 }, { 512, 256 }, { 1280, 400 } };
	for (int c = 0; c < 8; c++) {
		int w = sz[c][0], h = sz[c][1], reps = c < 4 ? 50 : c < 6 ? 10 : 3; int64_t t; long us[6];
		for (int i = 0; i < 8; i++) gk_v8[i] = 0x7bef;
		t = esp_timer_get_time(); for (int k = 0; k < reps; k++) for (int y = 0; y < h; y++) gk_c.fill(buf + y * W + 4, w, gk_v8); us[0] = (esp_timer_get_time() - t) / reps;
		t = esp_timer_get_time(); for (int k = 0; k < reps; k++) for (int y = 0; y < h; y++) gk_pie.fill(buf + y * W + 4, w, gk_v8); us[1] = (esp_timer_get_time() - t) / reps;
		t = esp_timer_get_time(); int ok = 1; for (int k = 0; k < reps; k++) ok &= ppa_fill_rgb565(0x7bef, buf, W, H, W * H * 2, 4, 0, w & ~3 ? w & ~3 : 4, h); us[2] = (esp_timer_get_time() - t) / reps;
		int exact = 1; for (int y = 0; y < h && exact; y++) for (int x = 0; x < (w & ~3); x++) if (buf[y * W + 4 + x] != 0x7bef) { exact = 0; break; }
		t = esp_timer_get_time(); for (int k = 0; k < reps; k++) for (int y = 0; y < h; y++) gk_c.copy(buf + y * W + 4, src + y * W + 8, w); us[3] = (esp_timer_get_time() - t) / reps;
		t = esp_timer_get_time(); for (int k = 0; k < reps; k++) for (int y = 0; y < h; y++) gk_pie.copy(buf + y * W + 4, src + y * W + 8, w); us[4] = (esp_timer_get_time() - t) / reps;
		t = esp_timer_get_time(); for (int k = 0; k < reps; k++) for (int y = 0; y < h; y++) gk_pie.rgb24(buf + y * W, (const uint8_t *)(src + y * W), w); us[5] = (esp_timer_get_time() - t) / reps;
		printf("[gfxbench] %dx%d fill C %ld PIE %ld PPA %ld (ok %d exact %d) | copy C %ld PIE %ld | rgb24 asm %ld us\r\n", w, h, us[0], us[1], us[2], ok, exact, us[3], us[4], us[5]);
	}
	heap_caps_free(buf); heap_caps_free(src);
}
#else
static inline void gk_sel(void) { }
#endif

#ifdef ESP_PLATFORM
int gfx_ppa_min_px = 0;   /* console "ppa N": solid PATCOPY of >= N px into the LFB goes to the PPA fill engine (0 = never) */
long gfx_ppa_fills;
#endif
static __attribute__((noinline)) int nat_bitblt(const uint8_t *a, int dry)   /* noinline: keeps the sum of all native locals out of gfx_reg_write's frame (992 B inlined) */
{
	gk_sel();
	uint32_t rop = rd32(a + 8); uint8_t r = rop >> 16;
	int w = (int16_t)rd16(a + 14), h = (int16_t)rd16(a + 12), dx = (int16_t)rd16(a + 26), dy = (int16_t)rd16(a + 24), sx = (int16_t)rd16(a + 18), sy = (int16_t)rd16(a + 16);
	int useS = ((r >> 2) ^ r) & 0x33, useP = ((r >> 4) ^ r) & 0x0f, useD = ((r >> 1) ^ r) & 0x55;
	Surf D, S; uint16_t pat[8][8]; uint16_t bk = 0, tx = 0;
	if (!surf_get(rd32(a + 28), &D)) { static int k; if (ddi_log && k++ < 20) { char t[64] = ""; DEVSTR(rd32(a + 28), t); fprintf(stderr, "REJ1 dst %s\n", t); } return REJ(1); }
	if (w <= 0 || h <= 0) return REJ(2);                                  /* let DIBENG decide what an empty blit returns */
	if (dx < 0 || dy < 0 || dx + w > D.w || dy + h > D.h) return REJ(3);
	if (D.vram && gfx_swcursor) return REJ(4);
	if (useS) {
		if (!surf_get(rd32(a + 20), &S)) { static int k; if (ddi_log && k++ < 20) { char t[64] = ""; DEVSTR(rd32(a + 20), t); fprintf(stderr, "REJ5 src %s\n", t); } return REJ(5); }
		if (sx < 0 || sy < 0 || sx + w > S.w || sy + h > S.h) return REJ(6);
		int ok = S.bpp == D.bpp ? (D.bpp != 4 || (S.npal == D.npal && !memcmp(S.pal, D.pal, 4 * D.npal))) : (D.bpp == 16 && S.bpp == 1) || (D.bpp == 1 && S.bpp == 16) || (D.bpp == 4 && S.bpp == 16);
		if (!ok) return REJ(7);
	}
	if ((useS && (S.bpp == 1 || D.bpp == 1) && S.bpp != D.bpp) || useP) {
		uword l; uint8_t dm[12]; if (!rd32(a) || !g_lin(rd32(a), &l, 12) || !g_read(l, dm, 12)) return REJ(8);
		bk = rd16(dm + 4); tx = rd16(dm + 8);
	}
	if (useP) {
		uword l; uint8_t b[0x50]; if (D.bpp != 16 || !rd32(a + 4) || !g_lin(rd32(a + 4), &l, 0x50) || !g_read(l, b, 0x50)) return REJ(9);
		int style = rd16(b + 2);
		if (style == 0 && b[1] == 0x14 && b[0] == 0x01) { for (int i = 0; i < 64; i++) pat[i >> 3][i & 7] = rd16(b + 0x4e); }
		else if (style == 3 && b[1] == 0x14 && b[0] == 0x0c) { for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) pat[y][x] = (b[0xe + y * 4] >> (7 - x) & 1) ? bk : tx; }
		else return REJ(10);
	}
	{ int bad11 = 0; for (int y = 0; y < h; y++) if (!row_check(&D, dy + y, dx, w, !dry) || (useS && !row_check(&S, sy + y, sx, w, 0))) bad11 = 1; if (bad11) return REJ(11); }
#ifdef ESP_PLATFORM
	if (r == 0xf0 && !dry && gfx_ppa_min_px && (long)w * h >= gfx_ppa_min_px && D.vram && D.bpp == 16 && !((dx | w) & 3) && D.delta == D.wb) {
		int solid = 1; for (int i = 1; i < 64; i++) if (pat[i >> 3][i & 7] != pat[0][0]) { solid = 0; break; }
		uint16_t *b0 = row_ptr16(&D, 0, 0, 1, 1);
		if (solid && b0 == (uint16_t *)dpc->vga_mem && ppa_fill_rgb565(pat[0][0], b0, D.wb / 2, D.h, (size_t)D.wb * D.h, dx, dy, w, h)) {
			for (int y = 0; y < h; y++) gfx_lfb_dirty((uword)((dy + y) * D.wb + dx * 2), w * 2);
			gfx_ppa_fills++; ddi_result = 1; return 32;
		}
	}
#endif
	/* source rows first (screen-to-screen overlap in any direction is then exact).  Board: bands of <= 64 K px (a full-screen blit
	 * needed 2 x 4 MB of scratch and failed, REJ 12); a screen-to-screen copy downwards walks the bands bottom-up so every band still
	 * reads its source before anything overwrites it.  The PC verify keeps one band (= the whole blit) for the oracle compare. */
	int band = dry || (long)w * h <= 65536 ? h : (65536 / w > 0 ? 65536 / w : 1);
	int up = useS && S.base == D.base && dy > sy;
	for (int b0 = 0; b0 < h; b0 += band) {
	int bh = h - b0 < band ? h - b0 : band, yb = up ? h - b0 - bh : b0;
	long need = (long)w * bh; if (need > nat_tmpn) { free(nat_tmp); nat_tmp = malloc(need * 2 * sizeof *nat_tmp); nat_tmpn = nat_tmp ? need : 0; if (!nat_tmp) return REJ(12); }
	uint16_t *sb = nat_tmp, *ob = nat_tmp + need, *s0 = NULL, *d0 = NULL;
	if (dry) { s0 = calloc(need, 2); d0 = calloc(need, 2); if (!s0 || !d0) { free(s0); free(d0); return REJ(13); } }
	if (useS) for (int y = 0; y < bh; y++) {
		if (!row_get(&S, sy + yb + y, sx, w, sb + (long)y * w)) { free(s0); free(d0); return REJ(14); }
		if (dry) memcpy(s0 + (long)y * w, sb + (long)y * w, w * 2);
		uint16_t *p = sb + (long)y * w;
		if (S.bpp == 1 && D.bpp == 16) for (int i = 0; i < w; i++) p[i] = p[i] ? bk : tx;
		else if (S.bpp == 16 && D.bpp == 1) for (int i = 0; i < w; i++) p[i] = p[i] == bk;
		else if (S.bpp == 16 && D.bpp == 4) for (int i = 0; i < w; i++) p[i] = nearest(&D, p[i]);
	}
	uint32_t mask = D.bpp == 16 ? 0xffff : D.bpp == 4 ? 15 : 1;
	for (int y = 0; y < bh; y++) {
		uint16_t *o = ob + (long)y * w, *s = sb + (long)y * w;
		if ((useD || dry) && !row_get(&D, dy + yb + y, dx, w, o)) { free(s0); free(d0); return REJ(15); }
		if (dry) memcpy(d0 + (long)y * w, o, w * 2);
		const uint16_t *pr = pat[(dy + yb + y) & 7]; int px0 = dx & 7;
		uint16_t *const v8 = gk_v8;
		switch (r) {   /* the ROPs the shell uses, same result as rop3(); row kernels = PIE on the board (gfx_pie.S), C on the PC */
		case 0xcc: gk.copy(o, s, w); break;
		case 0xf0: for (int j = 0; j < 8; j++) v8[j] = pr[(px0 + j) & 7]; gk.fill(o, w, v8); break;
		case 0x00: for (int j = 0; j < 8; j++) v8[j] = 0; gk.fill(o, w, v8); break;
		case 0xff: for (int j = 0; j < 8; j++) v8[j] = mask; gk.fill(o, w, v8); break;
		case 0x5a: for (int j = 0; j < 8; j++) v8[j] = pr[(px0 + j) & 7]; gk.xorv(o, w, v8); break;
		case 0x55: for (int j = 0; j < 8; j++) v8[j] = mask; gk.xorv(o, w, v8); break;
		case 0x66: gk.xors(o, s, w); break;
		case 0x88: gk.andd(o, s, w); break;
		case 0xee: for (int i = 0; i < w; i++) o[i] |= s[i]; break;
		default: for (int i = 0; i < w; i++) o[i] = rop3(r, useP ? pr[(px0 + i) & 7] : 0, useS ? s[i] : 0, o[i]) & mask;
		}
	}
	if (dry) {
		if (nvn >= 8) { free(s0); free(d0); return REJ(16); }
		NatVerify *v = &nv[nvn]; v->px = malloc(need * 2);
		if (!v->px) { free(s0); free(d0); return REJ(16); }
		nvn++; v->ord = 1; v->x = dx; v->y = dy; v->w = w; v->h = h; v->d = D; v->ret = 1; v->s0 = s0; v->d0 = d0;
		memcpy(v->px, ob, need * 2);
		return 32;
	}
	for (int y = 0; y < bh; y++) if (!row_put(&D, dy + yb + y, dx, w, ob + (long)y * w)) return REJ(17);
	}
	ddi_result = 1;
	return 32;
}

/* ExtTextOut(lpDestDev, x, y, lpClipRect, lpString, count, lpFontInfo, lpDrawMode, lpTextBBox, lpDx, lpOpaqueRect, options) - 40 argument bytes.
 * Only DIBENG's engine-font path (options & 0x300 == 0x100, read from DIBENG.DLL seg 4: entry 0, helpers 0x237/0x2e8/0x318/0x925/0xe23/0xeee):
 *   font segment (selector of lpFontInfo, offsets from 0): +2 flags (0x80 = big glyph headers: fallback), +6 dword offset of a WORD glyph-offset
 *   table indexed by the char code, +0xa dword offset of a WORD advance table.  Glyph = int8 x, int8 top (above the baseline y), u8 w, u8 h,
 *   then h rows of (w+7)/8 bytes, MSB first; every stored bit is drawn (DIBENG does not mask to w).
 *   lpTextBBox (arg +10) is the text box GDI computed; drawing is clipped to it and to the clip rect (and to the opaque rect with ETO_CLIPPED).
 *   Opaque rects (list ended by an all-zero rect) are filled with the bk colour first; glyph bits = text colour, other bits of the text box =
 *   bk colour only with bkMode OPAQUE.  Returns 0.  Fallback (counted): extent query (count < 0), glyph-index chars / TrueType (Marlett), lpDx. */
static int rect_rd(uint32_t fp, int *r)
{
	uword l; uint8_t b[8]; if (!fp || !g_lin(fp, &l, 8) || !g_read(l, b, 8)) return 0;
	for (int i = 0; i < 4; i++) r[i] = (int16_t)rd16(b + 2 * i);
	return 1;
}
static int rect_and(int *r, const int *c)   /* DIBENG helper 0x237: 0 = empty */
{
	for (int k = 0; k < 2; k++) { if (r[k] < c[k]) r[k] = c[k]; if (r[k + 2] > c[k + 2]) r[k + 2] = c[k + 2]; }
	return r[2] > r[0] && r[3] > r[1];
}
static int fseg_rd(uint16_t sel, uint32_t off, void *b, int n) { uword l; return g_lin32(sel, off, &l, n) && g_read(l, b, n); }
static uint16_t *eto_mono, *eto_lut; static long eto_monon; static int eto_ok = 1;   /* PSRAM, 16-byte aligned: mask rows + byte -> 8 px mask table (4 KB) */
#define ETO_PAD 32
/* C reference of the PIE kernels (the PC runs these; the board self-test compares them with eto_pie.S) */
static void eto_or_c(uint16_t *m, const uint8_t *bits, int nbytes, const uint16_t *lut) { for (int k = 0; k < nbytes; k++) for (int j = 0; j < 8; j++) m[8 * k + j] |= lut[bits[k] * 8 + j]; }
static void eto_tr_c(uint16_t *o, const uint16_t *m, int n, const uint16_t *cv) { for (int i = 0; i < n; i++) o[i] ^= (o[i] ^ cv[0]) & m[i]; }
static void eto_op_c(uint16_t *o, const uint16_t *m, int n, const uint16_t *cv) { for (int i = 0; i < n; i++) o[i] = cv[8] ^ ((cv[8] ^ cv[0]) & m[i]); }
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define ETO_AALLOC(n) heap_caps_aligned_alloc(16, (n), MALLOC_CAP_SPIRAM)
#define ETO_AFREE(p) heap_caps_free(p)
/* eto_pie.S (PIE, unaligned q loads/stores via CFG mis_ld/mis_st): OR 8-px masks; blend 8 px per q register */
void eto_pie_or(uint16_t *m, const uint8_t *bits, int nbytes, const uint16_t *lut);
void eto_pie_tr(uint16_t *o, const uint16_t *m, int n, const uint16_t *cv);
void eto_pie_op(uint16_t *o, const uint16_t *m, int n, const uint16_t *cv);
#define eto_or (gfx_ddi_native == 3 ? eto_or_c : eto_pie_or)   /* nat 3 = same native ExtTextOut with the C kernels (PIE A/B) */
#define eto_tr (gfx_ddi_native == 3 ? eto_tr_c : eto_pie_tr)   /* nat 3 = same native ExtTextOut with the C kernels (PIE A/B) */
#define eto_op (gfx_ddi_native == 3 ? eto_op_c : eto_pie_op)   /* nat 3 = same native ExtTextOut with the C kernels (PIE A/B) */
static int eto_st_run(const char *tag, uint16_t *A, uint16_t *Bv, uint16_t *M, uint16_t *cv)
{
	uint32_t r = 12345; int bad[3] = {0}, boff[8] = {0}; uint8_t bits[4];
	for (int t = 0; t < 400; t++) {
		int off = t & 7, n = (t >> 3) % 41;
		for (int i = 0; i < 64; i++) { r = r * 1103515245 + 12345; A[i] = Bv[i] = r >> 8; }
		for (int i = 0; i < 48; i++) { r = r * 1103515245 + 12345; M[i] = (r >> 20) & 1 ? 0xffff : 0; }
		for (int k = 0; k < 16; k++) { r = r * 1103515245 + 12345; cv[k] = r >> 9; }
		for (int k = 1; k < 8; k++) { cv[k] = cv[0]; cv[8 + k] = cv[8]; }   /* the kernels take splatted colour vectors (nat_eto builds them so) */
		for (int k = 0; k < 4; k++) { r = r * 1103515245 + 12345; bits[k] = r >> 16; }
		switch (t % 3) {
		case 0: eto_pie_tr(A + off, M, n, cv); eto_tr_c(Bv + off, M, n, cv); break;
		case 1: eto_pie_op(A + off, M, n, cv); eto_op_c(Bv + off, M, n, cv); break;
		default: eto_pie_or(A + off, bits, n & 3, eto_lut); eto_or_c(Bv + off, bits, n & 3, eto_lut);
		}
		if (memcmp(A, Bv, 64 * 2)) { bad[t % 3]++; boff[off]++; }
	}
	printf("[eto] st %s: tr %d op %d or %d of ~133 each; by off %d %d %d %d %d %d %d %d\r\n", tag, bad[0], bad[1], bad[2], boff[0], boff[1], boff[2], boff[3], boff[4], boff[5], boff[6], boff[7]);
	return bad[0] + bad[1] + bad[2];
}
static int eto_selftest(void)   /* PIE == C on every alignment 0..7 px and length 0..40; once in internal RAM (diagnostic), once in PSRAM (the real use) */
{
	static uint16_t IA[64] __attribute__((aligned(16))), IB[64] __attribute__((aligned(16))), IM[48] __attribute__((aligned(16))), Icv[16] __attribute__((aligned(16)));
	static uint16_t A[64] EXT_RAM_BSS_ATTR __attribute__((aligned(16))), Bv[64] EXT_RAM_BSS_ATTR __attribute__((aligned(16))), M[48] EXT_RAM_BSS_ATTR __attribute__((aligned(16))), cv[16] EXT_RAM_BSS_ATTR __attribute__((aligned(16)));
	int bi = eto_st_run("internal", IA, IB, IM, Icv), bad = eto_st_run("psram", A, Bv, M, cv);
	printf("[eto] PIE self-test %s (internal %d, psram %d differ of 400)\r\n", bad ? "FAIL - native ExtTextOut off" : "ok", bi, bad);
	return !bad;
}
#else
#define ETO_AALLOC(n) aligned_alloc(16, ((n) + 15) & ~15)
#define ETO_AFREE(p) free(p)
#define eto_or eto_or_c
#define eto_tr eto_tr_c
#define eto_op eto_op_c
#endif
#define ETO_MAXR 16
#ifdef ESP_PLATFORM

#define ETO_PS EXT_RAM_BSS_ATTR   /* PSRAM .bss: the 4 KB emulator task stack overflowed with 1.4 KB of locals here */
#else
#define ETO_PS
#endif
static uint8_t eto_str[256] ETO_PS, eto_bits[4 * 255] ETO_PS;
static int eto_R[ETO_MAXR][4] ETO_PS;
static uint16_t eto_cv[16] ETO_PS __attribute__((aligned(16)));
static __attribute__((noinline)) int nat_eto(const uint8_t *a, int dry)
{
	int opt = rd16(a), cnt = (int16_t)rd16(a + 22), x = (int16_t)rd16(a + 34), y = (int16_t)rd16(a + 32);
	uint32_t fi = rd32(a + 18); uint16_t fsel = fi >> 16;
	Surf D; int C[4], T[4] = {0}, (*const R)[4] = eto_R, nr = 0, text = 0;
	if (cnt < 0) return REJ(18);                                   /* extent query */
	/* DIBENG picks its glyph fetch on bit 0x100 alone (seg 4 0x44a: 0x100 -> fetch 0xf23 whatever 0x200 says); glyph indices (0x10) and the
	 * engine TrueType font (dfType 0x400, Marlett) use the same segment tables (PC verify: 0 mismatches, mutation check live) */
	if (gfx_ddi_native == 5 && ((opt & 0x300) != 0x100 || (opt & 0x10) || rd32(a + 6))) return REJ(19);   /* nat 5: master 846a46a..c3180b4 set */
	if (!(opt & 0x100)) { if (ddi_log >= 2) fprintf(stderr, "ETOREJ19 opt %04x dx %08x cnt %d\n", opt, (unsigned)rd32(a + 6), cnt); return REJ(19); }
	uint8_t fh[0xe]; if (!fi || !fseg_rd(fsel, fi & 0xffff, fh, 2) || (rd16(fh) && (rd16(fh) != 0x400 || gfx_ddi_native == 5))) return REJ(20);   /* other dfType */
	if (!fseg_rd(fsel, 0, fh, 0xe) || (rd16(fh + 2) & 0x80)) return REJ(21);
	uint32_t gtab = rd32(fh + 6), wtab = rd32(fh + 0xa); uword eto_dxp = 0;
	if (rd32(a + 6) && cnt > 0 && !g_lin(rd32(a + 6), &eto_dxp, 2 * cnt)) return REJ(25);
	if (!surf_get(rd32(a + 36), &D) || D.bpp != 16) return REJ(22);
	if (D.vram && gfx_swcursor) return REJ(4);
	uword l; uint8_t dm[10]; if (!rd32(a + 14) || !g_lin(rd32(a + 14), &l, 10) || !g_read(l, dm, 10)) return REJ(8);
	int opaque_bk = (rd16(dm + 2) - 1) & 1; uint16_t bk = rd16(dm + 4), tx = rd16(dm + 8);
#ifdef ETO_MUT
	tx ^= 0x0821;   /* mutation check: the verify must see this */
#endif
	if (!rect_rd(rd32(a + 28), C)) return REJ(23);
	uint32_t orp = rd32(a + 2);
	if ((opt & 4) && (orp >> 16)) { int o[4]; if (!rect_rd(orp, o)) return REJ(23); if (!rect_and(C, o)) { ddi_result = 0; return 40; } }
	int S[4] = { 0, 0, D.w, D.h }; if (!rect_and(C, S)) { ddi_result = 0; return 40; }
	if (orp >> 16) for (int i = 0;; i++) {                          /* opaque rect list, ended by an all-zero rect */
		uword ol; uint8_t b[8]; if (!g_lin(orp + 8 * i, &ol, 8) || !g_read(ol, b, 8)) return REJ(23);
		if (!(rd32(b) | rd32(b + 4))) break;
		if (nr >= ETO_MAXR) return REJ(24);
		for (int k = 0; k < 4; k++) R[nr][k] = (int16_t)rd16(b + 2 * k);
		if (rect_and(R[nr], C)) nr++;
	}
	if (cnt > 0) { if (!rect_rd(rd32(a + 10), T)) return REJ(23); text = rect_and(T, C); }
	if (!nr && !text) { ddi_result = 0; return 40; }
	int B[4] = { 32767, 32767, -32768, -32768 };                   /* bounding box of everything drawn */
	for (int i = 0; i <= nr; i++) { const int *r = i < nr ? R[i] : T; if (i == nr && !text) break;
		for (int k = 0; k < 2; k++) { if (r[k] < B[k]) B[k] = r[k]; if (r[k + 2] > B[k + 2]) B[k + 2] = r[k + 2]; } }
	int bw = B[2] - B[0], bh = B[3] - B[1]; long need = (long)bw * bh;
	{ int bad11 = 0; for (int yy = B[1]; yy < B[3]; yy++) if (!row_check(&D, yy, B[0], bw, !dry)) bad11 = 1; if (bad11) return REJ(11); }
	if (need > nat_tmpn) { free(nat_tmp); nat_tmp = malloc(need * 2 * sizeof *nat_tmp); nat_tmpn = nat_tmp ? need : 0; if (!nat_tmp) return REJ(12); }
	uint16_t *ob = nat_tmp;
	for (int yy = 0; yy < bh; yy++) if (!row_get(&D, B[1] + yy, B[0], bw, ob + (long)yy * bw)) return REJ(15);
	uint16_t *d0 = NULL; if (dry) { d0 = malloc(need * 2); if (!d0) return REJ(13); memcpy(d0, ob, need * 2); }
	for (int i = 0; i < nr; i++) for (int yy = R[i][1]; yy < R[i][3]; yy++) { uint16_t *o = ob + (long)(yy - B[1]) * bw - B[0]; for (int xx = R[i][0]; xx < R[i][2]; xx++) o[xx] = bk; }
	if (text) {
		/* 16-bit mask (0 / 0xffff per pixel) over the text box, ETO_PAD px of margin on both sides so a glyph row (<= 32 px) never needs
		 * x clipping; rows 16-byte aligned for the PIE kernels */
		int tw = T[2] - T[0], th = T[3] - T[1], ms = ((tw + 7) & ~7) + 2 * ETO_PAD; long mn = (long)ms * th;
		if (!eto_lut) { eto_lut = ETO_AALLOC(256 * 16); if (!eto_lut) { free(d0); return REJ(12); }
			for (int v = 0; v < 256; v++) for (int k = 0; k < 8; k++) eto_lut[v * 8 + k] = (v >> (7 - k) & 1) ? 0xffff : 0; 
#ifdef ESP_PLATFORM
			eto_ok = eto_selftest();
#endif
		}
		if (!eto_ok) { free(d0); return REJ(27); }
		if (mn > eto_monon) { ETO_AFREE(eto_mono); eto_mono = ETO_AALLOC(mn * 2); eto_monon = eto_mono ? mn : 0; if (!eto_mono) { free(d0); return REJ(12); } }
		memset(eto_mono, 0, mn * 2);
		uint8_t *const str = eto_str; if (cnt > 256 || !g_lin(rd32(a + 24), &l, cnt) || !g_read(l, str, cnt)) { free(d0); return REJ(25); }
		int pen = x;
		for (int i = 0; i < cnt; i++) {
			uint8_t w2[2], gh[4], *const bits = eto_bits; int c = str[i];
			if (!fseg_rd(fsel, gtab + 2 * c, w2, 2)) { free(d0); return REJ(25); }
			uint32_t go = rd16(w2); if (!fseg_rd(fsel, go, gh, 4)) { free(d0); return REJ(25); }
			int gx = pen + (int8_t)gh[0] - T[0], gy = y - (int8_t)gh[1] - T[1], gw = gh[2], gn = gh[3], bpr = (gw + 7) >> 3;
			if (gw && gn && gx + bpr * 8 > 0 && gx < tw && gy + gn > 0 && gy < th) {   /* glyphs fully outside the box draw nothing */
				if (bpr > 4 || !fseg_rd(fsel, go + 4, bits, gn * bpr)) { free(d0); return REJ(26); }
				for (int r = 0; r < gn; r++) if (gy + r >= 0 && gy + r < th) eto_or(eto_mono + (long)(gy + r) * ms + ETO_PAD + gx, bits + r * bpr, bpr, eto_lut);
			}
			if (eto_dxp) { if (!g_read(eto_dxp + 2 * i, w2, 2)) { free(d0); return REJ(25); } }   /* lpDx: caller's advances */
			else if (!fseg_rd(fsel, wtab + 2 * c, w2, 2)) { free(d0); return REJ(25); }
			pen += (int16_t)rd16(w2);
		}
		uint16_t *const cv = eto_cv; for (int k = 0; k < 8; k++) { cv[k] = tx; cv[8 + k] = bk; }
		for (int yy = 0; yy < th; yy++) { uint16_t *o = ob + (long)(T[1] + yy - B[1]) * bw + (T[0] - B[0]); const uint16_t *m = eto_mono + (long)yy * ms + ETO_PAD;
			if (opaque_bk) eto_op(o, m, tw, cv); else eto_tr(o, m, tw, cv); }
	}
	if (dry) {
		if (nvn >= 8) { free(d0); return REJ(16); }
		NatVerify *v = &nv[nvn]; v->px = malloc(need * 2); v->s0 = calloc(need, 2);
		if (!v->px || !v->s0) { free(v->px); free(v->s0); free(d0); return REJ(16); }
		nvn++; v->ord = 14; v->x = B[0]; v->y = B[1]; v->w = bw; v->h = bh; v->d = D; v->ret = 0; v->d0 = d0;
		memcpy(v->px, ob, need * 2);
		return 40;
	}
	for (int yy = 0; yy < bh; yy++) if (!row_put(&D, B[1] + yy, B[0], bw, ob + (long)yy * bw)) return REJ(17);
	ddi_result = 0;
	return 40;
}

/* ---- GFX B29b: DIB transfers (DIBToDevice = SetDIBitsToDevice, DIBBlt = SetDIBits into a device bitmap) ----
 * BITMAPINFOHEADER (40 bytes) + RGBQUAD table; BI_RGB only; bottom-up (biHeight > 0) or top-down.  lpTranslate must be NULL (true colour
 * device).  Colour of a DIB entry -> 5-6-5 = dib565() (checked call by call against DIBENG by the PC verify). */
typedef struct { int w, h, bpp, stride, topdown, npal; uint16_t pal[256]; uint32_t rgb[256]; } DibHdr;
static DibHdr dib_h ETO_PS;
static uint8_t dib_row[NAT_MAXW * 4] ETO_PS __attribute__((aligned(16)));
static uint8_t dib_ct[1024] ETO_PS;
static uint16_t dib_out[NAT_MAXW] ETO_PS __attribute__((aligned(16)));   /* one converted row (streaming path) */
static inline uint16_t dib565(uint32_t rgb)   /* rgb = 0x00RRGGBB */
{
	/* colour-table entries (1/4/8 bpp) and ColorInfo: nearest 5/6-bit cell centre, i.e. (c - 4) >> 3 and (c - 2) >> 2 (0x80 -> 15,
	 * 0x33 -> 5, 0xb2 -> 21 / 44); found with the per-call verify.  24 bpp pixels truncate instead (c >> 3, 0x40 -> 8). */
	int r = rgb >> 16 & 255, g = rgb >> 8 & 255, b = rgb & 255;
	r = r < 4 ? 0 : (r - 4) >> 3; g = g < 2 ? 0 : (g - 2) >> 2; b = b < 4 ? 0 : (b - 4) >> 3;
	return (uint16_t)(r << 11 | g << 5 | b);
}
static int dib_hdr(uint32_t fp, DibHdr *b)
{
	uword l; uint8_t h[40]; if (!fp || !g_lin(fp, &l, 40) || !g_read(l, h, 40)) return 0;
	if (rd32(h) != 40 || rd16(h + 12) != 1 || rd32(h + 16) != 0) return 0;   /* BI_RGB only */
	b->w = (int32_t)rd32(h + 4); int32_t hh = (int32_t)rd32(h + 8); b->topdown = hh < 0; b->h = hh < 0 ? -hh : hh; b->bpp = rd16(h + 14);
	if (b->w <= 0 || b->w > NAT_MAXW || !b->h || (b->bpp != 1 && b->bpp != 4 && b->bpp != 8 && b->bpp != 24)) return 0;
	b->stride = ((b->w * b->bpp + 31) >> 5) * 4; b->npal = 0;
	if (b->bpp <= 8) {
		int n = rd32(h + 32); if (!n) n = 1 << b->bpp; if (n > (1 << b->bpp)) return 0;
		uint8_t *c = dib_ct; if (!g_read(l + 40, c, n * 4)) return 0;
		for (int i = 0; i < n; i++) { b->rgb[i] = c[i * 4] | c[i * 4 + 1] << 8 | c[i * 4 + 2] << 16; b->pal[i] = dib565(b->rgb[i]); }
		for (int i = n; i < (1 << b->bpp); i++) { b->rgb[i] = 0; b->pal[i] = 0; }
		b->npal = n;
	}
	return 1;
}
/* one DIB scan (buffer index i) -> 16 bpp pixels [x0, x0+n) */
static int dib_scan565(const DibHdr *b, uword bits, int i, int x0, int n, uint16_t *o)
{
	int bo = (x0 * b->bpp) >> 3, be = ((x0 + n) * b->bpp + 7) >> 3;
	if (!g_read(bits + (uword)i * b->stride + bo, dib_row, be - bo)) return 0;
	const uint8_t *r = dib_row;
	switch (b->bpp) {
	case 24: gk.rgb24(o, r, n); break;   /* DIBENG: 24 bpp pixels truncate (0x40 -> 8); colour tables use the cell centre (dib565) */
	case 8: for (int k = 0; k < n; k++) o[k] = b->pal[r[k]]; break;
	case 4: for (int k = 0; k < n; k++) { int x = x0 + k - (bo << 1); o[k] = b->pal[(r[x >> 1] >> (x & 1 ? 0 : 4)) & 15]; } break;
	default: for (int k = 0; k < n; k++) { int x = x0 + k - (bo << 3); o[k] = b->pal[(r[x >> 3] >> (7 - (x & 7))) & 1]; }
	}
	return 1;
}
static int nat_store(int ord, const Surf *D, int x, int y, int w, int h, uint16_t *ob, uint32_t ret, int dry)
{
	long need = (long)w * h;
	if (dry) {
		if (nvn >= 8) return REJ(16);
		NatVerify *v = &nv[nvn]; v->px = malloc(need * 2); v->s0 = calloc(need, 2); v->d0 = calloc(need, 2);
		if (!v->px || !v->s0 || !v->d0) { free(v->px); free(v->s0); free(v->d0); return REJ(16); }
		nvn++; v->ord = ord; v->x = x; v->y = y; v->w = w; v->h = h; v->d = *D; v->ret = ret;
		memcpy(v->px, ob, need * 2); return 1;
	}
	for (int yy = 0; yy < h; yy++) if (!row_put(D, y + yy, x, w, ob + (long)yy * w)) return REJ(17);
	ddi_result = ret; return 1;
}
/* DIBToDevice(lpDestDev, X, Y, iScan, cScans, lpClipRect, lpDrawMode, lpDIBits, lpBitmapInfo, lpTranslate) - 32 bytes, returns cScans */
static __attribute__((noinline)) int nat_dib2dev(const uint8_t *a, int dry)
{
	gk_sel();
	DibHdr *const b = &dib_h; Surf D; int C[4];
	int X = (int16_t)rd16(a + 26), Y = (int16_t)rd16(a + 24), is = rd16(a + 22), cs = rd16(a + 20);
	if (rd32(a)) return REJ(30);                                       /* palette translate */
	if (!dib_hdr(rd32(a + 4), b)) return REJ(31);
	if (!surf_get(rd32(a + 28), &D) || D.bpp != 16) return REJ(22);
	if (D.vram && gfx_swcursor) return REJ(4);
	if (!rect_rd(rd32(a + 16), C)) return REJ(23);
	if (is + cs > b->h) return REJ(32);
	int S[4] = { 0, 0, D.w, D.h }, A[4];
	/* rows (from the DIB top) held in the buffer: bottom-up scan i is row h-1-iScan-i */
	A[0] = X; A[2] = X + b->w;
	if (b->topdown) { A[1] = Y + is; A[3] = Y + is + cs; } else { A[1] = Y + b->h - is - cs; A[3] = Y + b->h - is; }
	if (!rect_and(A, C) || !rect_and(A, S)) { if (dry) return REJ(39); ddi_result = cs; return 32; }
	int w = A[2] - A[0], h = A[3] - A[1]; long need = (long)w * h;
	uword bits; if (!g_lin32(rd32(a + 8) >> 16, rd32(a + 8) & 0xffff, &bits, (uword)cs * b->stride)) return REJ(33);
	{ int bad11 = 0; for (int yy = A[1]; yy < A[3]; yy++) if (!row_check(&D, yy, A[0], w, !dry)) bad11 = 1; if (bad11) return REJ(11); }
	if (!dry) {   /* one row at a time: DIB row -> 5-6-5 -> destination */
		uint16_t *const rb = dib_out;
		for (int yy = 0; yy < h; yy++) {
			int r = A[1] + yy - Y, i = b->topdown ? r - is : b->h - 1 - r - is;
			if (!dib_scan565(b, bits, i, A[0] - X, w, rb) || !row_put(&D, A[1] + yy, A[0], w, rb)) return REJ(34);
		}
		ddi_result = cs; return 32;
	}
	if (need > nat_tmpn) { free(nat_tmp); nat_tmp = malloc(need * 2 * sizeof *nat_tmp); nat_tmpn = nat_tmp ? need : 0; if (!nat_tmp) return REJ(12); }
	for (int yy = 0; yy < h; yy++) {
		int r = A[1] + yy - Y, i = b->topdown ? r - is : b->h - 1 - r - is;
		if (!dib_scan565(b, bits, i, A[0] - X, w, nat_tmp + (long)yy * w)) return REJ(34);
	}
	return nat_store(21, &D, A[0], A[1], w, h, nat_tmp, cs, dry) ? 32 : 0;
}
/* DIBBlt(lpBitmap, fGet, iStart, cScans, lpDIBits, lpBitmapInfo, lpDrawMode, lpTranslate) - 26 bytes; set direction only, returns cScans */
static __attribute__((noinline)) int nat_dibblt(const uint8_t *a, int dry)
{
	gk_sel();
	DibHdr *const b = &dib_h; Surf D;
	int get = rd16(a + 20), is = rd16(a + 18), cs = rd16(a + 16);
	if (get) return REJ(35);
	if (rd32(a)) return REJ(30);
	if (!dib_hdr(rd32(a + 8), b)) return REJ(31);
	if (!surf_get(rd32(a + 22), &D)) return REJ(1);
	if (D.vram && gfx_swcursor) return REJ(4);
	if (b->w != D.w || b->h != D.h) return REJ(36);
	if (D.bpp == 1) { if (b->bpp != 1) return REJ(37); for (int i = 0; i < 2; i++) { if (b->rgb[i] != 0 && b->rgb[i] != 0xffffff) return REJ(37); b->pal[i] = b->rgb[i] ? 1 : 0; } }
	else if (D.bpp != 16) return REJ(38);
	if (is + cs > b->h) cs = b->h - is;
	if (cs <= 0) return REJ(32);
	int y0 = b->topdown ? is : b->h - is - cs, w = D.w, h = cs;
	long need = (long)w * h;
	uword bits; if (!g_lin32(rd32(a + 12) >> 16, rd32(a + 12) & 0xffff, &bits, (uword)cs * b->stride)) return REJ(33);
	{ int bad11 = 0; for (int yy = y0; yy < y0 + h; yy++) if (!row_check(&D, yy, 0, w, !dry)) bad11 = 1; if (bad11) return REJ(11); }
	if (!dry) {
		uint16_t *const rb = dib_out;
		for (int yy = 0; yy < h; yy++) {
			int r = y0 + yy, i = b->topdown ? r - is : b->h - 1 - r - is;
			if (!dib_scan565(b, bits, i, 0, w, rb) || !row_put(&D, r, 0, w, rb)) return REJ(34);
		}
		ddi_result = rd16(a + 16); return 26;
	}
	if (need > nat_tmpn) { free(nat_tmp); nat_tmp = malloc(need * 2 * sizeof *nat_tmp); nat_tmpn = nat_tmp ? need : 0; if (!nat_tmp) return REJ(12); }
	for (int yy = 0; yy < h; yy++) {
		int r = y0 + yy, i = b->topdown ? r - is : b->h - 1 - r - is;
		if (!dib_scan565(b, bits, i, 0, w, nat_tmp + (long)yy * w)) return REJ(34);
	}
	return nat_store(19, &D, 0, y0, w, h, nat_tmp, rd16(a + 16), dry) ? 26 : 0;
}

/* ---- GFX B29b: ColorInfo(lpDestDev, ColorIn, lpPhysBits) - 12 bytes (DIBENG seg 3 entry 2, read instruction by instruction) ----
 * 16 bpp 5-6-5 device (deFlags & 0x40, no palette translate) and mono BITMAP (bmType 0, 1 bpp, word +0x1c == 0):
 *   lpPhysBits != NULL: ColorIn & 0xff000000 -> phys = ColorIn | 0xffff0000, DX:AX = phys;
 *     ColorIn 0 / 0xffffff -> phys = ColorIn | 0x40000000, AX = low word, DX = low byte;
 *     else 5-6-5: (c - 4) >> 3 / (c - 2) >> 2 clamped at 0 (DIBENG 0x2c4), DX:AX = that colour expanded back (tables 0x167 / 0x187);
 *     mono: avg(avg(B, R), G) >= 0x7f -> 1 (DIBENG 0x21e), phys = (0x80|bit) << 24 | bit, DX:AX = 0 or 0x00ff:ffff.
 *   lpPhysBits == NULL: physical -> RGB (DIBENG 0x1ee / 0x137). */
static const uint8_t ci_t5[32] = { 0x00,0x10,0x18,0x20,0x28,0x30,0x38,0x40,0x48,0x50,0x58,0x60,0x68,0x70,0x78,0x80,0x88,0x90,0x98,0xa0,0xa8,0xb0,0xb8,0xc0,0xc8,0xd0,0xd8,0xe0,0xe8,0xf0,0xf8,0xff };
static const uint8_t ci_t6[64] = { 0x00,0x08,0x0c,0x10,0x14,0x18,0x1c,0x20,0x24,0x28,0x2c,0x30,0x34,0x38,0x3c,0x40,0x44,0x48,0x4c,0x50,0x54,0x58,0x5c,0x60,0x64,0x68,0x6c,0x70,0x74,0x78,0x7c,0x80,
	0x84,0x88,0x8c,0x90,0x94,0x98,0x9c,0xa0,0xa4,0xa8,0xac,0xb0,0xb4,0xb8,0xbc,0xc0,0xc4,0xc8,0xcc,0xd0,0xd4,0xd8,0xdc,0xe0,0xe4,0xe8,0xec,0xf0,0xf4,0xf8,0xfc,0xff };
static int g_write(uword lin, const void *buf, int n)
{
	const uint8_t *s = buf;
	for (int i = 0; i < n; i++) { uint8_t *h = g_host(lin + i, 1); if (!h) return 0; }
	for (int i = 0; i < n; i++) *g_host(lin + i, 1) = s[i];
	return 1;
}
static __attribute__((noinline)) int nat_colorinfo(const uint8_t *a, int dry)
{
	uint32_t pp = rd32(a), c = rd32(a + 4), ph = 0, ret; uword l; uint8_t h[0x20];
	if (!rd32(a + 8) || !g_lin(rd32(a + 8), &l, 0x20) || !g_read(l, h, 0x20)) return REJ(40);
	int mono;
	if (rd16(h) == 0x5250 && h[9] == 16 && (rd16(h + 0x1c) & 0x40) && !(rd16(h + 0x1c) & 4)) mono = 0;
	else if (rd16(h) == 0 && h[9] == 1 && rd16(h + 0x1c) == 0) mono = 1;
	else return REJ(41);
	if (pp >> 16) {
		if (c & 0xff000000) { ph = c | 0xffff0000; ret = ph; }
		else if (c == 0 || c == 0xffffff) { ph = c | 0x40000000; ret = (c & 0xff) << 16 | (ph & 0xffff); }
		else if (mono) {
			unsigned s = (c >> 16 & 255) + (c & 255), v = s >> 1;   /* rcr bl,1 keeps the carry of the add */
			s = v + (c >> 8 & 255); v = s >> 1; int bit = v >= 0x7f;
			ph = (0x80u | bit) << 24 | bit; ret = bit ? 0x00ffffff : 0;
		} else {
			int R = c & 255, G = c >> 8 & 255, B = c >> 16 & 255;
			int r = R < 4 ? 0 : (R - 4) >> 3, g = G < 2 ? 0 : (G - 2) >> 2, b = B < 4 ? 0 : (B - 4) >> 3;
			ph = r << 11 | g << 5 | b; ret = (uint32_t)ci_t5[b] << 16 | ci_t6[g] << 8 | ci_t5[r];
		}
		uword pl; if (!g_lin(pp, &pl, 4)) return REJ(42);
		uint8_t pb[4] = { ph, ph >> 8, ph >> 16, ph >> 24 };
		if (dry) {
			if (nvn >= 8) return REJ(16);
			NatVerify *v = &nv[nvn++]; memset(v, 0, sizeof *v); v->ord = 2; v->ret = ret; v->fullret = 1; v->mlin = pl; memcpy(v->mexp, pb, 4); v->mlen = 4; return 12;
		}
		if (!g_write(pl, pb, 4)) return REJ(42);
	} else {
		if (mono) ret = (c & 1) ? 0x00ffffff : 0;
		else ret = (uint32_t)ci_t5[c & 31] << 16 | ci_t6[c >> 5 & 63] << 8 | ci_t5[c >> 11 & 31];
		if (dry) { if (nvn >= 8) return REJ(16); NatVerify *v = &nv[nvn++]; memset(v, 0, sizeof *v); v->ord = 2; v->ret = ret; v->fullret = 1; return 12; }
	}
	ddi_result = ret; return 12;
}

/* StretchDIBits(lpPDevice, fGet, DestX, DestY, DestXE, DestYE, SrcX, SrcY, SrcXE, SrcYE, lpBits, lpBitmapInfo, lpTranslate, dwRop, lpBrush,
 * lpDrawMode, lpClipRect) - 50 bytes.  Native: set direction, 1:1 size, ROPs without pattern, 16 bpp destination, BI_RGB source -> returns
 * DestYE.  DIBENG answers -1 (GDI then simulates) for RLE sources and for stretched blits into mono bitmaps: native answers the same. */
static __attribute__((noinline)) int nat_stretchdib(const uint8_t *a, int dry)
{
	gk_sel();
	DibHdr *const b = &dib_h; Surf D; int C[4]; uword l; uint8_t h[40];
	int dx = (int16_t)rd16(a + 42), dy = (int16_t)rd16(a + 40), dw = (int16_t)rd16(a + 38), dh = (int16_t)rd16(a + 36);
	int sx = (int16_t)rd16(a + 34), sy = (int16_t)rd16(a + 32), sw = (int16_t)rd16(a + 30), shh = (int16_t)rd16(a + 28);
	uint8_t r = rd32(a + 12) >> 16;
	if (rd16(a + 44)) return REJ(35);
	if (rd32(a + 16)) return REJ(30);
	if (!rd32(a + 20) || !g_lin(rd32(a + 20), &l, 40) || !g_read(l, h, 40) || rd32(h) != 40) return REJ(31);
	if (!surf_get(rd32(a + 46), &D)) return REJ(1);
	int stretch = dw != sw || dh != shh;
	if (rd32(h + 16) == 1 || rd32(h + 16) == 2 || (stretch && D.bpp == 1 && !D.npal)) {   /* RLE8/RLE4 source, or stretch into a mono BITMAP */
		if (D.bpp == 1 && !stretch && rd32(h + 16) == 0) return REJ(43);
		if (dry) { if (nvn >= 8) return REJ(16); NatVerify *v = &nv[nvn++]; memset(v, 0, sizeof *v); v->ord = 28; v->ret = 0xffff; return 50; }
		ddi_result = 0xffff; return 50;
	}
	if (stretch) return REJ(44);
	if (D.bpp != 16) return REJ(38);
	if (((r >> 4) ^ r) & 0x0f) return REJ(45);                              /* ROP uses the pattern */
	if (D.vram && gfx_swcursor) return REJ(4);
	if (!dib_hdr(rd32(a + 20), b)) return REJ(31);
	if (!rect_rd(rd32(a), C)) return REJ(23);
	if (sx < 0 || sy < 0 || sx + sw > b->w || sy + shh > b->h || dw <= 0 || dh <= 0) return REJ(46);
	int S[4] = { 0, 0, D.w, D.h }, A[4] = { dx, dy, dx + dw, dy + dh };
	if (!rect_and(A, C) || !rect_and(A, S)) return REJ(39);
	int w = A[2] - A[0], hh = A[3] - A[1]; long need = (long)w * hh;
	uword bits; if (!g_lin32(rd32(a + 24) >> 16, rd32(a + 24) & 0xffff, &bits, (uword)b->h * b->stride)) return REJ(33);
	{ int bad11 = 0; for (int yy = A[1]; yy < A[3]; yy++) if (!row_check(&D, yy, A[0], w, !dry)) bad11 = 1; if (bad11) return REJ(11); }
	if (need > nat_tmpn) { free(nat_tmp); nat_tmp = malloc(need * 2 * sizeof *nat_tmp); nat_tmpn = nat_tmp ? need : 0; if (!nat_tmp) return REJ(12); }
	uint16_t *ob = nat_tmp, *sb = nat_tmp + need;
	int useD = ((r >> 1) ^ r) & 0x55;
	for (int yy = 0; yy < hh; yy++) {
		int row = sy + (A[1] - dy) + yy;                                     /* top-down: source row from the DIB top */
		int i = b->topdown ? row : sy + shh - 1 - (A[1] - dy) - yy;            /* bottom-up: SrcY counts from the bottom (lower-left origin) */
		uint16_t *o = ob + (long)yy * w;
		if (!dib_scan565(b, bits, i, sx + (A[0] - dx), w, sb)) return REJ(34);
		if (useD && !row_get(&D, A[1] + yy, A[0], w, o)) return REJ(15);
		if (r == 0xcc) memcpy(o, sb, w * 2);
		else for (int k = 0; k < w; k++) o[k] = rop3(r, 0, sb[k], o[k]) & 0xffff;
	}
	return nat_store(28, &D, A[0], A[1], w, hh, ob, dh, dry) ? 50 : 0;
}

static int nat_call(int ord, const uint8_t *a, int dry)
{
	if (gfx_ddi_native == 5 && ord != 1 && ord != 14) return 0;   /* nat 5 = master's native set (A/B side) */
	switch (ord) {
	case 2: return nat_colorinfo(a, dry);
	/* DIBENG SelectBitmap (seg 3 0xd4d) is "mov ax,1; retf 0x10" and our UserRepaintDisable body is "xor ax,ax; lret 2": the host answers
	 * the same AX without entering x86 (DX is not part of either result) */
	case 29: if (dry) { if (nvn >= 8) return REJ(16); NatVerify *v = &nv[nvn++]; memset(v, 0, sizeof *v); v->ord = 29; v->ret = 1; return 16; } ddi_result = 1; return 16;
	case 500: if (dry) { if (nvn >= 8) return REJ(16); NatVerify *v = &nv[nvn++]; memset(v, 0, sizeof *v); v->ord = 500; v->ret = 0; return 2; } ddi_result = 0; return 2;
	case 19: return nat_dibblt(a, dry);
	case 28: return nat_stretchdib(a, dry);
	case 21: return nat_dib2dev(a, dry);
	case 1: return nat_bitblt(a, dry);
	case 14: return gfx_ddi_native == 2 && !dry ? 0 : nat_eto(a, dry);   /* nat 2 = A/B side without native ExtTextOut */
	}
	return 0;
}

#ifdef ESP_PLATFORM
static int nat_verify(int ord, const uint8_t *a, uint32_t ret) { (void)ord; (void)a; (void)ret; return 0; }
#else
/* PC verify: the dry-run result of the newest open call of this ordinal vs what DIBENG left in the destination */
static long nat_vlog;
static int nat_verify(int ord, const uint8_t *a, uint32_t ret)
{
	if (!nvn || nv[nvn - 1].ord != ord) return 0;
	NatVerify *v = &nv[--nvn]; int bad = 0, first = -1, shown = 0; static uint16_t row[NAT_MAXW];
	if (!v->px) {   /* no surface: guest bytes written + full DX:AX */
		uint8_t m[64]; if (v->mlen && (!g_read(v->mlin, m, v->mlen) || memcmp(m, v->mexp, v->mlen))) bad = 1;
		if (v->fullret ? ret != v->ret : (ret & 0xffff) != (v->ret & 0xffff)) bad = 1;
		if (bad && nat_vlog++ < 400) { fprintf(stderr, "VERIFY MISMATCH ord %d ret dibeng %08x native %08x mem", ord, (unsigned)ret, (unsigned)v->ret);
			for (int i = 0; i < v->mlen; i++) fprintf(stderr, " %02x/%02x", m[i], v->mexp[i]); fputc(10, stderr); }
		return bad ? -1 : 1;
	}
	for (int y = 0; y < v->h; y++) {
		if (!row_get(&v->d, v->y + y, v->x, v->w, row)) { bad = 1; break; }
		for (int i = 0; i < v->w; i++) { long k = (long)y * v->w + i; if (row[i] != v->px[k]) {
			bad++; if (first < 0) first = k;
			if (nat_vlog < 400 && shown++ < 6) { fprintf(stderr, "  px (%d,%d) src %04x dst0 %04x native %04x dibeng %04x row src:", i, y, v->s0[k], v->d0[k], v->px[k], row[i]);
				for (int j = 0; j < v->w; j++) { fprintf(stderr, " %04x", v->s0[(long)y * v->w + j]); }
				if (y) { fprintf(stderr, " | prev row:"); for (int j = 0; j < v->w; j++) fprintf(stderr, " %04x", v->s0[(long)(y - 1) * v->w + j]); } fputc(10, stderr); } } }
	}
	if (v->d.bpp == 4 && (rd32(a + 8) >> 16) == 0x66 && getenv("B29B_PAIRS")) {   /* PC: every (5-6-5 source, DIBENG index) pair, for the colour-match study */
		static FILE *pf; static uint8_t seen[65536]; if (!pf) pf = fopen(getenv("B29B_PAIRS"), "w");
		for (int y = 0; y < v->h && pf; y++) { if (!row_get(&v->d, v->y + y, v->x, v->w, row)) break;
			for (int i = 0; i < v->w; i++) { long k = (long)y * v->w + i; uint16_t c = v->s0[k]; if (!seen[c]) { seen[c] = 1; fprintf(pf, "%04x %d\n", c, (row[i] ^ v->d0[k]) & 15); } } }
		if (pf) fflush(pf);
	}
	if (bad && (ord == 19 || ord == 21)) { fprintf(stderr, "  dib bpp %d npal %d %dx%d\n", dib_h.bpp, dib_h.npal, dib_h.w, dib_h.h);
		static int sh; for (int y = 0; y < v->h && sh < 40; y++) { if (!row_get(&v->d, v->y + y, v->x, v->w, row)) break;
			for (int i = 0; i < v->w && sh < 40; i++) { uint16_t nn = v->px[(long)y * v->w + i]; if (row[i] == nn) continue;
				for (int k = 0; k < dib_h.npal; k++) if (dib_h.pal[k] == nn) { fprintf(stderr, "  PAL rgb %06x native %04x dibeng %04x\n", (unsigned)dib_h.rgb[k], nn, row[i]); sh++; break; } } } }
	if (bad && nat_vlog++ < 400) fprintf(stderr, "VERIFY MISMATCH ord %d rop %06x at %d,%d %dx%d bpp %d bad %d first (%d,%d)\n", ord, (unsigned)rd32(a + 8), v->x, v->y, v->w, v->h, v->d.bpp, bad,
		first % v->w, first / v->w);
	if (!bad && (ret & 0xffff) != (v->ret & 0xffff)) { bad = 1; if (nat_vlog++ < 200) fprintf(stderr, "VERIFY RETURN ord %d dibeng %08x native %08x\n", ord, (unsigned)ret, (unsigned)v->ret); }
	free(v->px); free(v->s0); free(v->d0);
	return bad ? -1 : 1;
}
#endif

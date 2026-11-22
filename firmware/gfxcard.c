/* BLUE B29a: paravirtual display card "GFX95" (own design).  Host side of the Win95 minidriver B29GFX.DRV.
 * Interface = two 16-bit I/O ports: INDEX 0xE8, DATA 0xEA (see DESIGN.md).  The linear framebuffer is the PCI VGA BAR0
 * (plain guest memory registered with cpu_register_mem: no per-access trap); this file only owns mode switch, pointer overlay
 * and damage/profile counters.  The mode switch drives the existing VBE LFB scan-out model (vga.c) so the PC build shows frames. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pc.h"
#include "vga.h"
#include "gfxcard.h"

GfxCard gfx;
static int gfx_on;   /* display = gfx95 in the ini: without it the card does not exist for the guest */
static void gfx_damage_add(int l, int t, int r, int b);
#ifdef GFX95_TEST   /* PC test harness only (shadow-panel model, damage-vs-pixels check): not part of the firmware */
extern long dbg_cycle_get(void);
#endif

#define DT_W 80
#define DT_H 50
static volatile uint8_t dt2[DT_W * DT_H];
static void gfx_apply_mode(PC *pc)
{
	if (gfx.ctrl & 1) {
		for (int i = 0; i < DT_W * DT_H; i++) dt2[i] = 1;   /* a (new) mode shows the whole surface: every tile is dirty */
		vbe_write(pc->vga, 0, 4); vbe_write(pc->vga, 1, 0);          /* disable first: clean reprogram */
		vbe_write(pc->vga, 0, 1); vbe_write(pc->vga, 1, gfx.w);
		vbe_write(pc->vga, 0, 2); vbe_write(pc->vga, 1, gfx.h);
		vbe_write(pc->vga, 0, 3); vbe_write(pc->vga, 1, gfx.bpp);
		vbe_write(pc->vga, 0, 4); vbe_write(pc->vga, 1, 0x01 | 0x40 | 0x80); /* enabled | LFB | keep memory */
		gfx.mode_sets++;
	} else {
		vbe_write(pc->vga, 0, 4); vbe_write(pc->vga, 1, 0x80);         /* leave VBE (text path comes back via BIOS INT 10h) */
	}
}

#include "gfxddi.c"   /* RED B29b: DDI stubs -> host (profile, native executor) */
static uint16_t gfx_reg_read(PC *pc)
{
	switch (gfx.index) {
	case 0x00: return 0xB295;
	case 0x01: return 1;
	case 0x02: return (uint16_t)pc->pci_vga_ram_addr;
	case 0x03: return (uint16_t)(pc->pci_vga_ram_addr >> 16);
	case 0x04: return (uint16_t)(pc->vga_mem_size >> 16);
	case 0x10: return gfx.w;
	case 0x11: return gfx.h;
	case 0x12: return gfx.bpp;
	case 0x13: return (uint16_t)(((gfx.w * gfx.bpp / 8) + 127) & ~127);
	case 0x15: return gfx.ctrl & 1;
	case 0x50: case 0x5a: return ddi_status;
	case 0x52: return ddi_nargs;
	case 0x53: ddi_open = 0; return (uint16_t)ddi_result;
	case 0x54: return (uint16_t)(ddi_result >> 16);
	case 0x55: if (ddi_tr < ddi_tn) return ddi_tl[ddi_tr].fl; return 0;   /* touch list: flags of the next entry */
	case 0x56: return ddi_tl[ddi_tr < 32 ? ddi_tr : 0].sel;
	case 0x58: return (uint16_t)(ddi_tl[ddi_tr < 32 ? ddi_tr : 0].off >> 16);
	case 0x57: { uint16_t v = (uint16_t)ddi_tl[ddi_tr < 32 ? ddi_tr : 0].off; ddi_tr++; return v; }
	default: return 0xffff;
	}
}

static void shape_commit(void)
{
	gfx.shape_hash = 2166136261u;
	for (int i = 0; i < GFX_SHAPE_WORDS; i++) gfx.shape_hash = (gfx.shape_hash ^ gfx.shape[i]) * 16777619u;
	if (gfx.shape_hash != gfx.last_hash) gfx.shape_changes++;
	gfx.last_hash = gfx.shape_hash;
	gfx.shapes++;
}

#ifdef GFX95_TEST   /* PC test harness only */
/* PC model of the board presenter (exit-based rule): rects reported at call entry are copied vram -> pshadow when the last GDI call exits
 * (depth 0).  gfx_damage_check() then counts rows where pshadow != vram = pixels the presenter would have shown stale. */
static uint8_t *pshadow; static int16_t prects[256][4]; static int npr, pr_all; long pm_released, pm_copies;
static void pm_add(int l, int t, int r, int b)
{
	if (l > r) { int x = l; l = r; r = x; }
	if (t > b) { int x = t; t = b; b = x; }
	if (l < 0) { l = 0; }
	if (t < 0) { t = 0; }
	if (r >= gfx.w) { r = gfx.w - 1; }
	if (b >= gfx.h) { b = gfx.h - 1; }
	if (r < l || b < t) { return; }
	if (npr == 256) { pr_all = 1; return; }
	prects[npr][0] = l; prects[npr][1] = t; prects[npr][2] = r; prects[npr][3] = b; npr++;
}
static void pm_release(const uint8_t *vram)
{
	int pitch = ((gfx.w * 2) + 127) & ~127; if (!pshadow) pshadow = calloc(1, (size_t)pitch * gfx.h);
	if (pr_all) { memcpy(pshadow, vram, (size_t)pitch * gfx.h); }
	for (int i = 0; i < npr && !pr_all; i++) for (int y = prects[i][1]; y <= prects[i][3]; y++)
		memcpy(pshadow + (size_t)y * pitch + prects[i][0] * 2, vram + (size_t)y * pitch + prects[i][0] * 2, (size_t)(prects[i][2] - prects[i][0] + 1) * 2);
	pm_copies += npr; npr = 0; pr_all = 0; pm_released++;
}
long pm_stale_rows(const uint8_t *vram)
{
	int pitch = ((gfx.w * 2) + 127) & ~127; long n = 0; if (!pshadow) return -1;
	for (int y = 0; y < gfx.h; y++) if (memcmp(vram + (size_t)y * pitch, pshadow + (size_t)y * pitch, (size_t)gfx.w * 2)) n++;
	return n;
}
#endif

/* ---- LFB write tracking: 16x16 tiles of the 1280x800 surface (pitch 2560 B).  Marked from pc.c iomem_write*, taken by the presenter.
 * The presenter clears a tile BEFORE it reads the pixels, so a write that lands during the present re-marks the tile for the next one. */
void gfx_lfb_dirty(uint32_t off, int len)
{
	gfx_lfb_wr += len;
	if (!(gfx.ctrl & 1) || len <= 0) return;
	while (len > 0) {
		const uint32_t pitch = (uint32_t)gfx.w * 2;   /* 2560 / 2048 / 1280 B: the LFB stride is 2 * width in all three modes */
		uint32_t y = off / pitch, ro = off % pitch;
		if (y >= (uint32_t)gfx.h) return;
		int n = (int)(pitch - ro); if (n > len) n = len;
		uint32_t x0 = ro >> 1, x1 = (ro + n - 1) >> 1;
		uint8_t *row = (uint8_t *)&dt2[(y >> 4) * DT_W];
		for (uint32_t tx = x0 >> 4; tx <= (x1 >> 4); tx++) row[tx] = 1;
		off += n; len -= n;
	}
}
/* tiles -> inclusive pixel rects (row runs, merged vertically when the run is identical); more than max rects are unioned into the last one */
int gfx_take_dirty(int16_t (*r)[4], int max)
{
	int n = 0, tw = (gfx.w + 15) / 16, th = (gfx.h + 15) / 16;
	if (tw > DT_W) { tw = DT_W; }
	if (th > DT_H) { th = DT_H; }
	for (int ty = 0; ty < th; ty++) {
		volatile uint8_t *row = &dt2[ty * DT_W];
		for (int tx = 0; tx < tw; ) {
			if (!row[tx]) { tx++; continue; }
			int e = tx; while (e + 1 < tw && row[e + 1]) e++;
			for (int k = tx; k <= e; k++) row[k] = 0;
			int x0 = tx * 16, x1 = e * 16 + 15, y0 = ty * 16, y1 = ty * 16 + 15, merged = 0;
			for (int i = 0; i < n; i++) if (r[i][0] == x0 && r[i][2] == x1 && r[i][3] == y0 - 1) { r[i][3] = y1; merged = 1; break; }
			if (!merged) {
				if (n == max) { int16_t *l = r[n - 1]; if (x0 < l[0]) l[0] = x0; if (y0 < l[1]) l[1] = y0; if (x1 > l[2]) l[2] = x1; if (y1 > l[3]) l[3] = y1; }
				else { r[n][0] = x0; r[n][1] = y0; r[n][2] = x1; r[n][3] = y1; n++; }
			}
			tx = e + 1;
		}
	}
	return n;
}
#ifdef GFX95_TEST
/* PC model of the presenter: take the dirty tiles and copy them vram -> pshadow (called at arbitrary times by the harness) */
long gfx_model_take(const uint8_t *vram)
{
	int16_t r[24][4]; int n = gfx_take_dirty(r, 24);
	int pitch = gfx.w * 2; if (!pshadow) pshadow = calloc(1, (size_t)2560 * 800);
	for (int i = 0; i < n; i++) for (int y = r[i][1]; y <= r[i][3]; y++) memcpy(pshadow + (size_t)y * pitch + r[i][0] * 2, vram + (size_t)y * pitch + r[i][0] * 2, (size_t)(r[i][2] - r[i][0] + 1) * 2);
	return n;
}
#endif

void gfx_reg_write(PC *pc, uint16_t v)
{
	switch (gfx.index) {
	case 0x10: gfx.w = v; break;
	case 0x11: gfx.h = v; break;
	case 0x12: gfx.bpp = v; break;
	case 0x14: gfx.ctrl = v; gfx_apply_mode(pc); break;
	case 0x20: gfx.gx = (int16_t)v; break;
	case 0x21: { int y = (int16_t)v; if (gfx.gx != gfx.cx || y != gfx.cy) gfx.mismatch += (gfx.pred_valid && (gfx.gx != gfx.cx || y != gfx.cy)); gfx.cx = gfx.gx; gfx.cy = y; gfx.pred_valid = 0; gfx.moves++; break; }
	case 0x22: gfx.cvis = v & 1; gfx.ctl_writes++; break;
	case 0x23: gfx.shape[gfx.sp] = v; if (++gfx.sp >= GFX_SHAPE_WORDS) { gfx.sp = 0; shape_commit(); } break;
	case 0x24: gfx.sp = 0; break; /* reset shape stream */
	case 0x30: gfx.dm[gfx.dp++] = (int16_t)v; if (gfx.dp == 4) { gfx.dp = 0; gfx.depth++; gfx_damage_add(gfx.dm[0], gfx.dm[1], gfx.dm[2], gfx.dm[3]); 
#ifdef GFX95_TEST
		pm_add(gfx.dm[0], gfx.dm[1], gfx.dm[2], gfx.dm[3]);
#endif
		} break;
	case 0x31: if (gfx.depth > 0) gfx.depth--; break;
	case 0x50: ddi_touch_ok = 0; ddi_enter(pc, v); break;
	case 0x5a: ddi_touch_ok = 1; ddi_enter(pc, v); break;   /* GFX B29b stub: same enter, and it can take status 2 (touch list) */
	case 0x51: ddi_exit(pc, v); break;
	case 0x55: gfx_swcursor = v & 1; break;
#ifdef GFX95_TEST
	case 0x3f: if ((v & 0xff00) == 0xd000) { gfx.dbgn[v & 0xff]++; break; } fprintf(stderr, "GFX_DBG %04x cyc=%ld\n", v, dbg_cycle_get()); break;
	case 0x40: if ((v & 0xff) < GFX_NPROF) { gfx.prof_t0[v & 0xff] = dbg_cycle_get(); } break;
	case 0x41: if ((v & 0xff) < GFX_NPROF) { int i = v & 0xff; gfx.prof_n[i]++; gfx.prof_c[i] += dbg_cycle_get() - gfx.prof_t0[i]; } break;
#endif
	}
}

uint32_t gfx_io_read(void *pcv, int addr, int size)
{
	if (!gfx_on) { return 0xffff; } PC *pc = pcv; (void)pc;
	if (addr == 0xE8) return gfx.index;
	if (addr == 0xEA) return gfx_reg_read(pc);
	return 0xffffffffu;
}

void gfx_io_write(void *pcv, int addr, uint32_t v, int size)
{
	if (!gfx_on) { return; } PC *pc = pcv; (void)pc;
	if (addr == 0xE8) { gfx_prev_index = gfx.index; gfx.index = (uint16_t)v; }
	else if (addr == 0xEA) gfx_reg_write(pc, (uint16_t)v);
}

int gfx_io_write_string(void *pcv, int addr, uint8_t *buf, int size, int count)
{
	if (!gfx_on) { return 0; } PC *pc = pcv; (void)pc;
	if (addr != 0xEA || size != 2) return 0;
	for (int i = 0; i < count; i++) gfx_reg_write(pc, (uint16_t)(buf[2 * i] | (buf[2 * i + 1] << 8)));
	return count;
}

void gfx_init(int enable) { memset(&gfx, 0, sizeof gfx); gfx.w = 1280; gfx.h = 800; gfx.bpp = 16; gfx_on = enable; }
int gfx_enabled(void) { return gfx_on; }

/* host-side prediction from the input event (zero guest instructions): pointer moves the moment the packet is injected */
void gfx_pred_move(int dx, int dy)
{
	/* Win95 default pointer acceleration measured by scripts/cal.scr: per axis |d| <= 2 -> d, else 2d */
	dx = (dx > 2 || dx < -2) ? 2 * dx : dx; dy = (dy > 2 || dy < -2) ? 2 * dy : dy;
	int bx = gfx.pq_n ? gfx.pqx[(gfx.pq_h + gfx.pq_n - 1) & 255] : gfx.cx, by = gfx.pq_n ? gfx.pqy[(gfx.pq_h + gfx.pq_n - 1) & 255] : gfx.cy;
	int nx = bx + dx, ny = by + dy;
	if (nx < 0) { nx = 0; } if (ny < 0) { ny = 0; } if (nx > gfx.w - 1) { nx = gfx.w - 1; } if (ny > gfx.h - 1) { ny = gfx.h - 1; }
	if (gfx.pq_n == 256) { gfx.pq_h = (gfx.pq_h + 1) & 255; gfx.pq_n--; }
	gfx.pqx[(gfx.pq_h + gfx.pq_n) & 255] = nx; gfx.pqy[(gfx.pq_h + gfx.pq_n) & 255] = ny; gfx.pq_n++;
	gfx.px = nx; gfx.py = ny; gfx.pred_valid = 1; gfx.pred_moves++;
}

/* B45: absolute host pointer from the pointer engine, in PANEL pixels (1280x800); scaled to the guest mode (1:1, 1.25, 2 - exact for the
 * three GFX95 sizes), queued like a predicted move and reconciled by MoveCursor */
void gfx_pred_abs(int px, int py)
{
	int nx = px * gfx.w / 1280, ny = py * gfx.h / 800;
	if (gfx.pq_n == 256) { gfx.pq_h = (gfx.pq_h + 1) & 255; gfx.pq_n--; }
	gfx.pqx[(gfx.pq_h + gfx.pq_n) & 255] = nx; gfx.pqy[(gfx.pq_h + gfx.pq_n) & 255] = ny; gfx.pq_n++;
	gfx.px = nx; gfx.py = ny; gfx.pred_valid = 1; gfx.pred_moves++;
}
/* B45: Windows moved the pointer itself (SetCursorPos ...): tell the engine, in panel pixels */
static void ptr_sync_guest(int x, int y) { extern void ptrdev_sync(int, int); if (gfx.w > 0 && gfx.h > 0) ptrdev_sync(x * 1280 / gfx.w, y * 800 / gfx.h); }

/* compose the visible frame: copy VRAM (RGB565) and apply the pointer AND/XOR shape; returns pitch bytes */
void gfx_compose(const uint8_t *vram, uint16_t *out, int overlay)
{
	int w = gfx.w, h = gfx.h, pitch = (((w * 2) + 127) & ~127);
	for (int y = 0; y < h; y++) memcpy(out + y * w, vram + y * pitch, w * 2);
	if (!overlay || !gfx.cvis) return;
	int hx = gfx.shape[0], hy = gfx.shape[1], cw = gfx.shape[2], ch = gfx.shape[3];
	const uint8_t *m = (const uint8_t *)&gfx.shape[4]; /* AND: 32 rows x 4 bytes, then XOR: 32 rows x 4 bytes (MSB = left) */
	int ox = (gfx.pred_valid ? gfx.px : gfx.cx) - hx, oy = (gfx.pred_valid ? gfx.py : gfx.cy) - hy;
	for (int y = 0; y < ch && y < 32; y++) for (int x = 0; x < cw && x < 32; x++) {
		int sx = ox + x, sy = oy + y; if (sx < 0 || sy < 0 || sx >= w || sy >= h) continue;
		int a = (m[y * 4 + (x >> 3)] >> (7 - (x & 7))) & 1, xr = (m[128 + y * 4 + (x >> 3)] >> (7 - (x & 7))) & 1;
		uint16_t *p = &out[sy * w + sx]; *p = (uint16_t)((*p & (a ? 0xffff : 0)) ^ (xr ? 0xffff : 0));
	}
}

/* dword port 0xEC: pointer position (y<<16|x) written by the driver's MoveCursor; moves the overlay and reconciles the host prediction */
void gfx_io_write32(void *pcv, int addr, uint32_t v)
{ PC *pc = pcv; (void)pc;
	if (!gfx_on) { return; }
	int x = (int16_t)(v & 0xffff), y = (int16_t)(v >> 16), best = -1, bd = 1 << 30;
	/* Windows can only produce pointer steps f(s) = s for |s| <= 2 and 2s above (3, 4, 5, 7 ... px are unreachable in one packet) and the
	 * input coalescer sends fewer, larger packets, so the guest position is a point NEAR the predicted path, not exactly on it.  Take the queued
	 * prediction closest to the reported position, drop everything up to it, keep the newer ones (the pointer stays ahead, no snap-back).  Only a
	 * distance > 6 px counts as a real mismatch (screen-edge clamp, a different acceleration setting). */
	for (int i = 0; i < gfx.pq_n; i++) { int qx = gfx.pqx[(gfx.pq_h + i) & 255], qy = gfx.pqy[(gfx.pq_h + i) & 255]; int d = (qx > x ? qx - x : x - qx) + (qy > y ? qy - y : y - qy); if (d < bd) { bd = d; best = i; } }
	if (best >= 0) { gfx.pq_h = (gfx.pq_h + best + 1) & 255; gfx.pq_n -= best + 1; }
	if (gfx.pred_moves && (best < 0 || bd > 6)) gfx.mismatch++;
	if (best >= 0 && bd > 6) { gfx.pq_n = 0; }
	if (!gfx.pq_n) gfx.pred_valid = 0;
	gfx.cx = x; gfx.cy = y; gfx.moves++;
	ptr_sync_guest(x, y);
}

/* damage reports (driver -> host) are only counted now: the presenter works from the LFB write tracking above (every guest write marks its tile),
 * so a GDI path that never reports damage cannot be missed.  The PC test harness (GFX95_TEST) still checks reported rects against changed pixels. */
#ifdef GFX95_TEST
#define TILE 16
#define TW 80
#define TH 50
static uint8_t dtile[TW * TH]; static uint8_t *shadow;
#endif
static void gfx_damage_add(int l, int t, int r, int b)
{
	gfx.damage_rects++;
	if (l > r) { int x = l; l = r; r = x; }
	if (t > b) { int x = t; t = b; b = x; }
	if (l < 0) { l = 0; } if (t < 0) { t = 0; } if (r >= gfx.w) { r = gfx.w - 1; } if (b >= gfx.h) { b = gfx.h - 1; }
	if (r < l || b < t) return;
	gfx.damage_px += (long)(r - l + 1) * (b - t + 1);
#ifdef GFX95_TEST
	for (int ty = t / TILE; ty <= b / TILE; ty++) for (int tx = l / TILE; tx <= r / TILE; tx++) dtile[ty * TW + tx] = 1;
#endif
}
#ifdef GFX95_TEST
/* returns number of changed tiles with no reported damage (violations); tiles = changed tiles, marked = tiles reported */
int gfx_damage_check(const uint8_t *vram, long *changed, long *marked)
{
	int pitch = ((gfx.w * 2) + 127) & ~127, viol = 0; long ch = 0, mk = 0;
	if (!shadow) { shadow = calloc(1, (size_t)pitch * gfx.h); }
	for (int ty = 0; ty < (gfx.h + TILE - 1) / TILE; ty++) for (int tx = 0; tx < (gfx.w + TILE - 1) / TILE; tx++) {
		int diff = 0;
		for (int y = ty * TILE; y < ty * TILE + TILE && y < gfx.h && !diff; y++) {
			int w = (tx * TILE + TILE <= gfx.w ? TILE : gfx.w - tx * TILE) * 2;
			diff = memcmp(vram + (size_t)y * pitch + tx * TILE * 2, shadow + (size_t)y * pitch + tx * TILE * 2, w) != 0;
		}
		if (dtile[ty * TW + tx]) mk++;
		if (diff) { ch++; if (!dtile[ty * TW + tx]) viol++; }
	}
	memcpy(shadow, vram, (size_t)pitch * gfx.h); memset(dtile, 0, sizeof dtile);
	*changed = ch; *marked = mk; return viol;
}
#endif

/* in-place pointer overlay on the presented 565 frame (board: called from redraw() before the PPA present; the VBE refresh rewrites the
 * whole frame every refresh, so the previous pointer position never lingers) */
int gfx_active(void) { return gfx.ctrl & 1; }
/* active guest display size in pixels (GFX95 mode when the driver has enabled it): the range an absolute pointer / touch position maps onto */
void gfx_pointer_range(int *w, int *h) { *w = gfx.w; *h = gfx.h; }
void gfx_overlay_apply(uint16_t *fb, int stride_px)
{
	if (!gfx.cvis) return;
	int w = gfx.w, h = gfx.h;
	int hx = gfx.shape[0], hy = gfx.shape[1], cw = gfx.shape[2], ch = gfx.shape[3];
	const uint8_t *m = (const uint8_t *)&gfx.shape[4];
	int ox = (gfx.pred_valid ? gfx.px : gfx.cx) - hx, oy = (gfx.pred_valid ? gfx.py : gfx.cy) - hy;
	for (int y = 0; y < ch && y < 32; y++) for (int x = 0; x < cw && x < 32; x++) {
		int sx = ox + x, sy = oy + y; if (sx < 0 || sy < 0 || sx >= w || sy >= h) continue;
		int a = (m[y * 4 + (x >> 3)] >> (7 - (x & 7))) & 1, xr = (m[128 + y * 4 + (x >> 3)] >> (7 - (x & 7))) & 1;
		uint16_t *p = &fb[sy * stride_px + sx]; *p = (uint16_t)((*p & (a ? 0xffff : 0)) ^ (xr ? 0xffff : 0));
	}
}

/* B61 snapshot: the card registers, pointer position + shape. Host-side caches (prediction queue, dirty tiles, counters) are not saved:
 * on resume the queue is empty and every tile is dirty, so the presenter shows the whole surface (VRAM itself is in the RAM image). */
#include "snap.h"
void gfx_snap(Snap *sn)
{
	sn_tag(sn, 0x47465831);
	SN(sn, gfx.index); SN(sn, gfx.w); SN(sn, gfx.h); SN(sn, gfx.bpp); SN(sn, gfx.ctrl);
	int32_t v[5] = { gfx.gx, gfx.cx, gfx.cy, gfx.cvis, gfx.sp }; SNA(sn, v, sizeof v);
	SNA(sn, gfx.shape, sizeof gfx.shape); SN(sn, gfx.shape_hash); SN(sn, gfx.last_hash);
	int32_t d[6] = { gfx.dm[0], gfx.dm[1], gfx.dm[2], gfx.dm[3], gfx.dp, gfx.depth }; SNA(sn, d, sizeof d);
	int32_t swc = gfx_swcursor; SN(sn, swc);   /* B29b: driver chose DIBENG pointer drawing -> no native screen drawing */
	if (sn->rd && !sn->err) {
		gfx.gx = v[0]; gfx.cx = v[1]; gfx.cy = v[2]; gfx.cvis = v[3]; gfx.sp = (v[4] >= 0 && v[4] < GFX_SHAPE_WORDS) ? v[4] : 0;
		for (int i = 0; i < 4; i++) gfx.dm[i] = d[i];
		gfx.dp = d[4] & 3; gfx.depth = d[5];
		gfx.pq_n = 0; gfx.pq_h = 0; gfx.pred_valid = 0; gfx.px = gfx.cx; gfx.py = gfx.cy;
		for (int i = 0; i < DT_W * DT_H; i++) dt2[i] = 1;
		gfx_swcursor = swc; ddi_sp = 0;   /* DDI profile frames are host-only (snapshot = idle desktop, no call in flight) */
	}
}

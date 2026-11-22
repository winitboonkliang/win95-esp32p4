#ifndef GFXCARD_H
#define GFXCARD_H
#include <stdint.h>
#define GFX_SHAPE_WORDS (4 + 128 / 2 * 2)   /* hotx hoty w h + AND 32x4 bytes + XOR 32x4 bytes = 4 + 128 words */
#define GFX_NPROF 40
typedef struct {
	uint16_t index, w, h, bpp, ctrl;
	int gx, cx, cy, cvis, px, py, pred_valid;
	uint16_t shape[GFX_SHAPE_WORDS]; int sp; uint32_t shape_hash, last_hash;
	int dm[4], dp;
	volatile int depth;   /* GDI calls in flight: +1 per reported rect (0x30), -1 per EndAccess/ExtTextOut exit (0x31) */
	long mode_sets, moves, pred_moves, mismatch, ctl_writes, shapes, shape_changes, damage_rects, damage_px;
	int pqx[256], pqy[256], pq_h, pq_n; long dbgn[256]; long prof_t0[GFX_NPROF], prof_n[GFX_NPROF], prof_c[GFX_NPROF];
} GfxCard;
extern GfxCard gfx;

void gfx_init(int enable);
int gfx_enabled(void);
uint32_t gfx_io_read(void *pc, int addr, int size);
void gfx_io_write(void *pc, int addr, uint32_t v, int size);
int gfx_io_write_string(void *pc, int addr, uint8_t *buf, int size, int count);
void gfx_io_write32(void *pc, int addr, uint32_t v);
int gfx_damage_check(const uint8_t *vram, long *changed, long *marked);
int gfx_active(void);
void gfx_pointer_range(int *w, int *h);   /* guest size of the active GFX95 mode, for absolute pointer / touch mapping */
void gfx_overlay_apply(uint16_t *fb, int stride_px);
void gfx_pred_move(int dx, int dy);
void gfx_pred_abs(int px, int py);
/* LFB write tracking (emulator memory write path): every guest write to the linear frame buffer marks its 16x16 tile; the presenter takes + clears them */
void gfx_lfb_dirty(uint32_t off, int len);
int gfx_take_dirty(int16_t (*r)[4], int max);
long gfx_model_take(const uint8_t *vram);
long pm_stale_rows(const uint8_t *vram);
void gfx_compose(const uint8_t *vram, uint16_t *out, int overlay);
/* RED B29b: per-export DDI profile (driver stubs, card index 0x50/0x51) */
#define GFX_DDI_N 40
typedef struct { long calls, incl, excl, wr, native, native_wr, verified, mismatch; } GfxDdi;   /* index = export ordinal (101..104 -> 32..35, 500 -> 36, 700 -> 37) */
extern GfxDdi *gfx_ddi; extern int gfx_ddi_n; extern long gfx_lfb_wr;
#endif

#pragma once
// ============================================================
//  ppa_rotator.h — ESP32-P4 Pixel Processing Accelerator wrapper
//  Hardware 90° rotation for LVGL tile → physical LCD layout.
//
//  PPA is a dedicated DMA peripheral that can rotate / scale /
//  mirror RGB565 buffers without using the CPU at all.  Used in
//  my_disp_flush() to replace the per-pixel CPU rotation loop.
//
//  Performance: ~10-30 MB/s of rotated output, fully parallel
//  with whatever the CPU is doing.  For our 1280×40 tile this is
//  about 100 µs per tile vs ~5-10 ms with the CPU loop.
// ============================================================
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

bool ppa_rotator_init(void);

// Rotate src (in RGB565, in_w × in_h pixels) 90° CW into dst.
// dst must be (in_h × in_w) pixels, RGB565, properly aligned for DMA.
// Returns true on success.  Blocking — returns when rotation done.
bool ppa_rotate_90cw_rgb565(const void *src, size_t in_w, size_t in_h,
                            void *dst, size_t dst_size);

// Rotate src 90° CW and write the result DIRECTLY into a larger destination
// picture at offset (dst_off_x, dst_off_y).  Used to push LVGL tiles straight
// into the DPI panel framebuffer (1 DSI refresh later they're on screen) —
// no per-tile LCD DMA call required.
bool ppa_rotate_90cw_to_fb(const void *src, size_t in_w, size_t in_h,
                           void *fb, size_t fb_w, size_t fb_h,
                           size_t dst_off_x, size_t dst_off_y);

// Scale src (RGB565, src_stride pixels wide, src_w × src_h actual image area)
// by (scale_x, scale_y) AND rotate 90° CW in one PPA operation.  Writes the
// result into dst at sub-pixel position (dst_off_x, dst_off_y) within an
// output picture of size (dst_pic_w × dst_pic_h).
//
// Used by the camera streaming path: lets PPA do letterbox-scale + rotate +
// place-in-output-buffer in ~1 hardware shot, replacing the CPU column loop.
// scale ranges supported: 1/16 .. 16.
//
// NOTE: the caller is responsible for clearing letterbox bars in dst BEFORE
// calling this function (PPA only writes the rotated/scaled region, it does
// not touch surrounding pixels).
bool ppa_rotate_90cw_scaled_off(const void *src_pic,
                                size_t src_pic_w, size_t src_pic_h,
                                size_t src_off_x, size_t src_off_y,
                                size_t blk_w, size_t blk_h,
                                void *dst, size_t dst_pic_w, size_t dst_pic_h,
                                size_t dst_size,
                                float scale_x, float scale_y);

bool ppa_rotate_90cw_scaled(const void *src,
                            size_t src_w, size_t src_h, size_t src_stride,
                            void *dst, size_t dst_pic_w, size_t dst_pic_h,
                            size_t dst_size,
                            size_t dst_off_x, size_t dst_off_y,
                            float scale_x, float scale_y);

// Non-blocking variant: starts the PPA transfer and returns immediately.
// Only ONE transfer may be in flight; call ppa_scaled_wait_done() before
// touching src/dst or starting the next one.  (Doom_P4_HD uses this to
// LUT-convert frame N+1 on the CPU while the PPA is still scaling frame N.)
bool ppa_rotate_90cw_scaled_async(const void *src,
                                  size_t src_w, size_t src_h, size_t src_stride,
                                  void *dst, size_t dst_pic_w, size_t dst_pic_h,
                                  size_t dst_size,
                                  size_t dst_off_x, size_t dst_off_y,
                                  float scale_x, float scale_y);

// Block until the async transfer completes (true) or timeout (false).
// Safe to call when nothing is in flight — returns true immediately.
bool ppa_scaled_wait_done(uint32_t timeout_ms);

// Fill a rectangle inside an RGB565 destination buffer with a solid colour
// (no CPU work — pure hardware DMA fill).  The destination is treated as
// a picture of size (dst_pic_w × dst_pic_h); the fill rect is placed at
// (off_x, off_y) and is fill_w × fill_h pixels.
//
//   rgb565    — 16-bit colour to fill with
//   dst       — destination buffer (RGB565), 64-byte aligned recommended
//   dst_size  — total bytes in the destination buffer
//
// Returns true on success.
bool ppa_fill_rgb565(uint16_t rgb565,
                     void *dst, size_t dst_pic_w, size_t dst_pic_h,
                     size_t dst_size,
                     size_t off_x, size_t off_y,
                     size_t fill_w, size_t fill_h);

// DMA-copy an RGB565 picture from src to dst (same dimensions, no scale, no
// rotate).  Used to snapshot a fresh frame into the OK/NG frame buffer
// without stalling the CPU on a ~720 KB memcpy.
//
// Both src and dst must be PSRAM-resident (or otherwise cache-coherent
// with PPA DMA).  Returns true on success.
bool ppa_copy_rgb565(const void *src, void *dst,
                     size_t w, size_t h, size_t stride);

// Rotate 90° CW a SUB-RECTANGLE of src (located at (src_off_x, src_off_y),
// size blk_w × blk_h, inside a src_pic_w × src_pic_h picture) and write the
// rotated output into dst at sub-rect (dst_off_x, dst_off_y) inside a
// dst_pic_w × dst_pic_h picture.  After 90° CW the rotated dimensions
// are (blk_h × blk_w) so the output region occupies
//   dst[dst_off_x .. dst_off_x + blk_h - 1, dst_off_y .. dst_off_y + blk_w - 1].
//
// Used by my_disp_flush() in DIRECT render mode: LVGL hands us the dirty
// bounding rect inside its full-screen landscape buffer (color_map +
// area->x1/y1/x2/y2); we rotate just that rect into g_stage_fb at the
// matching portrait offset, leaving the rest of g_stage_fb untouched so
// it keeps the latest cumulative panel state.  A subsequent
// lcd_draw_bitmap of the FULL g_stage_fb pushes that state to the DPI
// panel so both num_fbs=2 back buffers converge to the same content
// within two refreshes (anti-flicker for partial-rect-update divergence).
//
// dst_size is the total bytes available in dst (for the PPA driver's
// internal bounds check).
//
// Special case for "contiguous output" callers: pass dst_pic_w = blk_h,
// dst_pic_h = blk_w, dst_off_x = 0, dst_off_y = 0 to get a tightly-
// packed rotated picture at the start of dst.
//
// RGB565 in/out.  Blocking — returns when rotation completes.  Returns
// true on success.
bool ppa_rotate_90cw_sub(const void *src,
                         size_t src_pic_w, size_t src_pic_h,
                         size_t src_off_x, size_t src_off_y,
                         size_t blk_w,     size_t blk_h,
                         void  *dst,
                         size_t dst_pic_w, size_t dst_pic_h,
                         size_t dst_size,
                         size_t dst_off_x, size_t dst_off_y);

#ifdef __cplusplus
}
#endif

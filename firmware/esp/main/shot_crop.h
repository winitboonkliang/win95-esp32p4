/* shot_crop.h - pure C helpers of the "shot" command, no IDF includes so a PC test (tools/remote/shot_crop_test.c) runs the SAME code.
 * The guest surface is the console framebuffer: LCD_WIDTH x LCD_HEIGHT RGB565 little-endian, linear, stride = LCD_WIDTH * 2. vga.c has already
 * rendered the ACTIVE mode into it (palette, planar, text, VBE all resolved) at the rectangle vga_out_x/y/w/h, exactly what the presenter scales
 * to the panel, so copying that rectangle IS the guest's screen at native size. */
#ifndef SHOT_CROP_H
#define SHOT_CROP_H
#include <stdint.h>
#include <string.h>
#include <stddef.h>

static inline int shot_rect_ok(int x, int y, int w, int h, int fw, int fh)
{
	return w > 0 && h > 0 && x >= 0 && y >= 0 && x + w <= fw && y + h <= fh;
}

/* copy the w x h rectangle at (x,y) of an RGB565 surface that is src_w pixels wide into a contiguous dst (w*h*2 bytes) */
static inline void shot_copy_rect565(uint8_t *dst, const uint8_t *src, int src_w, int x, int y, int w, int h)
{
	for (int r = 0; r < h; r++)
		memcpy(dst + (size_t)r * w * 2, src + ((size_t)(y + r) * src_w + x) * 2, (size_t)w * 2);
}
#endif

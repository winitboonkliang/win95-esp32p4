// ============================================================
//  ppa_rotator.cpp
// ============================================================
#include "ppa_rotator.h"
#include <stdio.h>
#include "driver/ppa.h"
#include "esp_cache.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static ppa_client_handle_t s_client      = NULL;   // SRM (scale/rotate/mirror)
static ppa_client_handle_t s_fill_client = NULL;   // FILL  (solid colour rect)

// Async-transfer bookkeeping (single transfer in flight).
static SemaphoreHandle_t s_srm_done_sem = NULL;
static volatile bool     s_srm_inflight = false;

static bool srm_done_cb(ppa_client_handle_t, ppa_event_data_t *, void *)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_srm_done_sem, &woken);
    return woken == pdTRUE;
}

// BLUE B29s: the SRM client has max_pending_trans_num = 1, so a second caller while a blocking transfer is in flight gets ESP_FAIL
// ("exceed maximum pending transactions").  Two tasks do call it: the legacy lcd_present (console / boot) and the b29 presenter (first
// presents after the GFX95 driver switches the mode).  One mutex serialises every blocking SRM transfer; b29_ppa_contend counts the waits.
static SemaphoreHandle_t s_srm_mtx = NULL;
extern "C" { int b29_ppa_lasterr; int b29_ppa_lastrect[4]; unsigned long b29_ppa_contend; }
static esp_err_t srm_do(ppa_srm_oper_config_t *cfg)
{
    if (s_srm_mtx && xSemaphoreTake(s_srm_mtx, 0) != pdTRUE) { b29_ppa_contend++; xSemaphoreTake(s_srm_mtx, portMAX_DELAY); }
    esp_err_t r = ppa_do_scale_rotate_mirror(s_client, cfg);
    if (s_srm_mtx) xSemaphoreGive(s_srm_mtx);
    if (r != ESP_OK) b29_ppa_lasterr = (int)r;
    return r;
}

bool ppa_rotator_init(void)
{
    if (s_client && s_fill_client) return true;
    if (!s_srm_mtx) s_srm_mtx = xSemaphoreCreateMutex();

    // SRM client — scale/rotate/mirror operations
    if (!s_client) {
        ppa_client_config_t cfg = {};
        cfg.oper_type = PPA_OPERATION_SRM;
        cfg.max_pending_trans_num = 1;
        cfg.data_burst_length = PPA_DATA_BURST_LENGTH_32;
        esp_err_t r = ppa_register_client(&cfg, &s_client);
        if (r != ESP_OK) {
            printf("[PPA] register SRM client FAILED 0x%x\n", r);
            s_client = NULL;
            return false;
        }
    }

    // FILL client — solid-colour rectangle fills, used by clear_* helpers
    if (!s_fill_client) {
        ppa_client_config_t cfg = {};
        cfg.oper_type = PPA_OPERATION_FILL;
        cfg.max_pending_trans_num = 1;
        cfg.data_burst_length = PPA_DATA_BURST_LENGTH_32;
        esp_err_t r = ppa_register_client(&cfg, &s_fill_client);
        if (r != ESP_OK) {
            printf("[PPA] register FILL client FAILED 0x%x (fill disabled)\n", r);
            s_fill_client = NULL;
            // SRM still works — don't fail init outright
        }
    }

    if (!s_srm_done_sem) {
        s_srm_done_sem = xSemaphoreCreateBinary();
        ppa_event_callbacks_t cbs = {};
        cbs.on_trans_done = srm_done_cb;
        ppa_client_register_event_callbacks(s_client, &cbs);
    }

    printf("[PPA] HW rotator + fill ready (RGB565)\n");
    return true;
}

bool ppa_rotate_90cw_scaled_async(const void *src,
                                  size_t src_w, size_t src_h, size_t src_stride,
                                  void *dst, size_t dst_pic_w, size_t dst_pic_h,
                                  size_t dst_size,
                                  size_t dst_off_x, size_t dst_off_y,
                                  float scale_x, float scale_y)
{
    if (!s_client || !src || !dst || s_srm_inflight) return false;

    ppa_srm_oper_config_t cfg = {};
    cfg.in.buffer          = src;
    cfg.in.pic_w           = (uint32_t)src_stride;
    cfg.in.pic_h           = (uint32_t)src_h;
    cfg.in.block_w         = (uint32_t)src_w;
    cfg.in.block_h         = (uint32_t)src_h;
    cfg.in.srm_cm          = PPA_SRM_COLOR_MODE_RGB565;

    cfg.out.buffer         = dst;
    cfg.out.buffer_size    = (uint32_t)dst_size;
    cfg.out.pic_w          = (uint32_t)dst_pic_w;
    cfg.out.pic_h          = (uint32_t)dst_pic_h;
    cfg.out.block_offset_x = (uint32_t)dst_off_x;
    cfg.out.block_offset_y = (uint32_t)dst_off_y;
    cfg.out.srm_cm         = PPA_SRM_COLOR_MODE_RGB565;

    cfg.rotation_angle     = PPA_SRM_ROTATION_ANGLE_270;   // = 90° CW
    cfg.scale_x            = scale_x;
    cfg.scale_y            = scale_y;
    cfg.alpha_update_mode  = PPA_ALPHA_NO_CHANGE;
    cfg.mode               = PPA_TRANS_MODE_NON_BLOCKING;

    s_srm_inflight = true;
    esp_err_t r = ppa_do_scale_rotate_mirror(s_client, &cfg);
    if (r != ESP_OK) {
        s_srm_inflight = false;
        return false;
    }
    return true;
}

bool ppa_scaled_wait_done(uint32_t timeout_ms)
{
    if (!s_srm_inflight) return true;
    if (xSemaphoreTake(s_srm_done_sem, pdMS_TO_TICKS(timeout_ms)) != pdTRUE)
        return false;
    s_srm_inflight = false;
    return true;
}

bool ppa_rotate_90cw_rgb565(const void *src, size_t in_w, size_t in_h,
                            void *dst, size_t dst_size)
{
    if (!s_client || !src || !dst) return false;

    // PPA "ANGLE_270" is 270° CCW = 90° CW (what we want for landscape on portrait LCD).
    ppa_srm_oper_config_t cfg = {};
    cfg.in.buffer        = src;
    cfg.in.pic_w         = (uint32_t)in_w;
    cfg.in.pic_h         = (uint32_t)in_h;
    cfg.in.block_w       = (uint32_t)in_w;
    cfg.in.block_h       = (uint32_t)in_h;
    cfg.in.block_offset_x = 0;
    cfg.in.block_offset_y = 0;
    cfg.in.srm_cm        = PPA_SRM_COLOR_MODE_RGB565;

    cfg.out.buffer       = dst;
    cfg.out.buffer_size  = (uint32_t)dst_size;
    cfg.out.pic_w        = (uint32_t)in_h;     // swapped after rotation
    cfg.out.pic_h        = (uint32_t)in_w;
    cfg.out.block_offset_x = 0;
    cfg.out.block_offset_y = 0;
    cfg.out.srm_cm       = PPA_SRM_COLOR_MODE_RGB565;

    cfg.rotation_angle   = PPA_SRM_ROTATION_ANGLE_270;   // = 90° CW
    cfg.scale_x          = 1.0f;
    cfg.scale_y          = 1.0f;
    cfg.mirror_x         = false;
    cfg.mirror_y         = false;
    cfg.rgb_swap         = false;
    cfg.byte_swap        = false;
    cfg.alpha_update_mode = PPA_ALPHA_NO_CHANGE;
    cfg.mode             = PPA_TRANS_MODE_BLOCKING;

    esp_err_t r = srm_do(&cfg);
    return r == ESP_OK;
}

// Variant with source-rect offsets INSIDE a larger picture — the base
// buffer stays cache-aligned and the PPA driver handles the sub-rect
// (pointer-offset sources upset the cache sync and speckle the colors).
bool ppa_rotate_90cw_scaled_off(const void *src_pic,
                                size_t src_pic_w, size_t src_pic_h,
                                size_t src_off_x, size_t src_off_y,
                                size_t blk_w, size_t blk_h,
                                void *dst, size_t dst_pic_w, size_t dst_pic_h,
                                size_t dst_size,
                                float scale_x, float scale_y)
{
    if (!s_client || !src_pic || !dst) return false;

    ppa_srm_oper_config_t cfg = {};
    cfg.in.buffer          = src_pic;
    cfg.in.pic_w           = (uint32_t)src_pic_w;
    cfg.in.pic_h           = (uint32_t)src_pic_h;
    cfg.in.block_w         = (uint32_t)blk_w;
    cfg.in.block_h         = (uint32_t)blk_h;
    cfg.in.block_offset_x  = (uint32_t)src_off_x;
    cfg.in.block_offset_y  = (uint32_t)src_off_y;
    cfg.in.srm_cm          = PPA_SRM_COLOR_MODE_RGB565;

    cfg.out.buffer         = dst;
    cfg.out.buffer_size    = (uint32_t)dst_size;
    cfg.out.pic_w          = (uint32_t)dst_pic_w;
    cfg.out.pic_h          = (uint32_t)dst_pic_h;
    cfg.out.srm_cm         = PPA_SRM_COLOR_MODE_RGB565;

    cfg.rotation_angle     = PPA_SRM_ROTATION_ANGLE_270;  // = 90° CW
    cfg.scale_x            = scale_x;
    cfg.scale_y            = scale_y;
    cfg.alpha_update_mode  = PPA_ALPHA_NO_CHANGE;
    cfg.mode               = PPA_TRANS_MODE_BLOCKING;

    return srm_do(&cfg) == ESP_OK;
}

bool ppa_rotate_90cw_scaled(const void *src,
                            size_t src_w, size_t src_h, size_t src_stride,
                            void *dst, size_t dst_pic_w, size_t dst_pic_h,
                            size_t dst_size,
                            size_t dst_off_x, size_t dst_off_y,
                            float scale_x, float scale_y)
{
    if (!s_client || !src || !dst) return false;
    if (src_w == 0 || src_h == 0 || dst_pic_w == 0 || dst_pic_h == 0) return false;

    ppa_srm_oper_config_t cfg = {};
    cfg.in.buffer          = src;
    cfg.in.pic_w           = (uint32_t)src_stride;   // line stride in PIXELS
    cfg.in.pic_h           = (uint32_t)src_h;
    cfg.in.block_w         = (uint32_t)src_w;
    cfg.in.block_h         = (uint32_t)src_h;
    cfg.in.block_offset_x  = 0;
    cfg.in.block_offset_y  = 0;
    cfg.in.srm_cm          = PPA_SRM_COLOR_MODE_RGB565;

    cfg.out.buffer         = dst;
    cfg.out.buffer_size    = (uint32_t)dst_size;
    cfg.out.pic_w          = (uint32_t)dst_pic_w;
    cfg.out.pic_h          = (uint32_t)dst_pic_h;
    cfg.out.block_offset_x = (uint32_t)dst_off_x;
    cfg.out.block_offset_y = (uint32_t)dst_off_y;
    cfg.out.srm_cm         = PPA_SRM_COLOR_MODE_RGB565;

    cfg.rotation_angle     = PPA_SRM_ROTATION_ANGLE_270;  // = 90° CW
    cfg.scale_x            = scale_x;
    cfg.scale_y            = scale_y;
    cfg.mirror_x           = false;
    cfg.mirror_y           = false;
    cfg.rgb_swap           = false;
    cfg.byte_swap          = false;
    cfg.alpha_update_mode  = PPA_ALPHA_NO_CHANGE;
    cfg.mode               = PPA_TRANS_MODE_BLOCKING;

    esp_err_t r = srm_do(&cfg);
    return r == ESP_OK;
}

bool ppa_rotate_90cw_to_fb(const void *src, size_t in_w, size_t in_h,
                           void *fb, size_t fb_w, size_t fb_h,
                           size_t dst_off_x, size_t dst_off_y)
{
    if (!s_client || !src || !fb) return false;

    ppa_srm_oper_config_t cfg = {};
    cfg.in.buffer          = src;
    cfg.in.pic_w           = (uint32_t)in_w;
    cfg.in.pic_h           = (uint32_t)in_h;
    cfg.in.block_w         = (uint32_t)in_w;
    cfg.in.block_h         = (uint32_t)in_h;
    cfg.in.block_offset_x  = 0;
    cfg.in.block_offset_y  = 0;
    cfg.in.srm_cm          = PPA_SRM_COLOR_MODE_RGB565;

    cfg.out.buffer         = fb;
    cfg.out.buffer_size    = (uint32_t)(fb_w * fb_h * 2);
    cfg.out.pic_w          = (uint32_t)fb_w;
    cfg.out.pic_h          = (uint32_t)fb_h;
    cfg.out.block_offset_x = (uint32_t)dst_off_x;
    cfg.out.block_offset_y = (uint32_t)dst_off_y;
    cfg.out.srm_cm         = PPA_SRM_COLOR_MODE_RGB565;

    cfg.rotation_angle     = PPA_SRM_ROTATION_ANGLE_270;   // 90° CW
    cfg.scale_x            = 1.0f;
    cfg.scale_y            = 1.0f;
    cfg.alpha_update_mode  = PPA_ALPHA_NO_CHANGE;
    cfg.mode               = PPA_TRANS_MODE_BLOCKING;

    esp_err_t r = srm_do(&cfg);
    return r == ESP_OK;
}

// ── Solid-colour rectangle fill (zero-CPU) ───────────────────────
bool ppa_fill_rgb565(uint16_t rgb565,
                     void *dst, size_t dst_pic_w, size_t dst_pic_h,
                     size_t dst_size,
                     size_t off_x, size_t off_y,
                     size_t fill_w, size_t fill_h)
{
    if (!s_fill_client || !dst) return false;
    if (fill_w == 0 || fill_h == 0) return true;        // nothing to do
    if (off_x + fill_w > dst_pic_w || off_y + fill_h > dst_pic_h) return false;

    // Expand RGB565 → 8-bit per channel (PPA fill colour is a struct, not u32).
    uint8_t r5 = (rgb565 >> 11) & 0x1F;
    uint8_t g6 = (rgb565 >> 5)  & 0x3F;
    uint8_t b5 = (rgb565)       & 0x1F;
    uint8_t r8 = (uint8_t)((r5 * 255 + 15) / 31);
    uint8_t g8 = (uint8_t)((g6 * 255 + 31) / 63);
    uint8_t b8 = (uint8_t)((b5 * 255 + 15) / 31);

    ppa_fill_oper_config_t cfg = {};
    cfg.out.buffer         = dst;
    cfg.out.buffer_size    = (uint32_t)dst_size;
    cfg.out.pic_w          = (uint32_t)dst_pic_w;
    cfg.out.pic_h          = (uint32_t)dst_pic_h;
    cfg.out.block_offset_x = (uint32_t)off_x;
    cfg.out.block_offset_y = (uint32_t)off_y;
    cfg.out.fill_cm        = PPA_FILL_COLOR_MODE_RGB565;

    cfg.fill_argb_color.a  = 0xFF;
    cfg.fill_argb_color.r  = r8;
    cfg.fill_argb_color.g  = g8;
    cfg.fill_argb_color.b  = b8;
    cfg.fill_block_w       = (uint32_t)fill_w;
    cfg.fill_block_h       = (uint32_t)fill_h;
    cfg.mode               = PPA_TRANS_MODE_BLOCKING;

    esp_err_t r = ppa_do_fill(s_fill_client, &cfg);
    return r == ESP_OK;
}

// ── Pure DMA copy (PPA SRM with no scale, no rotate, no mirror) ──
bool ppa_copy_rgb565(const void *src, void *dst,
                     size_t w, size_t h, size_t stride)
{
    if (!s_client || !src || !dst) return false;
    if (w == 0 || h == 0) return true;

    ppa_srm_oper_config_t cfg = {};
    cfg.in.buffer         = src;
    cfg.in.pic_w          = (uint32_t)stride;
    cfg.in.pic_h          = (uint32_t)h;
    cfg.in.block_w        = (uint32_t)w;
    cfg.in.block_h        = (uint32_t)h;
    cfg.in.srm_cm         = PPA_SRM_COLOR_MODE_RGB565;

    cfg.out.buffer        = dst;
    cfg.out.buffer_size   = (uint32_t)(stride * h * 2);
    cfg.out.pic_w         = (uint32_t)stride;
    cfg.out.pic_h         = (uint32_t)h;
    cfg.out.srm_cm        = PPA_SRM_COLOR_MODE_RGB565;

    cfg.rotation_angle    = PPA_SRM_ROTATION_ANGLE_0;
    cfg.scale_x           = 1.0f;
    cfg.scale_y           = 1.0f;
    cfg.alpha_update_mode = PPA_ALPHA_NO_CHANGE;
    cfg.mode              = PPA_TRANS_MODE_BLOCKING;

    esp_err_t r = srm_do(&cfg);
    return r == ESP_OK;
}

// ── Partial-input rotation: src sub-rect → dst sub-rect (rotated) ──
bool ppa_rotate_90cw_sub(const void *src,
                         size_t src_pic_w, size_t src_pic_h,
                         size_t src_off_x, size_t src_off_y,
                         size_t blk_w,     size_t blk_h,
                         void  *dst,
                         size_t dst_pic_w, size_t dst_pic_h,
                         size_t dst_size,
                         size_t dst_off_x, size_t dst_off_y)
{
    if (!s_client || !src || !dst) return false;
    if (blk_w == 0 || blk_h == 0) return true;
    if (src_off_x + blk_w > src_pic_w || src_off_y + blk_h > src_pic_h) return false;
    // After 90° CW: rotated width = blk_h, rotated height = blk_w.
    if (dst_off_x + blk_h > dst_pic_w || dst_off_y + blk_w > dst_pic_h) return false;
    if (dst_size < dst_pic_w * dst_pic_h * 2) return false;

    ppa_srm_oper_config_t cfg = {};
    cfg.in.buffer          = src;
    cfg.in.pic_w           = (uint32_t)src_pic_w;
    cfg.in.pic_h           = (uint32_t)src_pic_h;
    cfg.in.block_w         = (uint32_t)blk_w;
    cfg.in.block_h         = (uint32_t)blk_h;
    cfg.in.block_offset_x  = (uint32_t)src_off_x;
    cfg.in.block_offset_y  = (uint32_t)src_off_y;
    cfg.in.srm_cm          = PPA_SRM_COLOR_MODE_RGB565;

    cfg.out.buffer         = dst;
    cfg.out.buffer_size    = (uint32_t)dst_size;
    cfg.out.pic_w          = (uint32_t)dst_pic_w;
    cfg.out.pic_h          = (uint32_t)dst_pic_h;
    cfg.out.block_offset_x = (uint32_t)dst_off_x;
    cfg.out.block_offset_y = (uint32_t)dst_off_y;
    cfg.out.srm_cm         = PPA_SRM_COLOR_MODE_RGB565;

    cfg.rotation_angle     = PPA_SRM_ROTATION_ANGLE_270;  // = 90° CW
    cfg.scale_x            = 1.0f;
    cfg.scale_y            = 1.0f;
    cfg.mirror_x           = false;
    cfg.mirror_y           = false;
    cfg.rgb_swap           = false;
    cfg.byte_swap          = false;
    cfg.alpha_update_mode  = PPA_ALPHA_NO_CHANGE;
    cfg.mode               = PPA_TRANS_MODE_BLOCKING;

    esp_err_t r = srm_do(&cfg);
    return r == ESP_OK;
}

// BLUE B29a: rotate one SOURCE block [x,x+w) x [y,y+h) of a pic_w x pic_h surface 90 deg CW, 1:1, into its place in the panel picture
// (dst_w x dst_h).  Panel block = x_p = dst_w - (y+h) .. dst_w - y, y_p = x .. x+w.  x_p and h must be multiples of 4 (PPA rule).
extern "C" bool b29_ppa_block_rot(const void *src_pic, size_t pic_w, size_t pic_h, size_t x, size_t y, size_t w, size_t h,
                                  void *dst, size_t dst_w, size_t dst_h, size_t dst_size, float scale)
{
    if (!s_client || !src_pic || !dst) return false;
    ppa_srm_oper_config_t cfg = {};
    cfg.in.buffer          = src_pic;
    cfg.in.pic_w           = (uint32_t)pic_w;
    cfg.in.pic_h           = (uint32_t)pic_h;
    cfg.in.block_w         = (uint32_t)w;
    cfg.in.block_h         = (uint32_t)h;
    cfg.in.block_offset_x  = (uint32_t)x;
    cfg.in.block_offset_y  = (uint32_t)y;
    cfg.in.srm_cm          = PPA_SRM_COLOR_MODE_RGB565;
    cfg.out.buffer         = dst;
    cfg.out.buffer_size    = (uint32_t)dst_size;
    cfg.out.pic_w          = (uint32_t)dst_w;
    cfg.out.pic_h          = (uint32_t)dst_h;
    cfg.out.block_offset_x = (uint32_t)(dst_w - (size_t)((float)(y + h) * scale + 0.5f));
    cfg.out.block_offset_y = (uint32_t)((float)x * scale + 0.5f);
    cfg.out.srm_cm         = PPA_SRM_COLOR_MODE_RGB565;
    cfg.rotation_angle     = PPA_SRM_ROTATION_ANGLE_270;  // = 90 deg CW
    cfg.scale_x            = scale;
    cfg.scale_y            = scale;
    cfg.alpha_update_mode  = PPA_ALPHA_NO_CHANGE;
    cfg.mode               = PPA_TRANS_MODE_BLOCKING;
    return srm_do(&cfg) == ESP_OK;
}

/* B83: scale only (no rotation): a transposed (column-major) Doom frame is already portrait; measured against b29_ppa_block_rot */
extern "C" bool b83_ppa_scale(const void *src_pic, size_t pic_w, size_t pic_h, void *dst, size_t dst_w, size_t dst_h, size_t dst_size, float scale)
{
    if (!s_client || !src_pic || !dst) return false;
    ppa_srm_oper_config_t cfg = {};
    cfg.in.buffer          = src_pic;
    cfg.in.pic_w           = (uint32_t)pic_w;
    cfg.in.pic_h           = (uint32_t)pic_h;
    cfg.in.block_w         = (uint32_t)pic_w;
    cfg.in.block_h         = (uint32_t)pic_h;
    cfg.in.srm_cm          = PPA_SRM_COLOR_MODE_RGB565;
    cfg.out.buffer         = dst;
    cfg.out.buffer_size    = (uint32_t)dst_size;
    cfg.out.pic_w          = (uint32_t)dst_w;
    cfg.out.pic_h          = (uint32_t)dst_h;
    cfg.out.srm_cm         = PPA_SRM_COLOR_MODE_RGB565;
    cfg.rotation_angle     = PPA_SRM_ROTATION_ANGLE_0;
    cfg.scale_x            = scale;
    cfg.scale_y            = scale;
    cfg.alpha_update_mode  = PPA_ALPHA_NO_CHANGE;
    cfg.mode               = PPA_TRANS_MODE_BLOCKING;
    return srm_do(&cfg) == ESP_OK;
}

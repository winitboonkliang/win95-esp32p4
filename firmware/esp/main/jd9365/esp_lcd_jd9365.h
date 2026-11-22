/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "soc/soc_caps.h"

#if SOC_MIPI_DSI_SUPPORTED
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_mipi_dsi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LCD panel initialization commands.
 *
 */
typedef struct {
    int cmd;                /*<! The specific LCD command */
    const void *data;       /*<! Buffer that holds the command specific data */
    size_t data_bytes;      /*<! Size of `data` in memory, in bytes */
    unsigned int delay_ms;  /*<! Delay in milliseconds after this command */
} jd9365_lcd_init_cmd_t;

/**
 * @brief LCD panel vendor configuration.
 *
 * @note  This structure needs to be passed to the `vendor_config` field in `esp_lcd_panel_dev_config_t`.
 *
 */
typedef struct {
    const jd9365_lcd_init_cmd_t *init_cmds;         /*!< Pointer to initialization commands array. Set to NULL if using default commands.
                                                     *   The array should be declared as `static const` and positioned outside the function.
                                                     *   Please refer to `vendor_specific_init_default` in source file.
                                                     */
    uint16_t init_cmds_size;                        /*<! Number of commands in above array */
    struct {
        esp_lcd_dsi_bus_handle_t dsi_bus;               /*!< MIPI-DSI bus configuration */
        const esp_lcd_dpi_panel_config_t *dpi_config;   /*!< MIPI-DPI panel configuration */
        uint8_t  lane_num;                              /*!< Number of MIPI-DSI lanes */
    } mipi_config;
} jd9365_vendor_config_t;

/**
 * @brief Create LCD panel for model JD9365
 *
 * @note  Vendor specific initialization can be different between manufacturers, should consult the LCD supplier for initialization sequence code.
 *
 * @param[in]  io LCD panel IO handle
 * @param[in]  panel_dev_config General panel device configuration
 * @param[out] ret_panel Returned LCD panel handle
 * @return
 *      - ESP_ERR_INVALID_ARG   if parameter is invalid
 *      - ESP_OK                on success
 *      - Otherwise             on fail
 */
esp_err_t esp_lcd_new_panel_jd9365(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                   esp_lcd_panel_handle_t *ret_panel);

/**
 * @brief MIPI-DSI bus configuration structure
 *
 */
#define JD9365_PANEL_BUS_DSI_2CH_CONFIG()                \
    {                                                    \
        .bus_id = 0,                                     \
        .num_data_lanes = 2,                             \
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,     \
        .lane_bit_rate_mbps = 1500,                      \
    }

/**
 * @brief MIPI-DBI panel IO configuration structure
 *
 */
#define JD9365_PANEL_IO_DBI_CONFIG()  \
    {                                 \
        .virtual_channel = 0,         \
        .lcd_cmd_bits = 8,            \
        .lcd_param_bits = 8,          \
    }

/**
 * @brief MIPI DPI configuration structure
 *
 * @note  refresh_rate = (dpi_clock_freq_mhz * 1000000) / (h_res + hsync_pulse_width + hsync_back_porch + hsync_front_porch)
 *                                                      / (v_res + vsync_pulse_width + vsync_back_porch + vsync_front_porch)
 *
 * @param[in] px_format Pixel format of the panel
 *
 */
// === Anti-flicker tuning notes ============================================
//
// The visible "วาบ" flicker has three independent root causes; each
// was tracked + fixed in sequence over multiple iterations:
//
//   1. Refresh rate below human flicker-fusion threshold.
//      h_total = 800 + 20 + 20 + 40 = 880 px
//      v_total = 1280 + 4 + 8 + 20  = 1312 lines
//      Iteration 1 (60 MHz):  ~52 Hz — visibly flickering steady.
//      Iteration 2 (72 MHz):  ~62 Hz — better, but operators still
//                              saw "random" flashes, especially in
//                              peripheral vision against the white UI.
//      Iteration 3 (80 MHz):  ~70 Hz — pushed comfortably above the
//                              upper end of the typical flicker-fusion
//                              threshold (60–70 Hz under bright UI).
//          refresh = 80,000,000 / (880 * 1312) = 69.3 Hz
//      80 MHz is still well within DSI PHY bandwidth (uses ~29% of
//      the 2-lane × 1500 Mbps = 3000 Mbps available) and only bumps
//      PSRAM continuous-read bandwidth from 49→54 MB/s (still leaves
//      ample headroom for CPU + other DMA on the shared PSRAM bus).
//      Iteration 4 (72 MHz, re-tested under DIRECT mode + always-push):
//                              still saw flicker — confirms clock/timing
//                              margin is NOT the residual root cause.
//                              Reverted to 80 MHz for higher refresh.
//
//   2. Tearing during back-to-back CPU writes into a single framebuffer.
//      With num_fbs = 1 the panel reads the SAME FB that the CPU/PPA
//      writes into, so partial updates can land mid-scanout and show
//      as a one-frame torn strip.  num_fbs = 2 lets the driver hold a
//      back buffer and present a clean frame on the next VSYNC.
//      Costs ~2 MB extra PSRAM for the second 800×1280×16-bit buffer.
//
//   3. Mid-DMA buffer reuse in the flush callback (see my_disp_flush
//      in the .ino — wait_dma_done() guard added at the start of each
//      flush so the previous LCD DMA is finished reading g_stage_fb
//      before PPA rotates the next dirty rect into it.  In DIRECT
//      render mode there is exactly one flush per refresh, so this
//      is the only synchronization needed; no multi-tile bookkeeping.
//
// All three together = stable image.  Keep this config.
// ===========================================================================
// === disable_lp was tried — REVERTED ======================================
//
// `disable_lp = true` pins the DPI PHY in HS mode (no LP/HS transitions).
// Sounds attractive for flicker, but JD9365 needs LP signalling to receive
// its init command stream (the long vendor reg table sent via DBI before
// DPI streaming starts) — with LP disabled the panel never initializes
// and the screen comes up black ("flash ผ่านแต่หน้าจอไม่ขึ้น").
//
// Keeping notes here so we don't re-try this exact flag.  If we want to
// avoid LP/HS races, the right place to clamp HS-only is AFTER init, not
// at panel-config time — but the IDF v5.3 snapshot here doesn't expose
// a runtime API to toggle .disable_lp after the fact, so we leave it off.
// ===========================================================================
#define JD9365_800_1280_PANEL_60HZ_DPI_CONFIG(px_format) \
    {							\
    	.virtual_channel = 0,                            \
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,     \
        .dpi_clock_freq_mhz = 80,                        \
        .pixel_format = px_format,                       \
        .num_fbs = 2,                                    \
        .video_timing = {                                \
            .h_size = 800,                               \
            .v_size = 1280,                              \
            .hsync_pulse_width = 20,   \
            .hsync_back_porch = 20,                  \
            .hsync_front_porch = 40,                     \
            .vsync_pulse_width = 4,  				\
            .vsync_back_porch = 8,                       \
            .vsync_front_porch = 20,                     \
        },                                               \
        .flags = {                                       \
            .use_dma2d = true,                           \
        },                                               \
    }
#endif

#ifdef __cplusplus
}
#endif

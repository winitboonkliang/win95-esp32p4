// lcd_jd9365_p4.c — JC8012P4A1C 10.1" MIPI-DSI display for tiny386.
//
// The panel is physically portrait 800x1280; tiny386's console (and the
// Windows desktop, via Bochs VBE) is logical landscape 1280x800.  The
// ESP32-P4's PPA does the 90° rotation in hardware straight into the DPI
// frame buffer — the same zero-CPU present pipeline this board runs DOOM
// with (see github.com/winitboonkliang/doom-esp32p4-hd).
#ifdef USE_LCD_JD9365_P4

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_ldo_regulator.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "../../gfxcard.h"

#include "esp_lcd_jd9365.h"
#include "ppa_rotator.h"
#include "common.h"
#include "prof.h"

static const char *TAG = "jd9365_p4";

#ifdef BOARD_NO_WIFI
// wifi.c is excluded on this board; ne2000.c still links against its
// packet hook.  NULL = "no network backend", which ne2000 handles.
#include <stdatomic.h>
#include <stdint.h>
void (*_Atomic esp32_send_packet)(uint8_t *buf, int size) = NULL;
#endif

#define PIN_LCD_RST   27
#define PIN_LCD_BL    23
#define PHYS_W        800
#define PHYS_H        1280

static esp_lcd_panel_handle_t s_panel;
#include "driver/jpeg_encode.h"
#include "esp_cache.h"
#include "shot_crop.h"
static void *s_fb[3];   /* [2] only in the B82 Doom boot (triple buffering) */
volatile int lcd_shot_hold;   /* card service "shot": freeze presenting while the front buffer is encoded */
static const void *s_last_fb;
static jpeg_encoder_handle_t s_jenc;
static uint8_t *s_jbuf;
static int   s_back = 0;

void pc_vga_step(void *pc);

// Active VGA output rect inside the console fb (exported by vga.c) —
// text modes are centered, graphics top-left, VBE fills it entirely.
extern int vga_out_x, vga_out_y, vga_out_w, vga_out_h;

// Whole-frame present: the ACTIVE mode rect (e.g. 720x400 DOS text) is
// hardware-scaled by the PPA to fill the entire 800x1280 panel while
// rotating — like a CRT stretching whatever mode it is fed.  A native
// 1280x800 VBE mode (Windows desktop) passes through at 1:1.
/* Written here on core 0, read by the emulation loop on core 1 for its
 * speed report — volatile so the read is not hoisted out of that loop. */
volatile unsigned long lcd_present_count;
static int64_t interval_us = 100000;   /* shared with vga_task's gate */

void lcd_present(void *fb)
{
	static int64_t last_us = 0;
	static int last_w = 0, last_h = 0;
	int64_t now = esp_timer_get_time();
	if (!s_panel)
		return;
	s_last_fb = fb;
	if (lcd_shot_hold)
		return;
	if (now - last_us < interval_us)
		return;
	last_us = now;

	int x = vga_out_x, y = vga_out_y, w = vga_out_w, h = vga_out_h;
	if (w <= 0 || h <= 0 || x + w > LCD_WIDTH || y + h > LCD_HEIGHT) {
		x = 0; y = 0; w = LCD_WIDTH; h = LCD_HEIGHT;
	}

	// Mode change: clear both DPI buffers so quantized-scale edges don't
	// keep stale pixels.
	if (w != last_w || h != last_h) {
		ppa_fill_rgb565(0x0000, s_fb[0], PHYS_W, PHYS_H,
				PHYS_W * PHYS_H * 2, 0, 0, PHYS_W, PHYS_H);
		ppa_fill_rgb565(0x0000, s_fb[1], PHYS_W, PHYS_H,
				PHYS_W * PHYS_H * 2, 0, 0, PHYS_W, PHYS_H);
		last_w = w;
		last_h = h;

		// Present rate is a bandwidth decision, not a taste one.  The
		// fixed cost is the whole-panel PPA write (2 MB) — the source
		// read is the part that scales with the mode, so 640x480 and
		// 1024x768 land within ~15% of each other and both can afford
		// 20 fps, which is where a moving mouse pointer stops looking
		// broken.  Only a native 1280x800 desktop doubles the total and
		// has to stay near 10 fps or it starves the emulator.
		interval_us = ((long)w * h <= 1024 * 768) ? 50000 : 100000;
	}

	float sx = (float)PHYS_H / (float)w;    /* fb X → panel vertical  */
	float sy = (float)PHYS_W / (float)h;    /* fb Y → panel horizontal */
	if (sx > 16.0f) sx = 16.0f;
	if (sy > 16.0f) sy = 16.0f;

	PROF_W_BEGIN(PW_PPA);
	bool ok = ppa_rotate_90cw_scaled_off(fb, LCD_WIDTH, LCD_HEIGHT,
					     x, y, w, h,
					     s_fb[s_back], PHYS_W, PHYS_H,
					     (size_t)PHYS_W * PHYS_H * 2,
					     sx, sy);
	if (ok) {
		esp_lcd_panel_draw_bitmap(s_panel, 0, 0, PHYS_W, PHYS_H,
					  s_fb[s_back]);
		s_back ^= 1;
		lcd_present_count++;
	}
	PROF_W_END(PW_PPA);
}

/* Screenshot for the card service (cardsvc.c, console command "shot"). A contiguous RGB565 copy (PSRAM, w*h*2 bytes) of
 *   panel=0  the guest screen: the ACTIVE VGA rectangle (vga_out_x/y/w/h) of the console surface, native size (640x480, 720x400, 1280x800 ...).
 *            vga.c has already rendered palette/planar/text/VBE modes into this RGB565 surface; it is what the presenter scales to the panel.
 *   panel=1  the panel's front DPI buffer, PHYS_W x PHYS_H RGB565 (800x1280, portrait; the PC rotates it back).
 * raw: the copy itself is sent (lossless, the PC converts: exact format known, nothing for a hardware block to misread).
 * jpg: the HW JPEG encoder (needs w and h multiples of 16) compresses the copy; faster over a slow link.
 * Opt-in: nothing runs until the command arrives; always-on cost = one pointer store + one flag test per present. */
int lcd_shot_live;   /* 1 = the last shot streams the live buffer (no private copy): cardsvc sends image CRC 0 */
static uint8_t *s_sbuf;
int b29_guest_surface(const uint8_t **p, int *w, int *h);
int lcd_shot_get(int panel, int jpg, const uint8_t **out, size_t *len, int *w, int *h)
{
	if (!s_panel)
		return -1;
	lcd_shot_live = 0; lcd_shot_hold = 1;
	vTaskDelay(pdMS_TO_TICKS(20));          /* let a present that is already running finish */
	const uint8_t *src; int sw, W, H, X = 0, Y = 0, rc = -2;
	if (panel) {
		src = (const uint8_t *)s_fb[s_back ^ 1]; sw = W = PHYS_W; H = PHYS_H;
		esp_cache_msync((void *)src, (size_t)W * H * 2, ESP_CACHE_MSYNC_FLAG_DIR_M2C | ESP_CACHE_MSYNC_FLAG_UNALIGNED);   /* PPA/DMA wrote it */
	} else if (b29_guest_surface(&src, &W, &H)) {   /* GFX95 linear mode: the guest surface itself (the VGA console fb is blank then) */
		sw = W;
	} else if (s_last_fb) {
		src = (const uint8_t *)s_last_fb; sw = LCD_WIDTH; W = LCD_WIDTH; H = LCD_HEIGHT;
		if (shot_rect_ok(vga_out_x, vga_out_y, vga_out_w, vga_out_h, LCD_WIDTH, LCD_HEIGHT)) { X = vga_out_x; Y = vga_out_y; W = vga_out_w; H = vga_out_h; }
	} else goto done;
	size_t need = (size_t)W * H * 2;
	s_sbuf = heap_caps_aligned_alloc(64, (need + 63) & ~(size_t)63, MALLOC_CAP_SPIRAM);
	const uint8_t *data;
	if (s_sbuf) { shot_copy_rect565(s_sbuf, src, sw, X, Y, W, H); data = s_sbuf; }
	else if (X == 0 && Y == 0 && W == sw) { data = src; lcd_shot_live = 1;   /* B29s: no 2 MB block (idle PSRAM free is ~2.8 MB, fragmented by the guest): stream the live buffer; the image CRC is then not sent (0), the frames keep their own CRCs */
		fprintf(stderr, "[shot] no private copy: largest PSRAM block %u < %u, live stream (image CRC 0)\r\n", (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM), (unsigned)need); }
	else goto done;
	lcd_shot_hold = 0;                      /* a private copy lets the panel run on while it is sent */
	if (!jpg) { *out = data; *len = need; *w = W; *h = H; rc = 0; goto done; }
	if ((W & 15) || (H & 15)) { rc = -3; goto done; }
	if (data != s_sbuf) { rc = -4; goto done; }   /* the live surface base is not 128 B aligned: the HW encoder's DMA read would be offset/garbled (seen on GFX95); only the 64 B-aligned private copy is encoded - use raw */
	if (!s_jenc) {
		jpeg_encode_engine_cfg_t ec = { .timeout_ms = 200 };
		if (jpeg_new_encoder_engine(&ec, &s_jenc) != ESP_OK) { s_jenc = NULL; goto done; }
	}
	size_t cap = 0;
	jpeg_encode_memory_alloc_cfg_t mc = { .buffer_direction = JPEG_ENC_ALLOC_OUTPUT_BUFFER };
	s_jbuf = jpeg_alloc_encoder_mem(512 * 1024, &mc, &cap);
	if (!s_jbuf) goto done;
	jpeg_encode_cfg_t cfg = { .height = H, .width = W, .src_type = JPEG_ENCODE_IN_FORMAT_RGB565,
				  .sub_sample = JPEG_DOWN_SAMPLING_YUV420, .image_quality = 90 };
	uint32_t n = 0;
	esp_cache_msync((void *)data, (need + 63) & ~(size_t)63, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);   /* the CPU just wrote the copy: push it to PSRAM before the DMA reads it */
	if (jpeg_encoder_process(s_jenc, &cfg, data, (uint32_t)need, s_jbuf, cap, &n) != ESP_OK) goto done;
	*out = s_jbuf; *len = n; *w = W; *h = H; rc = 0;
done:
	lcd_shot_hold = 0;
	return rc;
}
void lcd_shot_release(void)
{
	if (s_sbuf) { free(s_sbuf); s_sbuf = NULL; }
	if (s_jbuf) { free(s_jbuf); s_jbuf = NULL; }
	if (s_jenc) { jpeg_del_encoder_engine(s_jenc); s_jenc = NULL; }
}

/* BLUE B29a: the linear presenter writes the back panel buffer itself and then flips */
/* esp_lcd_panel_draw_bitmap(own fb) only SELECTS the buffer, the DMA is re-armed to it at the end of the frame being scanned.  Until that
 * refresh-done event the OTHER buffer is still on screen: writing it = scanning a half-written buffer (alternating states, smear, ghost pointer).
 * So after a select we wait for the next refresh-done before the back buffer is written again (<= 1 present per refresh, ~26 Hz at 30 MHz). */
static SemaphoreHandle_t s_refresh_sem; static volatile int s_flip_pending; volatile unsigned long b29_refresh_events, b29_flip_timeouts;
volatile uint32_t lcd_refresh_n;   /* refresh-done count (B82 triple buffering) */
volatile int64_t lcd_refresh_t;    /* B85: its time (input->photon is stamped on this clock) */
static bool IRAM_ATTR b29_refresh_cb(esp_lcd_panel_handle_t p, esp_lcd_dpi_panel_event_data_t *e, void *ctx)
{
	BaseType_t hp = pdFALSE; b29_refresh_events++; lcd_refresh_n++; lcd_refresh_t = esp_timer_get_time();
	if (s_refresh_sem) { xSemaphoreGiveFromISR(s_refresh_sem, &hp); }
	return hp == pdTRUE;
}
void lcd_wait_flip(void)
{
	if (!s_flip_pending || !s_refresh_sem) { return; }
	if (xSemaphoreTake(s_refresh_sem, pdMS_TO_TICKS(70)) != pdTRUE) { b29_flip_timeouts++; }
	s_flip_pending = 0;
}
/* 1 = the last select has taken effect (or none pending); 0 = still waiting after ms.  Lets the presenter serve the pointer while it waits. */
int lcd_wait_flip_ms(int ms)
{
	if (!s_flip_pending || !s_refresh_sem) { return 1; }
	if (xSemaphoreTake(s_refresh_sem, pdMS_TO_TICKS(ms)) != pdTRUE) { return 0; }
	s_flip_pending = 0; return 1;
}
void lcd_flip_timeout(void) { b29_flip_timeouts++; s_flip_pending = 0; }
void *lcd_back_fb(void) { return s_fb[s_back]; }
void *lcd_fb(int i) { return s_fb[i & 1]; }   /* both DPI buffers: the pointer fast path writes both */
void lcd_swap_present(void)
{
	if (s_refresh_sem) { xSemaphoreTake(s_refresh_sem, 0); }   /* drop events from before this select */
	s_flip_pending = 1;
	esp_lcd_panel_draw_bitmap(s_panel, 0, 0, PHYS_W, PHYS_H, s_fb[s_back]);
	s_back ^= 1;
	lcd_present_count++;
}
void *lcd_front_fb(void) { return s_fb[s_back ^ 1]; }
/* B82 Doom boot: any of the 2 or 3 DPI buffers, and queue buffer i for the next refresh */
void *lcd_fbn(int i) { return s_fb[i]; }
/* B82: sleep until the refresh count is past n (3-buffer pacing); the refresh-done ISR gives s_refresh_sem every refresh */
void lcd_wait_refresh_after(uint32_t n) { while ((int32_t)(lcd_refresh_n - n) <= 0) { if (s_refresh_sem) xSemaphoreTake(s_refresh_sem, pdMS_TO_TICKS(3)); else vTaskDelay(1); } }
/* 1-line rect: the PPA filled s_fb[i] by DMA, nothing dirty in the cache. A full-frame draw_bitmap ran a 2 MB esp_cache_msync (one critical
 * section) on the present core, the core that also owns the DSI/DMA-restart interrupts in the Doom boot: suspect of the white flashes. */
int lcd_full_flip;   /* ablation: 1 = full-frame draw_bitmap (2 MB msync inside IDF) */
void lcd_show(int i) { if (s_refresh_sem) { xSemaphoreTake(s_refresh_sem, 0); } s_flip_pending = 1; esp_lcd_panel_draw_bitmap(s_panel, 0, 0, PHYS_W, lcd_full_flip ? PHYS_H : 1, s_fb[i]); lcd_present_count++; }
int lcd_panel_ready(void) { return s_panel != NULL; }

// Strip interface unused in BOARD_FULLFRAME_PRESENT mode, but keep the
// symbol so shared code links.
void lcd_draw(int x_start, int y_start, int x_end, int y_end, void *src)
{
	(void)x_start; (void)y_start; (void)x_end; (void)y_end; (void)src;
}

/* B82: the panel bring-up alone (the Doom boot uses it without the emulator); pclk_mhz 0 = the Win95 default */
static int s_bl_ledc;   /* backlight runs on LEDC PWM (bl.txt < 100) */
int lcd_wait_sd;   /* B82: set by the Windows boot, which starts the panel before the SD mount */
void lcd_hw_init(int pclk_mhz, int nfb)
{
	ESP_LOGI(TAG, "Init 10.1\" JD9365 MIPI-DSI display");

	// VDD_MIPI_DPHY = 2.5 V from internal LDO channel 3.
	esp_ldo_channel_handle_t ldo_mipi_phy = NULL;
	esp_ldo_channel_config_t ldo_cfg = {
		.chan_id = 3,
		.voltage_mv = 2500,
	};
	ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_cfg, &ldo_mipi_phy));

	gpio_config_t bk = {
		.pin_bit_mask = 1ULL << PIN_LCD_BL,
		.mode = GPIO_MODE_OUTPUT,
	};
	ESP_ERROR_CHECK(gpio_config(&bk));
	gpio_set_level(PIN_LCD_BL, 0);
	gpio_hold_dis(PIN_LCD_BL);   /* release the hold set by lcd_bl_off before the restart (no-op after power-on) */

	esp_lcd_dsi_bus_handle_t bus;
	esp_lcd_dsi_bus_config_t bus_config = JD9365_PANEL_BUS_DSI_2CH_CONFIG();
	ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &bus));

	esp_lcd_panel_io_handle_t io;
	esp_lcd_dbi_io_config_t dbi_config = JD9365_PANEL_IO_DBI_CONFIG();
	ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(bus, &dbi_config, &io));

	esp_lcd_dpi_panel_config_t dpi_config =
		JD9365_800_1280_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
	dpi_config.num_fbs = nfb == 3 ? 3 : 2;
	// The emulator + PPA + panel all share the PSRAM bus; at 60 MHz pclk
	// the DPI FIFO underruns (bright white flashes).  40 MHz (~35 Hz
	// refresh, same as the Wireless-Tag P4 board) keeps the panel fed.
#ifndef LCD_PCLK_MHZ
#define LCD_PCLK_MHZ 40   /* 30 MHz (26 Hz refresh) gave vertical stripes + broken glyphs on the JC8012P4A1C glass; 40/50/60 MHz are clean (B29a 15 Nov, webcam A/B, no DSI underrun) */
#endif
	dpi_config.dpi_clock_freq_mhz = pclk_mhz ? pclk_mhz : LCD_PCLK_MHZ;
	{	/* maintenance override: /sdcard/pclk.txt holds one integer (MHz, 20..80); htotal 880 x vtotal 1312, refresh = pclk / 1.1546 M */
		FILE *pf = pclk_mhz ? NULL : fopen("/sdcard/pclk.txt", "r");
		int v = 0;
		if (pf) { if (fscanf(pf, "%d", &v) == 1 && v >= 20 && v <= 80) dpi_config.dpi_clock_freq_mhz = v; fclose(pf); }
		fprintf(stderr, "[lcd] DPI pclk %d MHz = %.1f Hz refresh%s\r\n", (int)dpi_config.dpi_clock_freq_mhz, dpi_config.dpi_clock_freq_mhz * 1e6 / (880.0 * 1312.0), v ? " (from /sdcard/pclk.txt)" : "");
	}

	jd9365_vendor_config_t vendor_config = {
		.mipi_config = {
			.dsi_bus = bus,
			.dpi_config = &dpi_config,
		},
	};
	esp_lcd_panel_dev_config_t panel_config = {
		.reset_gpio_num = PIN_LCD_RST,
		.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
		.bits_per_pixel = 16,
		.vendor_config = &vendor_config,
	};
	esp_lcd_panel_handle_t panel;
	ESP_ERROR_CHECK(esp_lcd_new_panel_jd9365(io, &panel_config, &panel));
	ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
	ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
	if (lcd_wait_sd) xEventGroupWaitBits(global_event_group, BIT2, pdFALSE, pdFALSE, portMAX_DELAY);   /* B82: early init, SD overrides below */
#ifndef BOARD_ROTATE_180
#define BOARD_ROTATE_180 1  /* owner 14 Nov: image was upside down on this unit */
#endif
	/* 180 deg = panel GS+SS mirror bits (MADCTL), done by the panel itself: zero CPU/PPA/PSRAM cost */
	{	/* maintenance override: /sdcard/rot180.txt = 1 (default) or 0 (GS+SS mirror on); display-path tests only */
		int rot = BOARD_ROTATE_180;
		FILE *rf = fopen("/sdcard/rot180.txt", "r");
		if (rf) { int v = -1; if (fscanf(rf, "%d", &v) == 1 && (v == 0 || v == 1)) { rot = v; } fclose(rf); }
		ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, !rot, !rot));
	}

	ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(panel, dpi_config.num_fbs,
							   &s_fb[0], &s_fb[1], &s_fb[2]));
	ppa_rotator_init();
	ppa_fill_rgb565(0x0000, s_fb[0], PHYS_W, PHYS_H,
			PHYS_W * PHYS_H * 2, 0, 0, PHYS_W, PHYS_H);
	ppa_fill_rgb565(0x0000, s_fb[1], PHYS_W, PHYS_H,
			PHYS_W * PHYS_H * 2, 0, 0, PHYS_W, PHYS_H);
	if (s_fb[2] && dpi_config.num_fbs == 3) ppa_fill_rgb565(0x0000, s_fb[2], PHYS_W, PHYS_H, PHYS_W * PHYS_H * 2, 0, 0, PHYS_W, PHYS_H);
	s_refresh_sem = xSemaphoreCreateBinary();
	{
		esp_lcd_dpi_panel_event_callbacks_t cbs = { .on_refresh_done = b29_refresh_cb };
		ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(panel, &cbs, NULL));
	}
	s_panel = panel;

	{	/* backlight: /sdcard/bl.txt = percent 1..100 -> 5 kHz LEDC PWM (100 or no file = DC) */
		FILE *bf = fopen("/sdcard/bl.txt", "r");
		int pct = 100;
		if (bf) { int v = 0; if (fscanf(bf, "%d", &v) == 1 && v >= 1 && v <= 100) { pct = v; } fclose(bf); }
		if (pct >= 100) {
			gpio_set_level(PIN_LCD_BL, 1);
		} else {
			ledc_timer_config_t t = { .speed_mode = LEDC_LOW_SPEED_MODE, .duty_resolution = LEDC_TIMER_10_BIT, .timer_num = LEDC_TIMER_1, .freq_hz = 5000, .clk_cfg = LEDC_AUTO_CLK };
			ledc_channel_config_t c = { .gpio_num = PIN_LCD_BL, .speed_mode = LEDC_LOW_SPEED_MODE, .channel = LEDC_CHANNEL_1, .timer_sel = LEDC_TIMER_1, .duty = (uint32_t)(1023 * pct / 100), .hpoint = 0 };
			ledc_timer_config(&t); ledc_channel_config(&c); s_bl_ledc = 1;
		}
	}

	globals.panel = panel;
	globals.panel_fb = NULL;   /* logical fb is rotated: no direct use */
}

/* backlight off before esp_restart (Windows<->Doom switch): the scan-out stops at reset while the light stays on = the white flash the cam saw at the switch */
void lcd_bl_off(void) { if (s_bl_ledc) ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0); gpio_set_direction(PIN_LCD_BL, GPIO_MODE_OUTPUT); gpio_set_level(PIN_LCD_BL, 0); gpio_hold_en(PIN_LCD_BL); }   /* pad hold keeps it dark through ROM + bootloader + early boot until lcd_hw_init owns the pin */

void vga_task(void *arg)
{
	(void)arg;
	lcd_hw_init(0, 2);

	/* Signal i386_task that the LCD is ready */
	xEventGroupSetBits(global_event_group, BIT1);
	xEventGroupWaitBits(global_event_group, BIT0, pdFALSE, pdFALSE,
			    portMAX_DELAY);

	/* The panel is presented at ~10 fps (see lcd_present).  vga_refresh()
	 * converts the WHOLE 1280x800 frame — ~4 MB of PSRAM traffic — and the
	 * VGA raster would fire it ~33x/s, so two out of every three
	 * conversions used to be computed and thrown away.  Open the gate only
	 * when a present is actually due; keep stepping the raster always. */
	extern int pc_vga_refresh_gate;
	int64_t next_frame_us = 0;
	while (1) {
		{ extern volatile int snap_hold, snap_parked; if (snap_hold) { snap_parked = 1; vTaskDelay(1); continue; } snap_parked = 0; }   /* B61: parked while a snapshot is applied */
		int64_t now = esp_timer_get_time();
		pc_vga_refresh_gate = (now >= next_frame_us);
		if (gfx_active()) { pc_vga_refresh_gate = 0; }   /* GFX95 linear mode: the VBE->fb conversion (4 MB of PSRAM traffic per refresh) is pure waste */

		unsigned long before = lcd_present_count;
		pc_vga_step(globals.pc);

		/* Start the next frame's clock from a frame that actually
		 * reached the panel.  A refresh only happens on the VGA
		 * raster's own retrace tick, which will not line up with the
		 * 5 ms poll below — closing the gate on the poll that opened
		 * it would drop most frames on the floor. */
		if (pc_vga_refresh_gate && lcd_present_count != before)
			next_frame_us = now + interval_us;
		vTaskDelay(pdMS_TO_TICKS(5));
	}
}

#endif /* USE_LCD_JD9365_P4 */

/* BLUE B45: GSL3680 touch controller on the shared I2C bus 0 (SDA 7 / SCL 8, same bus as ES8311 + RX8025T) -> pointer device.
 * Init sequence and report format follow the Doom_P4 driver that is proven on this board (oracle), ported to the i2c_master
 * driver the base already uses for the codec and the RTC (the legacy driver/i2c.h cannot share a bus with it).
 * Reports: register 0x80, 24 bytes: [0] = finger count, finger j at 4(j+1): y lo, y hi, x lo, x hi(4 bit) | id << 4.
 * The vendor ID/filter algorithm (gsl_point_id.c, copied unchanged) runs on every report, as in Doom.
 * Chip coordinates -> engine raw: (x, 1280 - y), the Doom mirror_y; the engine applies landscape + BOARD_ROTATE_180. */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_timer.h"
#include "gsl_point_id.h"
#include "gsl_fw.h"

#define GSL_ADDR 0x40
#define PIN_RST 22
#define PIN_INT 21
void b45_host_touch(uint32_t t_us, int down, int rx, int ry);
void b45_host_tick(uint32_t t_us);
int b85_touch(int n, const int *lx, const int *ly, const int *id);
extern volatile int b45_replay_active;

static i2c_master_dev_handle_t dev;
static TaskHandle_t gtask;
volatile unsigned long gsl_reports, gsl_errs, gsl_ints;
volatile int gsl_ok;

static esp_err_t wr(uint8_t reg, const uint8_t *d, int n)
{
	uint8_t b[8]; b[0] = reg; memcpy(b + 1, d, n);
	return i2c_master_transmit(dev, b, n + 1, 50);
}
static esp_err_t rd(uint8_t reg, uint8_t *d, int n) { return i2c_master_transmit_receive(dev, &reg, 1, d, n, 50); }
static void ms(int n) { vTaskDelay(pdMS_TO_TICKS(n)); }
static void w1(uint8_t reg, uint8_t v) { wr(reg, &v, 1); }

static void chip_reset(void)
{
	gpio_set_level(PIN_RST, 0); ms(20); gpio_set_level(PIN_RST, 1); ms(20);
	w1(0xe0, 0x88); ms(10); w1(0xe4, 0x04); ms(10);
	uint8_t z[4] = {0}; wr(0xbc, z, 4); ms(10);
}
static void startup(void) { w1(0xe0, 0x00); ms(10); gsl_DataInit(gsl_config_data_id); }

static void IRAM_ATTR int_isr(void *a) { gsl_ints++; BaseType_t w = pdFALSE; if (gtask) vTaskNotifyGiveFromISR(gtask, &w); portYIELD_FROM_ISR(w); }

static int chip_init(void)
{
	i2c_master_bus_handle_t bus = NULL;
	if (i2c_master_get_bus_handle(0, &bus) != ESP_OK || !bus) { puts("[gsl] no I2C bus 0"); return 0; }
	i2c_device_config_t dc = { .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = GSL_ADDR, .scl_speed_hz = 400000 };
	if (i2c_master_bus_add_device(bus, &dc, &dev) != ESP_OK) return 0;
	gpio_config_t o = { .mode = GPIO_MODE_OUTPUT, .pin_bit_mask = 1ULL << PIN_RST };
	gpio_config(&o);
	gpio_config_t io = { .mode = GPIO_MODE_OUTPUT, .pull_up_en = 1, .pin_bit_mask = 1ULL << PIN_INT };
	gpio_config(&io);
	gpio_set_level(PIN_RST, 0); gpio_set_level(PIN_INT, 0); ms(10); ms(1);
	gpio_set_level(PIN_RST, 1); ms(60);
	uint8_t b[4] = {0};
	if (rd(0xf0, b, 4) != ESP_OK) { puts("[gsl] GSL3680 not answering at 0x40, touch off"); return 0; }
	/* clear_reg (soft ICE halt), reset, firmware into RAM, start, reset again, start (Doom_P4 order) */
	w1(0xe0, 0x88); ms(20); w1(0x88, 0x01); ms(5); w1(0xe4, 0x04); ms(5); w1(0xe0, 0x00); ms(20);
	chip_reset();
	int64_t t0 = esp_timer_get_time(); unsigned nerr = 0;
	for (unsigned i = 0; i < sizeof GSLX680_FW / sizeof GSLX680_FW[0]; i++) {
		uint32_t v = GSLX680_FW[i].val; uint8_t d[4] = { v, v >> 8, v >> 16, v >> 24 };
		if (wr(GSLX680_FW[i].offset, d, GSLX680_FW[i].offset == 0xf0 ? 1 : 4) != ESP_OK) nerr++;
	}
	printf("[gsl] firmware %u words loaded in %lld ms, %u NACK\n", (unsigned)(sizeof GSLX680_FW / sizeof GSLX680_FW[0]), (esp_timer_get_time() - t0) / 1000, nerr);
	startup(); chip_reset(); startup();
	gpio_config_t ii = { .mode = GPIO_MODE_INPUT, .intr_type = GPIO_INTR_NEGEDGE, .pin_bit_mask = 1ULL << PIN_INT };
	gpio_config(&ii);
	gpio_install_isr_service(0);
	gpio_isr_handler_add(PIN_INT, int_isr, NULL);
	return 1;
}

static void gsl_task(void *a)
{
	(void)a;
	gtask = xTaskGetCurrentTaskHandle();
	gsl_ok = chip_init();   /* on core 1: the I2C ISR lands on the init core (Doom_P4 lesson: reports only reliable from core 1) */
	int down = 0; uint32_t busy_until = 0;
	for (;;) {
		/* up to 1 s after any report: 4 ms ticks drive long press / held button / 30 Hz pacing; idle: sleep until INT
		 * (50 ms poll only while no INT edge has ever been seen) */
		uint32_t t = (uint32_t)esp_timer_get_time();
		ulTaskNotifyTake(pdTRUE, (int32_t)(busy_until - t) > 0 ? pdMS_TO_TICKS(4) : pdMS_TO_TICKS(gsl_ints ? 1000 : 50));
		uint32_t now = (uint32_t)esp_timer_get_time();
		if (b45_replay_active) continue;
		if (!gsl_ok) { b45_host_tick(now); continue; }
		uint8_t d[24];
		if (rd(0x80, d, 24) != ESP_OK) { gsl_errs++; b45_host_tick(now); continue; }
		struct gsl_touch_info ci; memset(&ci, 0, sizeof ci);
		ci.finger_num = d[0] & 0x0f;
		for (int j = 0; j < ci.finger_num && j < 5; j++) {
			ci.x[j] = ((d[(j + 1) * 4 + 3] & 0x0f) << 8) | d[(j + 1) * 4 + 2];
			ci.y[j] = (d[(j + 1) * 4 + 1] << 8) | d[(j + 1) * 4 + 0];
			ci.id[j] = d[(j + 1) * 4 + 3] >> 4;
		}
		gsl_alg_id_main(&ci);
		unsigned tp = gsl_mask_tiaoping();
		if (tp > 0 && tp < 0xffffffffu) { uint8_t p[4] = { 0x0a, 0, 0, 0 }; wr(0xf0, p, 4); uint8_t q[4] = { tp, tp >> 8, tp >> 16, tp >> 24 }; wr(0x08, q, 4); }
		{	/* Doom boot: all fingers (move + fire at once) in landscape screen pixels, same transform as b45ptr map(); B85 also passes the finger ids */
			int lx[5], ly[5], id[5], k = 0;
			for (int j = 0; j < ci.finger_num && j < 5; j++) { int px = ci.x[j], py = 1280 - ci.y[j]; lx[k] = 1279 - py; ly[k] = px; id[k] = ci.id[j]; k++;   /* + 180 deg, as the Win95 touch engine (b45ptr rotate180 = 1, panel mounted upside down) */ }
			if (b85_touch(k, lx, ly, id)) { busy_until = now + 1000000; continue; }
		}
		int n = ci.finger_num > 0;
		if (n) {
			int x = ci.x[0], y = 1280 - ci.y[0];
			if (x < 0) { x = 0; } if (x > 799) { x = 799; } if (y < 0) { y = 0; } if (y > 1279) { y = 1279; }
			gsl_reports++; b45_host_touch(now, 1, x, y); down = 1; busy_until = now + 1000000;
		} else if (down) { gsl_reports++; b45_host_touch(now, 0, 0, 0); down = 0; busy_until = now + 1000000; }
		else b45_host_tick(now);
	}
}

void gsl_start(void) { xTaskCreatePinnedToCore(gsl_task, "gsl", 4096, NULL, 4, NULL, 1); }

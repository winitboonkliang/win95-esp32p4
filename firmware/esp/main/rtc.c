/* RX8025T RTC on the shared I2C bus 0 (SDA 7 / SCL 8, same bus as ES8311 + touch). Register logic is from our proven
 * INSPECTOR/screen/.../display/src/rx8025t.cpp: Flag at 0x0E (bit1 VLF = time lost, bit0 VDET = cell low), NOT the
 * RX-8025SA/NB layout. The chip holds local time (ICT-7); the guest CMOS reads host time(), so setting the host clock
 * at boot is all the guest needs. The bus is created by i2s.c (ES8311 init); we only add a device on it. */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "driver/i2c_master.h"
#include "board_jc8012p4a1c.h"

#ifdef USE_ES8311
#define RX_ADDR 0x32
#define RX_FLAG 0x0E
#define RX_VLF  0x02
#define RX_VDET 0x01
#define MIN_EPOCH 1672531200 /* 2023-01-01: anything older = never set */

static i2c_master_dev_handle_t s_dev;
static int s_written;

static int dev_ok(void)
{
	if (s_dev) return 1;
	i2c_master_bus_handle_t bus = NULL;
	if (i2c_master_get_bus_handle(ES8311_I2C_NUM, &bus) != ESP_OK || !bus) return 0;
	i2c_device_config_t dc = { .dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = RX_ADDR, .scl_speed_hz = 100000 };
	return i2c_master_bus_add_device(bus, &dc, &s_dev) == ESP_OK;
}
static esp_err_t rd(uint8_t reg, uint8_t *d, size_t n)
{
	if (!dev_ok()) return ESP_ERR_INVALID_STATE;
	return i2c_master_transmit_receive(s_dev, &reg, 1, d, n, 50);
}
static esp_err_t wr(uint8_t reg, const uint8_t *d, size_t n)
{
	uint8_t b[8];
	if (n > 7 || !dev_ok()) return ESP_ERR_INVALID_STATE;
	b[0] = reg; memcpy(b + 1, d, n);
	return i2c_master_transmit(s_dev, b, n + 1, 50);
}
static uint8_t b2d(uint8_t v) { return (v >> 4) * 10 + (v & 15); }
static uint8_t d2b(int v) { return ((v / 10) << 4) | (v % 10); }

/* Write local time to the chip and clear VLF/VDET (a VDET that returns later = cell really low). */
int rtc_write(const struct tm *t)
{
	if (t->tm_year < 123) { puts("[RTC] refusing year < 2023"); return 0; }
	uint8_t r[7] = { d2b(t->tm_sec), d2b(t->tm_min), d2b(t->tm_hour), 1 << (t->tm_wday & 7), d2b(t->tm_mday),
			 d2b(t->tm_mon + 1), d2b(t->tm_year - 100) };
	if (wr(0, r, 7) != ESP_OK) { puts("[RTC] write failed"); return 0; }
	uint8_t f = 0;
	rd(RX_FLAG, &f, 1); f &= ~(RX_VLF | RX_VDET); wr(RX_FLAG, &f, 1);
	s_written = 1;
	printf("[RTC] wrote %04d-%02d-%02d %02d:%02d:%02d\n", 1900 + t->tm_year, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
	return 1;
}

/* Boot: chip -> host clock. Returns 1 if the host clock was set; on VLF / no chip / garbage the clock is left alone. */
int rtc_boot(void)
{
	uint8_t f = 0, r[7];
	if (rd(RX_FLAG, &f, 1) != ESP_OK) { puts("[RTC] RX8025T not answering at 0x32, clock left alone"); return 0; }
	printf("[RTC] flag=0x%02X VLF=%d VDET=%d\n", f, !!(f & RX_VLF), !!(f & RX_VDET));
	if (f & RX_VLF) { puts("[RTC] VLF: time lost / never set, clock left alone (use: settime YYYY-MM-DD HH:MM:SS)"); return 0; }
	if (rd(0, r, 7) != ESP_OK) return 0;
	struct tm t = { .tm_sec = b2d(r[0] & 0x7F), .tm_min = b2d(r[1] & 0x7F), .tm_hour = b2d(r[2] & 0x3F), .tm_mday = b2d(r[4] & 0x3F),
			.tm_mon = b2d(r[5] & 0x1F) - 1, .tm_year = b2d(r[6]) + 100, .tm_isdst = -1 };
	time_t e = mktime(&t); /* TZ = ICT-7 is already set */
	if (e < MIN_EPOCH) { puts("[RTC] chip time out of range, ignored"); return 0; }
	struct timeval tv = { .tv_sec = e };
	settimeofday(&tv, NULL);
	printf("[RTC] host clock set from RTC: %04d-%02d-%02d %02d:%02d:%02d ICT\n", 1900 + t.tm_year, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	return 1;
}

/* B86: the chip has whole seconds only, so rtc_boot leaves the host clock 0..1 s behind. This short task (Windows boot only, ends
 * within ~1 s) waits for the seconds register to change and sets the host clock on that edge (+-5 ms). */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
extern volatile int b86_host_stepped;
static void refine_task(void *a)
{
	uint8_t s0 = 0xFF, r[7]; int i = 0, why = 2;
		for (; i < 400; i++) {
			vTaskDelay(pdMS_TO_TICKS(5));
			if (rd(0, r, 7) != ESP_OK) { why = 3; s0 = 0xFF; continue; }   /* shared bus (touch, codec): retry; an edge across a failed read is not timed */
			if (s0 == 0xFF) { s0 = r[0]; continue; }
			if (r[0] == s0) continue;
			struct tm t = { .tm_sec = b2d(r[0] & 0x7F), .tm_min = b2d(r[1] & 0x7F), .tm_hour = b2d(r[2] & 0x3F), .tm_mday = b2d(r[4] & 0x3F),
					.tm_mon = b2d(r[5] & 0x1F) - 1, .tm_year = b2d(r[6]) + 100, .tm_isdst = -1 };
			time_t e = mktime(&t); struct timeval was, tv = { .tv_sec = e };
			if (e < MIN_EPOCH) { why = 4; break; }
			gettimeofday(&was, NULL); settimeofday(&tv, NULL); b86_host_stepped = 1;
			esp_rom_printf("[RTC] refined on the seconds edge after %d polls: host clock moved %d ms\r\n", i, (int)((e - was.tv_sec) * 1000 - was.tv_usec / 1000));
			why = 0; break;
		}
	if (why) esp_rom_printf("[RTC] refine failed (reason %d, %d polls)\r\n", why, i);
	vTaskDelete(NULL);
}
void rtc_refine_start(void) { if (!s_dev || xTaskCreatePinnedToCore(refine_task, "rtc_refine", 2560, NULL, 2, NULL, 0) != pdPASS) esp_rom_printf("[RTC] refine not started\r\n"); }

/* "YYYY-MM-DD HH:MM:SS" (local ICT) -> host clock + chip. Later NTP can call this too. */
int rtc_settime(const char *s)
{
	int y, mo, d, h, mi, se;
	if (sscanf(s, "%d-%d-%d %d:%d:%d", &y, &mo, &d, &h, &mi, &se) != 6 || y < 2023 || y > 2099 || mo < 1 || mo > 12 ||
	    d < 1 || d > 31 || h > 23 || mi > 59 || se > 59) { puts("[RTC] usage: settime YYYY-MM-DD HH:MM:SS"); return 0; }
	struct tm t = { .tm_year = y - 1900, .tm_mon = mo - 1, .tm_mday = d, .tm_hour = h, .tm_min = mi, .tm_sec = se, .tm_isdst = -1 };
	time_t e = mktime(&t); /* normalises + fills tm_wday */
	struct timeval tv = { .tv_sec = e };
	settimeofday(&tv, NULL); b86_host_stepped = 1;
	return rtc_write(&t);
}
#else
int rtc_boot(void) { return 0; }
int rtc_settime(const char *s) { return 0; }
void rtc_refine_start(void) {}
int rtc_write(const void *t) { return 0; }
#endif

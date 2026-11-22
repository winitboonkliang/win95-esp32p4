#include "prof.h"
/* Card service: file push/pull + sector patch on the SD card over the USB-Serial/JTAG port,
 * while the emulator is NOT running. Built only with -DCARD_SERVICE (CMake default ON).
 *
 * Entry: normal mode runs a tiny listener task (polls the 64 B USB FIFO, no driver, no ring buffer).
 * The line "svc!" sets an RTC-noinit magic and restarts; the next boot skips the emulator and runs
 * service_run() forever (until `reboot`). Zero file access unless a command asks for it.
 *
 * Protocol idea and per-chunk ack flow control come from lab:BLUE/B31 (b31.py put). Changes vs B31:
 * CRC per frame (frame = u32 off, u32 n, data, u32 crc32(off|n|data)), host resends on 'N', atomic put
 * (tmp name, CRC + size check, rename), idle timeouts, path sandbox, bulk polling without a driver.
 *
 * Commands (one line, reply lines end with "OK ..." or "ERR ..."):
 *   hello | info | df | ls [dir] | stat P | rm P | mv A B | mkdir P | sha256 P | blkhash P BLK
 *   get P [off len]  -> OK len, frames from device (host acks each with A/N), final frame n=0
 *   put P size crc32hex -> READY, frames from host (device acks A/N/E), atomic rename at the end
 *   patch P -> READY, frames with arbitrary off written in place (never grows the file), final frame n=0
 *   reboot (normal boot)
 */
#ifdef CARD_SERVICE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_attr.h"
#include "esp_rom_crc.h"
#include "esp_vfs_fat.h"
#include "hal/usb_serial_jtag_ll.h"
#include "hal/uart_ll.h"
#include "driver/uart.h"
#include "mbedtls/sha256.h"

#define ROOT "/sdcard/"
#define CH 16384
static RTC_NOINIT_ATTR uint32_t svc_magic[2];
#define MAGIC 0x53564331u

/* Transport: USB-Serial/JTAG FIFO (polled, no driver) or UART0 (CH340 boards; IDF driver with a 32 KiB ring).
 * The first byte seen on either one selects it for replies. `baud N` changes the UART rate (host follows). */
static int g_uart;   /* 0 = USB-Serial/JTAG, 1 = UART0 */
static QueueHandle_t g_uq; static unsigned g_ovf, g_full, g_frm, g_par, g_bytes;
static volatile int g_yield;   /* set while c_shot streams: the polling loops below sleep a tick instead of spinning (see c_shot) */
volatile int card_quiet;   /* esp_main.c skips its periodic console lines while a shot streams */
static inline int u_drv(void) { return uart_is_driver_installed(UART_NUM_0); }   /* service mode / ESPDEBUG build: IDF driver; else raw FIFO */
static int u_get(uint8_t *d, size_t n) {
	if (!u_drv()) { uart_dev_t *hw = UART_LL_GET_HW(0); uint32_t k = uart_ll_get_rxfifo_len(hw); if (k > n) k = n; if (k) uart_ll_read_rxfifo(hw, d, k); return (int)k; }
	uart_event_t e; while (g_uq && xQueueReceive(g_uq, &e, 0)) { if (e.type == UART_FIFO_OVF) g_ovf++; else if (e.type == UART_BUFFER_FULL) g_full++; else if (e.type == UART_FRAME_ERR) g_frm++; else if (e.type == UART_PARITY_ERR) g_par++; }
	int k = uart_read_bytes(UART_NUM_0, d, n, 0); if (k > 0) g_bytes += k; return k;
}
static int rx_byte(int tmo_ms) {
	int64_t t0 = esp_timer_get_time(); uint8_t b = 0; unsigned spins = 0;
	for (;;) {
		if (usb_serial_jtag_ll_rxfifo_data_available()) { usb_serial_jtag_ll_read_rxfifo(&b, 1); g_uart = 0; return b; }
		if (u_get(&b, 1) == 1) { g_uart = 1; return b; }
		if (g_yield) vTaskDelay(1);
		if (tmo_ms >= 0 && !(++spins & 1023) && (esp_timer_get_time() - t0) / 1000 > tmo_ms) return -1;
	}
}
static int rx(uint8_t *d, size_t n, int tmo_ms) {
	int64_t t0 = esp_timer_get_time(); unsigned spins = 0;
	while (n) {
		int k = 0;
		if (g_uart) k = u_get(d, n > 4096 ? 4096 : n);
		else if (usb_serial_jtag_ll_rxfifo_data_available()) k = usb_serial_jtag_ll_read_rxfifo(d, n);
		if (k > 0) { d += k; n -= k; t0 = esp_timer_get_time(); spins = 0; }
		else { if (g_yield) vTaskDelay(1); if (!(++spins & 1023) && (esp_timer_get_time() - t0) / 1000 > tmo_ms) return -1; }
	}
	return 0;
}
static void tx(const void *p, size_t n) {
	if (g_uart && !u_drv()) {   /* normal mode without driver: poll the TX FIFO */
		uart_dev_t *hw = UART_LL_GET_HW(0); const uint8_t *b = p; int64_t t0 = esp_timer_get_time();
		while (n && esp_timer_get_time() - t0 < 2000000) { uint32_t f = uart_ll_get_txfifo_len(hw); if (!f) { if (g_yield) vTaskDelay(1); continue; } if (f > n) f = n; uart_ll_write_txfifo(hw, b, f); b += f; n -= f; t0 = esp_timer_get_time(); }
		while (!uart_ll_is_tx_idle(hw)) if (g_yield) vTaskDelay(1);
		return;
	}
	if (g_uart) { uart_write_bytes(UART_NUM_0, p, n); uart_wait_tx_done(UART_NUM_0, pdMS_TO_TICKS(2000)); return; }
	const uint8_t *b = p; int64_t t0 = esp_timer_get_time();
	while (n) {
		if (!usb_serial_jtag_ll_txfifo_writable()) { if (esp_timer_get_time() - t0 > 2000000) return; continue; }
		uint32_t k = usb_serial_jtag_ll_write_txfifo(b, n); b += k; n -= k; usb_serial_jtag_ll_txfifo_flush(); t0 = esp_timer_get_time();
	}
}
static void say(const char *f, ...) { char l[200]; va_list a; va_start(a, f); int n = vsnprintf(l, sizeof l, f, a); va_end(a); tx(l, n < (int)sizeof l ? n : (int)sizeof l - 1); }

static int path_ok(const char *p) { return p[0] && p[0] != '/' && !strstr(p, "..") && strlen(p) < 120; }
static int mkpath(char *out, const char *p) { if (!path_ok(p)) return -1; snprintf(out, 140, ROOT "%s", p); return 0; }

/* ---- frames: u32 off, u32 n, data[n], u32 crc32(off|n|data) ---- */
static uint8_t *g_buf;
static int frame_rx(uint32_t *off, uint32_t *n) {
	uint8_t h[8], t[4];
	if (rx(h, 8, 3000)) return -1;
	*off = h[0] | h[1] << 8 | h[2] << 16 | (uint32_t)h[3] << 24; *n = h[4] | h[5] << 8 | h[6] << 16 | (uint32_t)h[7] << 24;
	if (*n > CH) return -1;
	if (rx(g_buf, *n, 3000) || rx(t, 4, 3000)) return -1;
	uint32_t c = esp_rom_crc32_le(esp_rom_crc32_le(0, h, 8), g_buf, *n);
	return c == (t[0] | t[1] << 8 | t[2] << 16 | (uint32_t)t[3] << 24) ? 0 : 1;
}
static void frame_tx(uint32_t off, const uint8_t *d, uint32_t n) {
	uint8_t h[8] = { off, off >> 8, off >> 16, off >> 24, n, n >> 8, n >> 16, n >> 24 }, t[4];
	uint32_t c = esp_rom_crc32_le(esp_rom_crc32_le(0, h, 8), d, n);
	t[0] = c; t[1] = c >> 8; t[2] = c >> 16; t[3] = c >> 24;
	tx(h, 8); tx(d, n); tx(t, 4);
}

static void c_ls(const char *dir) {
	char p[140], q[300];
	if (dir[0]) { if (mkpath(p, dir)) { say("ERR path\n"); return; } } else strcpy(p, ROOT);
	DIR *d = opendir(p); if (!d) { say("ERR opendir\n"); return; }
	struct dirent *e; int n = 0;
	while ((e = readdir(d))) {
		struct stat st; snprintf(q, sizeof q, "%s/%s", p, e->d_name); if (stat(q, &st)) st.st_size = 0;
		say("%c %" PRIu32 " %s\n", e->d_type == DT_DIR ? 'D' : 'F', (uint32_t)st.st_size, e->d_name); n++;
	}
	closedir(d); say("OK %d\n", n);
}
static void c_get(const char *name, long off, long len) {
	char p[140]; struct stat st; if (mkpath(p, name) || stat(p, &st)) { say("ERR nofile\n"); return; }
	if (len < 0) len = st.st_size - off;
	if (off < 0 || len < 0 || off + len > st.st_size) { say("ERR range\n"); return; }
	int fd = open(p, O_RDONLY); if (fd < 0) { say("ERR open\n"); return; }
	say("OK %ld\n", len); lseek(fd, off, SEEK_SET);
	long pos = 0; int ok = 1;
	while (pos < len && ok) {
		uint32_t n = len - pos > CH ? CH : len - pos;
		if (read(fd, g_buf, n) != (ssize_t)n) { frame_tx(off + pos, g_buf, 0); say("ERR read\n"); close(fd); return; }
		for (int tries = 0;; tries++) {
			frame_tx(off + pos, g_buf, n); int a = rx_byte(5000);
			if (a == 'A') break;
			if (a != 'N' || tries > 5) { ok = 0; break; }
		}
		pos += n;
	}
	close(fd); if (ok) { frame_tx(off + len, g_buf, 0); say("OK done\n"); } else say("ERR abort\n");
}
/* put (tmp+rename) and patch (in place) share the frame receiver */
static void c_write(const char *name, int patch, long size, uint32_t want_crc) {
	char p[140], t[150]; struct stat st; if (mkpath(p, name)) { say("ERR path\n"); return; }
	int has = !stat(p, &st); const char *wp = p;
	if (patch) { if (!has) { say("ERR nofile\n"); return; } size = st.st_size; }
	else { snprintf(t, sizeof t, "%s.tmp~", p); wp = t; }
	int fd = open(wp, patch ? O_RDWR : (O_WRONLY | O_CREAT | O_TRUNC), 0666); if (fd < 0) { say("ERR open\n"); return; }
	say("READY\n");
	long pos = 0, total = 0; uint32_t crc = 0; int err = 0;
	for (;;) {
		uint32_t off, n; int r = frame_rx(&off, &n);
		if (r < 0) { err = 1; say("ERR timeout\n"); break; }
		if (r > 0) { tx("N", 1); continue; }
		if (n == 0) break;
		if (patch ? (long)off + (long)n > size : (long)off != pos) { tx("E", 1); err = 1; say("ERR offset\n"); break; }
		if (patch && lseek(fd, off, SEEK_SET) < 0) { tx("E", 1); err = 1; say("ERR seek\n"); break; }
		if (write(fd, g_buf, n) != (ssize_t)n) { tx("E", 1); err = 1; say("ERR write (card full?)\n"); break; }
		pos = off + n; total += n; crc = esp_rom_crc32_le(crc, g_buf, n); tx("A", 1);
	}
	if (!err && fsync(fd)) { err = 1; say("ERR sync\n"); }
	close(fd);
	if (err) { if (!patch) unlink(wp); return; }
	if (patch) { say("OK %ld\n", total); return; }
	if (pos != size || crc != want_crc) { unlink(wp); say("ERR verify size %ld crc %08" PRIx32 "\n", pos, crc); return; }
	if (has) unlink(p);
	if (rename(wp, p)) { say("ERR rename\n"); return; }
	say("OK %ld %08" PRIx32 "\n", pos, crc);
}
static void c_sha(const char *name) {
	char p[140]; if (mkpath(p, name)) { say("ERR path\n"); return; }
	int fd = open(p, O_RDONLY); if (fd < 0) { say("ERR open\n"); return; }
	mbedtls_sha256_context c; mbedtls_sha256_init(&c); mbedtls_sha256_starts(&c, 0);
	uint8_t h[32]; ssize_t n; long tot = 0; int64_t t0 = esp_timer_get_time();
	while ((n = read(fd, g_buf, CH)) > 0) { mbedtls_sha256_update(&c, g_buf, n); tot += n; }
	close(fd); mbedtls_sha256_finish(&c, h); mbedtls_sha256_free(&c);
	say("OK "); for (int i = 0; i < 32; i++) say("%02x", h[i]); say(" %ld %ld\n", tot, (long)((esp_timer_get_time() - t0) / 1000));
}
static void c_blkhash(const char *name, long blk) {
	char p[140]; struct stat st; if (mkpath(p, name) || stat(p, &st)) { say("ERR nofile\n"); return; }
	if (blk < 512 || blk > CH || (blk & (blk - 1))) { say("ERR blk\n"); return; }
	int fd = open(p, O_RDONLY); if (fd < 0) { say("ERR open\n"); return; }
	long nb = (st.st_size + blk - 1) / blk; say("OK %ld\n", nb);
	uint8_t o[256]; int k = 0; uint32_t all = 0;
	for (long i = 0; i < nb; i++) {
		ssize_t n = read(fd, g_buf, blk); if (n < 0) n = 0; if (n < blk) memset(g_buf + n, 0, blk - n);
		uint32_t c = esp_rom_crc32_le(0, g_buf, blk); memcpy(o + k, &c, 4); k += 4; all = esp_rom_crc32_le(all, (uint8_t *)&c, 4);
		if (k == sizeof o) { tx(o, k); k = 0; }
	}
	if (k) tx(o, k);
	close(fd); tx(&all, 4);
}

/* "shot guest|panel [baud] [jpg]" on the console of the RUNNING emulator (no service mode). Default = RAW RGB565 of the guest's active VGA
 * rectangle (or the panel's front buffer), lossless, the PC converts it to PNG/JPEG; "jpg" = HW-JPEG of the same surface. Streamed as ack'd CRC
 * frames like `get`. Header line "SHOT w h len crc32hex panel encode_ms kind" (kind 0 = raw565, 1 = jpeg). Over UART the rate is raised to
 * `baud` for the transfer and put back to 115200 afterwards; the periodic [perf] lines and logs are muted meanwhile. card.py shot = host side. */
extern int lcd_shot_get(int panel, int jpg, const uint8_t **out, size_t *len, int *w, int *h) __attribute__((weak));
extern void lcd_shot_release(void) __attribute__((weak));
extern int lcd_shot_live __attribute__((weak));
static void c_shot(int panel, long baud, int jpg) {
	const uint8_t *j = NULL; size_t len = 0; int w = 0, h = 0;
	if (!lcd_shot_get) { say("ERR no display in this build\n"); return; }
	int64_t t0 = esp_timer_get_time();
	int rc = lcd_shot_get(panel, jpg, &j, &len, &w, &h);
	if (rc) { say("ERR capture %d\n", rc); if (lcd_shot_release) lcd_shot_release(); return; }
	int ems = (int)((esp_timer_get_time() - t0) / 1000);
	uint32_t crc = (&lcd_shot_live && lcd_shot_live) ? 0 : esp_rom_crc32_le(0, j, len);   /* 0 = live stream: the host checks the frames and the length only */
	int sw = g_uart == 1 && baud > 0; int lvl = esp_log_level_get("*");
	/* B29s: cardsvc (core 0, prio 1) polled the UART in 23 s busy loops: vga_task (prio 0) and IDLE0 got no CPU for the whole transfer */
	g_yield = 1; card_quiet = 1; esp_log_level_set("*", ESP_LOG_NONE);
	/* B29s: card.py raises ITS baud 50 ms after it sent the command; a header sent before that is garbled (host: no header, then the board times out waiting for
	 * the ack: "ERR abort" 5 s later).  The copy/CRC used to take ~60 ms by luck; with no copy it is under 25 ms.  Wait before the baud change. */
	if (sw) { vTaskDelay(pdMS_TO_TICKS(150)); uart_set_baudrate(UART_NUM_0, baud); }
	say("SHOT %d %d %u %08x %d %d %d\n", w, h, (unsigned)len, (unsigned)crc, panel, ems, jpg);
	int ok = 1; size_t pos = 0;
	while (pos < len && ok) {
		uint32_t n = len - pos > CH ? CH : len - pos;
		for (int tries = 0;; tries++) {
			frame_tx(pos, j + pos, n); int a = rx_byte(5000);
			if (a == 'A') break;
			if (a != 'N' || tries > 5) { ok = 0; break; }
		}
		pos += n;
	}
	if (ok) { frame_tx(len, j, 0); say("OK shot\n"); } else say("ERR abort\n");
	if (sw) uart_set_baudrate(UART_NUM_0, 115200);
	esp_log_level_set("*", lvl); card_quiet = 0; g_yield = 0;
	if (lcd_shot_release) lcd_shot_release();
}

/* MEM team 16 Nov: runtime internal-RAM map (console "heapmap"): heap_caps summary + every internal region + stack high-water per known task */
static void c_heapmap(void) {
	static const char *const tn[] = { "main", "IDLE0", "IDLE1", "ipc0", "ipc1", "esp_timer", "Tmr Svc", "cardsvc", "b29_present", "i386_main", "vga_task", "i2s_task", "usb_task", "usb_boot", "scn", "prof0", "prof1", "profrep", "wifi", "uart_event", "usbh_hub", "hid_task", "sdmmc" };
	fprintf(stderr, "[heapmap] begin\r\n");
	fprintf(stderr, "[heapmap] internal free=%u min=%u largest=%u total=%u | dma free=%u | 8bit free=%u | spiram free=%u total=%u\r\n",
		(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
		(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_total_size(MALLOC_CAP_INTERNAL),
		(unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
		(unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM), (unsigned)heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
	fprintf(stderr, "[heapmap] spiram largest_block=%u (a shot needs one 2048000 B block)\r\n", (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
	heap_caps_print_heap_info(MALLOC_CAP_INTERNAL);
	for (unsigned i = 0; i < sizeof tn / sizeof tn[0]; i++) {
		TaskHandle_t h = xTaskGetHandle(tn[i]);
		if (h) fprintf(stderr, "[heapmap] task %-12s stack_hwm_free_bytes=%u\r\n", tn[i], (unsigned)uxTaskGetStackHighWaterMark(h));
	}
	fprintf(stderr, "[heapmap] end\r\n");
}
static void cmd(char *l) {
	char a[4][140] = {{0}}, p[140], q[140];
	int na = sscanf(l, "%139s %139s %139s %139s", a[0], a[1], a[2], a[3]); if (na < 1) { say("OK SVC1\n"); return; }
	const char *c = a[0];
	if (!strcmp(c, "hello")) say("OK SVC1 chunk=%d free=%u\n", CH, (unsigned)esp_get_free_heap_size());
	else if (!strcmp(c, "info")) say("OK free_internal=%u free_8bit=%u\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)esp_get_free_heap_size());
	else if (!strcmp(c, "df")) { uint64_t t = 0, f = 0; if (esp_vfs_fat_info("/sdcard", &t, &f)) say("ERR df\n"); else say("OK %" PRIu64 " %" PRIu64 "\n", t, f); }
	else if (!strcmp(c, "ls")) c_ls(na > 1 ? a[1] : "");
	else if (!strcmp(c, "stat") && na > 1) { struct stat st; if (mkpath(p, a[1]) || stat(p, &st)) say("ERR nofile\n"); else say("OK %" PRIu32 " %c %ld\n", (uint32_t)st.st_size, S_ISDIR(st.st_mode) ? 'D' : 'F', (long)st.st_mtime); }
	else if (!strcmp(c, "rm") && na > 1) { if (mkpath(p, a[1])) say("ERR path\n"); else if (unlink(p) && rmdir(p)) say("ERR rm\n"); else say("OK\n"); }
	else if (!strcmp(c, "mv") && na > 2) { if (mkpath(p, a[1]) || mkpath(q, a[2])) say("ERR path\n"); else if (rename(p, q)) say("ERR rename\n"); else say("OK\n"); }
	else if (!strcmp(c, "mkdir") && na > 1) { if (mkpath(p, a[1])) say("ERR path\n"); else if (mkdir(p, 0777)) say("ERR mkdir\n"); else say("OK\n"); }
	else if (!strcmp(c, "sha256") && na > 1) c_sha(a[1]);
	else if (!strcmp(c, "blkhash") && na > 2) c_blkhash(a[1], atol(a[2]));
	else if (!strcmp(c, "get") && na > 1) c_get(a[1], na > 3 ? atol(a[2]) : 0, na > 3 ? atol(a[3]) : -1);
	else if (!strcmp(c, "put") && na > 3) c_write(a[1], 0, atol(a[2]), strtoul(a[3], 0, 16));
	else if (!strcmp(c, "patch") && na > 1) c_write(a[1], 1, 0, 0);
	else if (!strcmp(c, "baud") && na > 1) { long bd = atol(a[1]); say("OK %ld\n", bd); if (g_uart) { uart_wait_tx_done(UART_NUM_0, pdMS_TO_TICKS(500)); uart_set_baudrate(UART_NUM_0, bd); } }
	else if (!strcmp(c, "dbg")) say("OK uart bytes=%u fifo_ovf=%u buf_full=%u frame_err=%u parity=%u\n", g_bytes, g_ovf, g_full, g_frm, g_par);
	else if (!strcmp(c, "reboot")) { say("OK rebooting\n"); vTaskDelay(pdMS_TO_TICKS(200)); esp_restart(); }
	else say("ERR unknown\n");
}

/* Never returns. Runs after the SD card is mounted, before the emulator is started. */
int rtc_settime(const char *s);
static TaskHandle_t g_listener;
static void service_run(void) {
	if (g_listener) vTaskDelete(g_listener); /* it would steal FIFO bytes */
	esp_log_level_set("*", ESP_LOG_NONE); esp_task_wdt_deinit();
	uart_driver_install(UART_NUM_0, 32768, 0, 64, &g_uq, 0); uart_set_rx_full_threshold(UART_NUM_0, 32); /* ESP_ERR_INVALID_STATE if present: fine */
	uart_set_baudrate(UART_NUM_0, 115200);
	g_buf = heap_caps_aligned_alloc(64, CH, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
	char l[200]; int n = 0;
	for (;;) {
		int b = rx_byte(-1); if (b == '\r') continue;
		if (b == '\n') { l[n] = 0; n = 0; cmd(l); } else if (n < (int)sizeof l - 1) l[n++] = b;
	}
}
void card_service_boot(void) {
	if (svc_magic[0] == MAGIC && svc_magic[1] == ~MAGIC) { svc_magic[0] = 0; service_run(); }
}
/* Normal mode: wait for the line "svc!" on the USB-Serial/JTAG FIFO or UART0 FIFO (no driver installed, 0 bytes of ring buffer). */
static void listener(void *arg) {
	char l[160]; int n = 0;   /* B87: was 40, cut b76keys/ptr lines over 39 chars into garbage (owner demo 22 Nov); stack 4096 */
	for (;;) {
		uint8_t b = 0;
		for (;;) {
			if (usb_serial_jtag_ll_rxfifo_data_available()) { usb_serial_jtag_ll_read_rxfifo(&b, 1); g_uart = 0; }
			else if (uart_ll_get_rxfifo_len(UART_LL_GET_HW(0))) { uart_ll_read_rxfifo(UART_LL_GET_HW(0), &b, 1); g_uart = 1; }
			else if (uart_is_driver_installed(UART_NUM_0) && uart_read_bytes(UART_NUM_0, &b, 1, 0) == 1) g_uart = 1; /* ESPDEBUG driver drains the FIFO into its ring */
			else if (uart_is_driver_installed(UART_NUM_0) && uart_read_bytes(UART_NUM_0, &b, 1, 0) == 1) ; /* ESPDEBUG driver drains the FIFO into its ring */
			else break;
			if (b == '\n') { l[n] = 0; n = 0; if (!strcmp(l, "svc!")) { svc_magic[0] = MAGIC; svc_magic[1] = ~MAGIC; esp_restart(); } else if (!strncmp(l, "touch ", 6)) { extern void b45_console_touch(const char *); b45_console_touch(l + 6); } else if (!strcmp(l, "touchstat")) { extern void b45_console_stat(void); b45_console_stat(); } else if (!strncmp(l, "settime ", 8)) rtc_settime(l + 8); else if (!strcmp(l, "wclock")) { extern void b86_wclock(uint32_t); extern uint32_t b86_bda(void); b86_wclock(b86_bda()); } else if (!strcmp(l, "prof")) prof_request(); else if (!strncmp(l, "shot", 4)) { char m[8] = "", k[8] = ""; long bd = 0; sscanf(l + 4, "%7s %ld %7s", m, &bd, k); c_shot(!strcmp(m, "panel"), bd, !strcmp(k, "jpg")); } else if (!strcmp(l, "text")) { extern void diag_text(void); diag_text(); } else if (!strncmp(l, "ptr ", 4)) { extern void inject_ptr(const char *); inject_ptr(l + 4); } else if (!strcmp(l, "p1")) { extern void inject_p1(void); inject_p1(); }
#ifdef B76
				else if (!strncmp(l, "doom", 4)) { extern void b82_console(const char *); b82_console(l); }
				else if (!strncmp(l, "b76keys ", 8)) { extern void b76_keys(const char *); b76_keys(l + 8); }
#endif
				 else if (!strcmp(l, "scn") || !strcmp(l, "b29drag") || !strcmp(l, "paint") || !strcmp(l, "demo") || !strcmp(l, "ddi") || !strncmp(l, "nat ", 4) || !strncmp(l, "scn ", 4) || !strcmp(l, "gfxbench") || !strncmp(l, "gk ", 3) || !strncmp(l, "ppa ", 4)) { extern void r29b_console(const char *); r29b_console(l); } else if (!strncmp(l, "b29d ", 5)) { extern void b29d_set(const char *); b29d_set(l + 5); } else if (!strcmp(l, "heapmap")) c_heapmap(); else if (!strcmp(l, "x86")) { extern volatile int x86_req; x86_req = 1; } }
			else if (b != '\r') { if (n < (int)sizeof l - 1) l[n++] = b; else n = 0; }
		}
		vTaskDelay(pdMS_TO_TICKS(50));
	}
}
void card_service_listener_start(void) { xTaskCreatePinnedToCore(listener, "cardsvc", 4096, NULL, 1, &g_listener, 0); }
#endif

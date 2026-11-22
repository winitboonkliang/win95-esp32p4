/* B61 decode bench (board only, triggered by /sdcard/b61.bench; data = a golden snapshot copied to /sdcard/b61bench.snap).
 * Reads the compressed chunks of a REAL golden into PSRAM, then for every decoder variant: decodes all of them (output walks a 4 MB PSRAM arena like the
 * guest RAM does), times it, compares each chunk's output CRC with the Rust oracle, and fuzzes mutated chunks against the oracle (return value +
 * bytes + guard bytes behind dst). Also times CRC-32 over the compressed bytes. Prints "[b61bench] ..." lines on the console. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_async_memcpy.h"
#include "esp_cache.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
static SemaphoreHandle_t b_sem;
static bool b_cb(async_memcpy_handle_t hh, async_memcpy_event_t *ev, void *u) { BaseType_t hp = pdFALSE; xSemaphoreGiveFromISR(b_sem, &hp); return hp == pdTRUE; }

extern uint32_t snap_crc32(uint32_t crc, const uint8_t *p, size_t n);
extern int snap_lz4_decompress(const uint8_t *src, size_t n, uint8_t *dst, size_t cap);
extern int snap_lz4_decompress_fast(const uint8_t *src, size_t n, uint8_t *dst, size_t cap);
extern int snap_lz4_dec_pie(const uint8_t *src, size_t n, uint8_t *dst, size_t cap);
extern uint32_t b61_crc_fast(uint32_t crc, const uint8_t *p, size_t n) __attribute__((weak));

#define PAGE 4096
#define MAXCH 600
typedef int (*decfn)(const uint8_t *, size_t, uint8_t *, size_t);
typedef uint32_t (*crcfn)(uint32_t, const uint8_t *, size_t);
static struct { uint32_t off, len, np; } ch[MAXCH];
static uint32_t nch, ocrc[MAXCH];

static int rd_full(int fd, void *b, size_t n) { size_t g = 0; while (g < n) { ssize_t k = read(fd, (uint8_t *) b + g, n - g); if (k <= 0) return -1; g += k; } return 0; }
static uint32_t rng_s = 12345;
static uint32_t rnd(void) { rng_s = rng_s * 1664525u + 1013904223u; return rng_s >> 8; }

void b61_bench(const char *path)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0) { fprintf(stderr, "[b61bench] no %s\r\n", path); return; }
	uint8_t hd[128];
	if (rd_full(fd, hd, 128)) { close(fd); return; }
	uint32_t dev_len = *(uint32_t *)(hd + 56), ram = *(uint32_t *)(hd + 48), vram = *(uint32_t *)(hd + 52);
	uint32_t npg = (ram + vram) / PAGE, rp = ram / PAGE;
	uint8_t *bm = malloc((npg + 7) / 8);
	lseek(fd, 128 + dev_len, SEEK_SET);
	if (!bm || rd_full(fd, bm, (npg + 7) / 8)) { close(fd); return; }
	size_t ARENA = 1536u << 10;
	uint8_t *arena = heap_caps_aligned_alloc(128, ARENA, MALLOC_CAP_SPIRAM);
	uint8_t *out = heap_caps_aligned_alloc(128, 2u << 20, MALLOC_CAP_SPIRAM);
	uint8_t *cb = heap_caps_aligned_alloc(128, 80000, MALLOC_CAP_SPIRAM);
	if (!arena || !out || !cb) { fprintf(stderr, "[b61bench] alloc failed\r\n"); close(fd); return; }
	uint32_t used = 0, idx = 0, tot_c = 0, tot_chunks = 0, stored = 0;
	for (uint32_t i = 0; i < npg; ) {
		if (!(bm[i >> 3] & (1 << (i & 7)))) { i++; continue; }
		uint32_t n = 1; while (n < 16 && i + n < npg && (bm[(i + n) >> 3] & (1 << ((i + n) & 7))) && i + n != rp) n++;
		uint32_t rec; if (rd_full(fd, &rec, 4)) break;
		uint32_t len = rec & 0x7fffffffu; if (len > 70000) break;
		if (rd_full(fd, cb, len)) break;
		tot_chunks++; tot_c += len;
		if (rec & 0x80000000u) stored++;
		else if (idx++ % 4 == 0 && nch < MAXCH && used + len + 16 <= ARENA) { memcpy(arena + used, cb, len); ch[nch].off = used; ch[nch].len = len; ch[nch].np = n; nch++; used += (len + 15) & ~15u; }
		i += n;
	}
	close(fd);
	uint32_t sum_c = 0, sum_r = 0;
	for (uint32_t k = 0; k < nch; k++) { sum_c += ch[k].len; sum_r += ch[k].np * PAGE; }
	fprintf(stderr, "[b61bench] file chunks=%lu stored=%lu comp=%lu | sampled chunks=%lu comp=%lu raw=%lu\r\n", (unsigned long) tot_chunks, (unsigned long) stored, (unsigned long) tot_c, (unsigned long) nch, (unsigned long) sum_c, (unsigned long) sum_r);

	/* oracle output CRCs */
	for (uint32_t k = 0; k < nch; k++) {
		uint8_t *d = out + (size_t)(k % 32) * 65536;
		int r = snap_lz4_decompress(arena + ch[k].off, ch[k].len, d, ch[k].np * PAGE);
		ocrc[k] = r == (int)(ch[k].np * PAGE) ? snap_crc32(0, d, r) : 0xdead0000u;
		if (r != (int)(ch[k].np * PAGE)) fprintf(stderr, "[b61bench] ORACLE FAILED on chunk %lu\r\n", (unsigned long) k);
	}
	static const struct { const char *n; decfn f; } V[] = { { "rust", snap_lz4_decompress }, { "asm", snap_lz4_decompress_fast }, { "pie", snap_lz4_dec_pie } };
	for (int v = 0; v < 3; v++) {
		int bad = 0; int64_t best = 1 << 30;
		for (int rep = 0; rep < 3; rep++) {
			int64_t t0 = esp_timer_get_time();
			for (uint32_t k = 0; k < nch; k++) V[v].f(arena + ch[k].off, ch[k].len, out + (size_t)(k % 32) * 65536, ch[k].np * PAGE);
			int64_t dt = esp_timer_get_time() - t0; if (dt < best) best = dt;
		}
		for (uint32_t k = 0; k < nch; k++) {
			uint8_t *d = out + (size_t)(k % 32) * 65536; memset(d, 0, ch[k].np * PAGE);
			int r = V[v].f(arena + ch[k].off, ch[k].len, d, ch[k].np * PAGE);
			if (r != (int)(ch[k].np * PAGE) || snap_crc32(0, d, r) != ocrc[k]) bad++;
		}
		fprintf(stderr, "[b61bench] dec %s: %lld us for raw=%lu comp=%lu -> %.1f MB/s out, %.2f ns/B comp | mismatches %d of %lu\r\n", V[v].n, (long long) best, (unsigned long) sum_r, (unsigned long) sum_c,
			(double) sum_r / best, 1000.0 * best / sum_c, bad, (unsigned long) nch);
	}
	/* memcpy floor: write the same raw bytes into the walking 4 MB arena */
	{ int64_t best = 1 << 30; for (int rep = 0; rep < 3; rep++) { int64_t t0 = esp_timer_get_time(); for (uint32_t k = 0; k < nch; k++) memset(out + (size_t)(k % 32) * 65536, 0x5a, ch[k].np * PAGE); int64_t dt = esp_timer_get_time() - t0; if (dt < best) best = dt; }
	  fprintf(stderr, "[b61bench] floor memset raw=%lu: %lld us -> %.1f MB/s\r\n", (unsigned long) sum_r, (long long) best, (double) sum_r / best); }
	{ int64_t best = 1 << 30; for (int rep = 0; rep < 3; rep++) { int64_t t0 = esp_timer_get_time(); for (uint32_t k = 0; k < nch; k++) memcpy(out + (size_t)(k % 32) * 65536, arena + ch[k].off, ch[k].len); int64_t dt = esp_timer_get_time() - t0; if (dt < best) best = dt; }
	  fprintf(stderr, "[b61bench] floor memcpy comp=%lu: %lld us -> %.1f MB/s\r\n", (unsigned long) sum_c, (long long) best, (double) sum_c / best); }
	/* AXI-GDMA mem2mem internal SRAM -> PSRAM vs CPU memcpy (is a DMA write-out of the decoded chunk worth it?) */
	{
		async_memcpy_handle_t h;
		uint8_t *isrc = heap_caps_aligned_alloc(128, 65536, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
		fprintf(stderr, "[b61bench] internal alloc 64K aligned: %s (largest internal block %lu, free internal %lu)\r\n", isrc ? "ok" : "FAILED", (unsigned long) heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL), (unsigned long) heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
		if (isrc) {
			memset(isrc, 0x3c, 65536);
			int64_t best = 1 << 30;
			for (int rep = 0; rep < 3; rep++) { int64_t t0 = esp_timer_get_time(); for (uint32_t k = 0; k < nch; k++) memcpy(out + (size_t)(k % 32) * 65536, isrc, ch[k].np * PAGE); int64_t dt = esp_timer_get_time() - t0; if (dt < best) best = dt; }
			fprintf(stderr, "[b61bench] cpu memcpy SRAM->PSRAM raw=%lu: %lld us -> %.1f MB/s\r\n", (unsigned long) sum_r, (long long) best, (double) sum_r / best);
			b_sem = xSemaphoreCreateCounting(16, 0);
			async_memcpy_config_t cfg = ASYNC_MEMCPY_DEFAULT_CONFIG(); cfg.backlog = 8;
			esp_err_t e = esp_async_memcpy_install_gdma_axi(&cfg, &h);
			fprintf(stderr, "[b61bench] install axi gdma memcpy: %d\r\n", (int) e);
			if (e == ESP_OK) {
				for (int mode = 0; mode < 2; mode++) {   /* 0 = one at a time, 1 = keep 4 in flight */
					int64_t bestd = 1 << 30; int errs = 0;
					for (int rep = 0; rep < 3; rep++) {
						int64_t t0 = esp_timer_get_time(); int infl = 0;
						for (uint32_t k = 0; k < nch; k++) {
							if (mode) { while (infl >= 4) { xSemaphoreTake(b_sem, portMAX_DELAY); infl--; } }
							if (esp_async_memcpy(h, out + (size_t)(k % 32) * 65536, isrc, ch[k].np * PAGE, b_cb, NULL) != ESP_OK) { errs++; continue; }
							infl++;
							if (!mode) { xSemaphoreTake(b_sem, portMAX_DELAY); infl--; }
						}
						while (infl > 0) { xSemaphoreTake(b_sem, portMAX_DELAY); infl--; }
						int64_t dt = esp_timer_get_time() - t0; if (dt < bestd) bestd = dt;
					}
					esp_cache_msync(out, 65536, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
					fprintf(stderr, "[b61bench] dma SRAM->PSRAM %s raw=%lu: %lld us -> %.1f MB/s errs=%d first byte %02x\r\n", mode ? "4 in flight" : "serial", (unsigned long) sum_r, (long long) bestd, (double) sum_r / bestd, errs, out[0]);
				}
			}
		}
	}
	/* CRC */
	{ static const struct { const char *n; crcfn f; } C[] = { { "rust-slice4", snap_crc32 }, { "asm", b61_crc_fast } };
	  for (int v = 0; v < 2; v++) { if (!C[v].f) continue;
		int64_t best = 1 << 30; uint32_t c0 = 0; for (int rep = 0; rep < 3; rep++) { int64_t t0 = esp_timer_get_time(); uint32_t c = 0; for (uint32_t k = 0; k < nch; k++) c = C[v].f(c, arena + ch[k].off, ch[k].len); int64_t dt = esp_timer_get_time() - t0; if (dt < best) best = dt; c0 = c; }
		uint32_t cr = 0; for (uint32_t k = 0; k < nch; k++) cr = snap_crc32(cr, arena + ch[k].off, ch[k].len);
		fprintf(stderr, "[b61bench] crc %s: %lld us for %lu B -> %.1f MB/s %s\r\n", C[v].n, (long long) best, (unsigned long) sum_c, (double) sum_c / best, c0 == cr ? "match" : "MISMATCH"); } }
	/* fuzz: mutated real chunks, asm/pie vs oracle (return value, bytes, guard bytes) */
	for (int v = 1; v < 3; v++) {
		uint32_t bad = 0, okc = 0, errc = 0; const uint32_t N = 6000;
		uint8_t *mut = cb; uint8_t *dA = out, *dB = out + 70000;
		for (uint32_t t = 0; t < N; t++) {
			uint32_t k = rnd() % nch; uint32_t len = ch[k].len; memcpy(mut, arena + ch[k].off, len);
			uint32_t m = rnd() % 8;
			if (m < 4) { uint32_t nm = 1 + rnd() % 3; while (nm--) mut[rnd() % len] = (uint8_t) rnd(); }
			else if (m == 4) len = 1 + rnd() % len;
			else if (m == 5) { mut[len - 1 - rnd() % 4 % len] ^= (uint8_t)(1 << (rnd() & 7)); }
			uint32_t cap = ch[k].np * PAGE; if ((rnd() & 7) == 0) cap -= 1 + rnd() % 64;
			memset(dA, 0xA5, cap + 64); memset(dB, 0xA5, cap + 64);
			int ra = snap_lz4_decompress(mut, len, dA, cap), rb = V[v].f(mut, len, dB, cap);
			int ok = ra == rb;
			if (ok && ra >= 0 && memcmp(dA, dB, ra)) ok = 0;
			for (int g = 0; g < 64 && ok; g++) if (dB[cap + g] != 0xA5) ok = 0;
			if (ra >= 0) okc++; else errc++;
			if (!ok) bad++;
		}
		fprintf(stderr, "[b61bench] fuzz %s: %lu trials (%lu decoded, %lu refused by oracle) mismatches %lu\r\n", V[v].n, (unsigned long) N, (unsigned long) okc, (unsigned long) errc, (unsigned long) bad);
	}
	fprintf(stderr, "[b61bench] done\r\n");
	free(bm); heap_caps_free(arena); heap_caps_free(out); heap_caps_free(cb);
}

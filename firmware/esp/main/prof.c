/* B50_PROF board profiler, see prof.h. */
#ifdef B50_PROF
#include <stdio.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "driver/gptimer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "prof.h"

enum { B_GUEST, B_DECODE, B_MEMTLB, B_DEVIO, B_DISK, B_PRESENT, B_USB, B_IRQ, B_IDLE, B_RTOS, B_OTHER, B_ROM, B_N };
static const char *const bname[B_N] = { "guest_exec", "decode", "mem_tlb", "device_io", "disk", "present_ppa", "usb_input", "irq", "idle", "rtos_libc", "other", "rom" };
typedef struct { uint32_t lo, hi; uint8_t b; } prof_range_t;
#include "prof_map.inc"      /* const prof_range_t prof_map[512]; const int prof_map_n;  generated, fixed size so addresses do not move */

#define SAMPLE_HZ 2000
#define GR 6                 /* 64-byte granules */
uint64_t prof_wait_cyc[2][PW_N];
uint32_t prof_wait_cnt[2][PW_N];
extern int _instruction_reserved_start, _instruction_reserved_end, _iram_start, _iram_end;
static uint32_t fl_lo, fl_n, ir_lo, ir_n;
static uint32_t *hist[2];
static volatile uint32_t nsamp[2], nrom[2], noth[2];
static volatile uint64_t isr_cyc[2];
static volatile int want_report;

static bool on_alarm(gptimer_handle_t t, const gptimer_alarm_event_data_t *e, void *u)
{
    uint32_t c0 = prof_cyc(), core = prof_core(), pc;
    __asm__ volatile("csrr %0, mepc" : "=r"(pc));      /* the interrupted instruction (IDF's handler has not nested) */
    uint32_t *h = hist[core];
    if (pc - fl_lo < (fl_n << GR)) h[(pc - fl_lo) >> GR]++;
    else if (pc - ir_lo < (ir_n << GR)) h[fl_n + ((pc - ir_lo) >> GR)]++;
    else if (pc - 0x4fc00000u < 0x80000u) nrom[core]++;
    else noth[core]++;
    nsamp[core]++;
    isr_cyc[core] += (uint32_t)(prof_cyc() - c0);
    return false;
}

static void core_task(void *arg)
{
    gptimer_handle_t t;
    gptimer_config_t cfg = { .clk_src = GPTIMER_CLK_SRC_DEFAULT, .direction = GPTIMER_COUNT_UP, .resolution_hz = 1000000 };
    gptimer_event_callbacks_t cb = { .on_alarm = on_alarm };
    gptimer_alarm_config_t al = { .alarm_count = 1000000 / SAMPLE_HZ, .reload_count = 0, .flags.auto_reload_on_alarm = true };
    if (gptimer_new_timer(&cfg, &t) == ESP_OK && gptimer_register_event_callbacks(t, &cb, NULL) == ESP_OK &&
        gptimer_enable(t) == ESP_OK && gptimer_set_alarm_action(t, &al) == ESP_OK && gptimer_start(t) == ESP_OK)
        printf("[prof] sampler started on core %d (%d Hz)\r\n", (int)prof_core(), SAMPLE_HZ);
    else
        printf("[prof] sampler FAILED on core %d\r\n", (int)prof_core());
    vTaskDelete(NULL);
}

static int bucket_of(uint32_t addr)
{
    int lo = 0, hi = prof_map_n - 1, r = -1;
    while (lo <= hi) { int m = (lo + hi) / 2; if (prof_map[m].lo <= addr) { r = m; lo = m + 1; } else hi = m - 1; }
    if (r >= 0 && addr < prof_map[r].hi) return prof_map[r].b;
    return B_OTHER;
}

static void report(const char *why)
{
    static uint32_t last_cyc[2];
    static int64_t last_us;
    int64_t now = esp_timer_get_time();
    double secs = (now - last_us) / 1e6;
    if (last_us == 0) secs = 0;
    for (int core = 1; core >= 0; core--) {
        uint32_t cnt[B_N] = { 0 }, top_a[6] = { 0 }, top_c[6] = { 0 };
        uint32_t *h = hist[core];
        for (uint32_t i = 0; i < fl_n + ir_n; i++) {
            uint32_t c = h[i];
            if (!c) continue;
            uint32_t a = i < fl_n ? fl_lo + (i << GR) : ir_lo + ((i - fl_n) << GR);
            cnt[bucket_of(a)] += c;
            for (int k = 0; k < 6; k++) if (c > top_c[k]) { for (int j = 5; j > k; j--) { top_c[j] = top_c[j - 1]; top_a[j] = top_a[j - 1]; } top_c[k] = c; top_a[k] = a; break; }
            h[i] = 0;
        }
        cnt[B_ROM] += nrom[core]; cnt[B_OTHER] += noth[core]; nrom[core] = noth[core] = 0;
        uint32_t tot = 0;
        for (int b = 0; b < B_N; b++) tot += cnt[b];
        uint32_t cyc = prof_cyc();
        double wall = (uint32_t)(cyc - last_cyc[core]);
        last_cyc[core] = cyc;
        uint64_t ic = isr_cyc[core]; isr_cyc[core] = 0;
        uint32_t ns = nsamp[core]; nsamp[core] = 0;
        printf("[prof] %s t=%.0fs core%d samples=%u | ", why, now / 1e6, core, (unsigned)tot);
        for (int b = 0; b < B_N; b++) if (tot && cnt[b] * 1000 / tot) printf("%s %.1f%% ", bname[b], cnt[b] * 100.0 / tot);
        printf("\r\n");
        printf("[prof] core%d waits (%% of wall cycles, events): ", core);
        for (int w = 0; w < PW_N; w++) {
            static const char *const wn[PW_N] = { "disk", "ppa", "idle_sleep", "lock" };
            uint64_t wc = prof_wait_cyc[core][w]; uint32_t we = prof_wait_cnt[core][w];
            prof_wait_cyc[core][w] = 0; prof_wait_cnt[core][w] = 0;
            printf("%s %.2f%% (%u) ", wn[w], wall > 0 ? wc * 100.0 / wall : 0.0, (unsigned)we);
        }
        printf("| sampler isr %.3f%% of wall (%u samples, body only)\r\n", wall > 0 ? ic * 100.0 / wall : 0.0, (unsigned)ns);
        printf("[prof] core%d top64B:", core);
        for (int k = 0; k < 6 && top_c[k]; k++) printf(" 0x%08x=%u", (unsigned)top_a[k], (unsigned)top_c[k]);
        printf("\r\n");
    }
    (void)secs;
    last_us = now;
}

static void report_task(void *arg)
{
    int64_t next = esp_timer_get_time() + 10000000;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(200));
        if (want_report) { want_report = 0; report("ONESHOT"); next = esp_timer_get_time() + 10000000; }
        else if (esp_timer_get_time() >= next) { report("10s"); next += 10000000; }
    }
}

void prof_request(void) { want_report = 1; }

void prof_init(void)
{
    fl_lo = (uint32_t)&_instruction_reserved_start;
    fl_n = (((uint32_t)&_instruction_reserved_end - fl_lo) >> GR) + 1;
    ir_lo = (uint32_t)&_iram_start;
    ir_n = (((uint32_t)&_iram_end - ir_lo) >> GR) + 1;
    for (int c = 0; c < 2; c++) hist[c] = heap_caps_calloc(fl_n + ir_n, sizeof(uint32_t), MALLOC_CAP_SPIRAM);
    if (!hist[0] || !hist[1]) { printf("[prof] histogram alloc failed\r\n"); return; }
    /* cost of one wait-counter pair, measured here */
    uint32_t c0 = prof_cyc();
    for (int i = 0; i < 1000; i++) { PROF_W_BEGIN(PW_LOCK); PROF_W_END(PW_LOCK); }
    uint32_t pair = (prof_cyc() - c0) / 1000;
    memset(prof_wait_cyc, 0, sizeof prof_wait_cyc); memset(prof_wait_cnt, 0, sizeof prof_wait_cnt);
    printf("[prof] init: flash text %u KB, iram %u KB, histogram %u KB x2 (PSRAM), map %d ranges, wait-counter pair = %u cycles\r\n",
           (unsigned)((fl_n << GR) >> 10), (unsigned)((ir_n << GR) >> 10), (unsigned)(((fl_n + ir_n) * 4) >> 10), prof_map_n, (unsigned)pair);
    xTaskCreatePinnedToCore(core_task, "prof0", 3072, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(core_task, "prof1", 3072, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(report_task, "profrep", 4096, NULL, 1, NULL, 0);
}
#endif

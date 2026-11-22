// B40 glue (RED): halt sleep to the exact timer deadline, loop-collapse speed, and the [b40] counter line.
// The PIT itself is Rust (rust/b40). Core-1 only (emulation task); no locks.
#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

/* iterations/us the guest believes a `LOOP $` runs at = what the interpreter really does, so executed and collapsed
 * iterations agree (the VMM calibrates against whichever it sees). 2 = 2 MIPS, set from the first [perf] reading. */
uint32_t b40_loop_ipus = 2;
unsigned long long b40_loop_skipped;

extern uint32_t b40_next_tick_in_us(void);
extern uint32_t b41b_tl_stats(void); extern unsigned long b41b_arms, b41b_reason[8]; /* RED B41b: IRQ0 edges skipped while the guest idles tickless */
extern uint64_t b40_late_hist(uint32_t i), b40_taken(void), b40_late_max_us(void), b40_dropped(void), b40_warp_total_us(void);

static SemaphoreHandle_t sem;
static esp_timer_handle_t tmr;
unsigned long b40_halt_sleeps, b40_halt_spins_skipped;

static void cb(void *a) { (void)a; BaseType_t w = pdFALSE; xSemaphoreGiveFromISR(sem, &w); }

/* Called by the CPU when halted, no IRQ pending, and the old 512-poll spin is used up. Sleeps until just before the
 * next PIT edge (never longer than 1 ms so USB/keyboard/disk wake-ups keep today's latency), then returns so the
 * poll block raises the tick. Replaces vTaskDelay(1): that is one 1 ms FreeRTOS tick, up to 1 ms late per HLT. */
void b40_halt_sleep(void)
{
	if (!sem) {
		sem = xSemaphoreCreateBinary();
		esp_timer_create_args_t a = { .callback = cb, .name = "b40halt" };
		esp_timer_create(&a, &tmr);
	}
	uint32_t us = b40_next_tick_in_us();
	if (us <= 250)
		return; /* due very soon: spin the rest (poll block runs each time round) */
	uint32_t w = us - 200;
	if (w > 1000)
		w = 1000;
	b40_halt_sleeps++;
	xSemaphoreTake(sem, 0);
	esp_timer_start_once(tmr, w);
	xSemaphoreTake(sem, portMAX_DELAY);
}

/* B45 (set_pointer_abs A5): host queued pointer records -> end the current halt sleep at once. O(1); a stale give is cleared by the
 * xSemaphoreTake(sem, 0) at the start of the next sleep. */
void b40_halt_wake(void) { if (sem) xSemaphoreGive(sem); }

/* one line per 10 s, formats stable: taken, lost, late histogram, loop collapse, halt sleeps */
void b40_report(void)
{
	fprintf(stderr, "[b40] taken %llu lost %llu late_max_us %llu late_hist(<64us,<256us,<1ms,<2ms,<4ms,<8ms,<16ms,more) %llu %llu %llu %llu %llu %llu %llu %llu | loop_skipped %llu warp_us %llu | halt_sleeps %lu | tl_skipped %lu arms %lu fb %lu %lu %lu %lu %lu\r\n",
		b40_taken(), b40_dropped(), b40_late_max_us(),
		b40_late_hist(0), b40_late_hist(1), b40_late_hist(2), b40_late_hist(3), b40_late_hist(4), b40_late_hist(5), b40_late_hist(6), b40_late_hist(7),
		b40_loop_skipped, b40_warp_total_us(), b40_halt_sleeps, (unsigned long) b41b_tl_stats(), b41b_arms, b41b_reason[1], b41b_reason[2], b41b_reason[3], b41b_reason[4], b41b_reason[5]);
}

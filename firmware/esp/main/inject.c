/* Shared input injector (console commands, off the hot path; fixture for rule 33):
 *   ptr dx dy buttons   one guest mouse packet through the normal PS/2 mouse path (the one usb_input.c uses)
 *   p1                  replay pointer_trace_P1.txt (lab:judge/fixtures, sha256 checked) at its recorded timing, then print guest
 *                       instructions per packet for phase A (button up sweep) and phase B (button down drag) and the wall time.
 *                       Source: /sdcard/p1.txt when present, else the copy embedded in the image (esp/main/p1_trace.txt).
 * Both are called from the cardsvc console listener (core 0), the emulator runs on core 1.
 *
 * CONSOLE SEND PROTOCOL (confirmed on board A, tools/bench/logs/inj_test.log, 15 Nov): UART0 = the CH340 port (board A), 115200 8N1.
 *   1. open the port with dtr=False, rts=False BEFORE open (otherwise the board resets)
 *   2. send a bare newline ("
" or "
") first: the console driver eats the first bytes after a quiet period; a command sent
 *      straight away is lost (no echo)
 *   3. wait >= 300 ms
 *   4. send the command, one per line, ending "
" ("" is ignored), at most 39 characters
 *   5. space further commands >= 200 ms apart (the listener polls every 50 ms)
 *   Replies come back on the same UART: "[ptr] sent dx=.. dy=.. buttons=.." and, for p1, "[p1] source .., sha256 .. (matches FIXTURES.md)"
 *   then after ~10 s "[p1] DONE phase A .. instr/packet | phase B .. | wall .. s | sha OK". Send only when the guest is at the idle desktop
 *   (about 100 s after boot). Same listener path as settime / prof / text / shot. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "mbedtls/sha256.h"
#include "../../pc.h"
#include "../../i8042.h"
#include "common.h"

extern const char p1_start[] asm("_binary_p1_trace_txt_start");
extern const char p1_end[] asm("_binary_p1_trace_txt_end");
#define P1_SHA "8b43eaf108cb6b556300f7ca8aca89c865c349dfa9f56a6d173239b63a0d7c6c"   /* lab:judge/FIXTURES.md */

void inject_ptr(const char *a)
{
    int dx = 0, dy = 0, b = 0;
    if (sscanf(a, "%d %d %d", &dx, &dy, &b) < 2 || !globals.mouse) { fprintf(stderr, "[ptr] usage: ptr dx dy buttons\r\n"); return; }
    {   /* GFX95 linear mode: the guest pointer follows the host pointer (gfx_pred_move + coalesced PS/2): go through the same entry as the USB HID mouse, dy positive = down */
        extern int gfx_active(void); extern void b29_mouse_in(int, int, int);
        if (gfx_active()) b29_mouse_in(dx, dy, b); else ps2_mouse_event(globals.mouse, dx, dy, 0, b);
    }
    fprintf(stderr, "[ptr] sent dx=%d dy=%d buttons=%d\r\n", dx, dy, b);
}

static void p1_run(void)
{
    PC *pc = globals.pc;
    char *buf = NULL; size_t len = 0;
    FILE *f = fopen("/sdcard/p1.txt", "rb");
    const char *src = "embedded";
    if (f) {
        fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET);
        buf = malloc(len + 1);
        if (!buf || fread(buf, 1, len, f) != len) { fprintf(stderr, "[p1] read error\r\n"); fclose(f); free(buf); return; }
        fclose(f); src = "/sdcard/p1.txt";
    } else {
        len = p1_end - p1_start;
        buf = malloc(len + 1);
        if (!buf) return;
        memcpy(buf, p1_start, len);
    }
    buf[len] = 0;
    uint8_t h[32]; char hx[65];
    mbedtls_sha256((const unsigned char *)buf, len, h, 0);
    for (int i = 0; i < 32; i++) sprintf(hx + 2 * i, "%02x", h[i]);
    int shaok = !strcmp(hx, P1_SHA);
    fprintf(stderr, "[p1] source %s, %u bytes, sha256 %s (%s)\r\n", src, (unsigned)len, hx, shaok ? "matches FIXTURES.md" : "MISMATCH - not the fixture");
    if (!pc || !globals.mouse) { fprintf(stderr, "[p1] no guest\r\n"); free(buf); return; }

    int64_t t0 = esp_timer_get_time();
    long c_first = 0, c_a_last = 0, c_b_first = 0, c_b_last = 0;
    extern volatile uint32_t x86_busy_pub_us;   /* esp_main.c: guest CPU busy us, sampled at the same points as the cycle count */
    uint32_t b_first = 0, b_a_last = 0, b_b_first = 0, b_b_last = 0;
    int nA = 0, nB = 0, nR = 0, seenB = 0;
    char *save = NULL;
    for (char *ln = strtok_r(buf, "\n", &save); ln; ln = strtok_r(NULL, "\n", &save)) {
        int t, dx, dy, b;
        if (sscanf(ln, "%d %d %d %d", &t, &dx, &dy, &b) != 4) continue;
        while (esp_timer_get_time() < t0 + (int64_t)t * 1000) vTaskDelay(1);
        long c = cpui386_get_cycle(pc->cpu);
        uint32_t bu = x86_busy_pub_us;
        ps2_mouse_event(globals.mouse, dx, dy, 0, b);
        if (b == 0 && !seenB) { if (!nA) { c_first = c; b_first = bu; } c_a_last = c; b_a_last = bu; nA++; }
        else if (b != 0) { if (!seenB) { c_b_first = c; b_b_first = bu; } seenB = 1; c_b_last = c; b_b_last = bu; nB++; }
        else nR++;
    }
    double wall = (esp_timer_get_time() - t0) / 1e6;
    fprintf(stderr, "[p1] DONE phase A (button up) %d packets: %ld instr = %.0f instr/packet | phase B (button down) %d packets: %ld instr = %.0f instr/packet | release packets %d | wall %.3f s | sha %s\r\n",
            nA, c_a_last - c_first, nA > 1 ? (double)(c_a_last - c_first) / (nA - 1) : 0.0,
            nB, c_b_last - c_b_first, nB > 1 ? (double)(c_b_last - c_b_first) / (nB - 1) : 0.0, nR, wall, shaok ? "OK" : "MISMATCH");
    /* same spans as the instr figures above (first..last packet of the phase, per packet = span / (n - 1)) */
    uint32_t bA = b_a_last - b_first, bB = b_b_last - b_b_first;
    fprintf(stderr, "[p1x] A busy_ms=%.1f us_per_pkt=%.0f B busy_ms=%.1f us_per_pkt=%.0f\r\n",
            bA / 1000.0, nA > 1 ? (double)bA / (nA - 1) : 0.0, bB / 1000.0, nB > 1 ? (double)bB / (nB - 1) : 0.0);
    free(buf);
}

static void p1_task(void *a) { p1_run(); vTaskDelete(NULL); }
/* own task (8 KB stack): the console listener task is small and must keep polling */
void inject_p1(void) { xTaskCreatePinnedToCore(p1_task, "p1", 8192, NULL, 2, NULL, 0); }

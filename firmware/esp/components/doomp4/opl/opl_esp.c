// ============================================================
//  opl_esp.c — ESP32-P4 OPL backend (replaces opl_sdl.c)
//
//  Same structure as chocolate-doom 2.3.0's SDL backend, but the
//  pump is inverted: instead of an SDL audio callback pulling
//  samples, the game's own mixer task calls OPL_ESP_Render() once
//  per chunk and mixes the returned stereo stream into its output.
//  FreeRTOS mutexes replace the SDL ones; the Nuked OPL3 emulator
//  (opl3.c) is unchanged.
// ============================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"

#include "opl.h"
#include "opl_internal.h"
#include "opl_queue.h"
#include "fmopl.h"   /* B84: OPL2 core (MAME/QEMU fmopl, Doom-boot copy); Nuked OPL3 needs ~1.2 cores at 360 MHz (board B t5) */

typedef struct
{
    unsigned int rate;
    unsigned int enabled;
    unsigned int value;
    uint64_t expire_time;
} opl_timer_t;

static SemaphoreHandle_t callback_mutex;
static SemaphoreHandle_t callback_queue_mutex;
static opl_callback_queue_t *callback_queue;
static uint64_t current_time;
static int opl_esp_paused;
static uint64_t pause_offset;

static FM_OPL *opl_chip;      /* B84: fmopl, tables + state in PSRAM */
static int16_t *opl_mono;     /* fmopl renders mono; the mixer takes stereo */
static int opl_opl3mode;
static int const register_num__b82i = 0;  /* B82: initial value in flash */
static int register_num;  /* B82: live copy, .bss -> PSRAM */
static opl_timer_t const timer1__b82i = { 12500, 0, 0, 0 };  /* B82: initial value in flash */
static opl_timer_t timer1;  /* B82: live copy, .bss -> PSRAM */
static opl_timer_t const timer2__b82i = { 3125, 0, 0, 0 };  /* B82: initial value in flash */
static opl_timer_t timer2;  /* B82: live copy, .bss -> PSRAM */

static int const initialized__b82i = 0;  /* B82: initial value in flash */
static int initialized;  /* B82: live copy, .bss -> PSRAM */
static unsigned int mixing_freq;

int OPL_ESP_IsActive(void) { return initialized; }
/* B84: micro-benchmark of the Nuked core on the calling core (9 voices keyed on, 128 samples at 44.1 kHz) -> us, -1 no RAM.
 * board B t2: 128 samples took ~108 ms in the mixer (host: 0.03 ms) - this tells core / RAM / load apart. */
#include <stdint.h>
int64_t esp_timer_get_time(void);   /* esp_timer is linked; doomp4 does not REQUIRE it */
int b84_opl_bench(int internal)
{
    (void)internal;
    static int16_t b[128];
    FM_OPL *c = OPLCreate(3579545, 44100);
    if (!c) return -1;
    for (int ch = 0; ch < 9; ch++) {
        int op = (ch % 3) + (ch / 3) * 8;
        OPLWrite(c, 0, 0x20 + op); OPLWrite(c, 1, 0x01); OPLWrite(c, 0, 0x40 + op); OPLWrite(c, 1, 0x10); OPLWrite(c, 0, 0x60 + op); OPLWrite(c, 1, 0xF0);
        OPLWrite(c, 0, 0x80 + op); OPLWrite(c, 1, 0x77); OPLWrite(c, 0, 0xA0 + ch); OPLWrite(c, 1, 0x98); OPLWrite(c, 0, 0xB0 + ch); OPLWrite(c, 1, 0x31);
    }
    int64_t t = esp_timer_get_time();
    YM3812UpdateOne(c, b, 128);
    int us = (int)(esp_timer_get_time() - t);
    OPLDestroy(c);
    return us;
}

// Advance emulated time, firing due callbacks (same as SDL AdvanceTime).
static void AdvanceTime(unsigned int nsamples)
{
    opl_callback_t callback;
    void *callback_data;
    uint64_t us;

    xSemaphoreTake(callback_queue_mutex, portMAX_DELAY);
    us = ((uint64_t) nsamples * OPL_SECOND) / mixing_freq;
    current_time += us;
    if (opl_esp_paused)
        pause_offset += us;

    while (!OPL_Queue_IsEmpty(callback_queue)
        && current_time >= OPL_Queue_Peek(callback_queue) + pause_offset)
    {
        if (!OPL_Queue_Pop(callback_queue, &callback, &callback_data))
            break;
        xSemaphoreGive(callback_queue_mutex);
        xSemaphoreTake(callback_mutex, portMAX_DELAY);
        callback(callback_data);
        xSemaphoreGive(callback_mutex);
        xSemaphoreTake(callback_queue_mutex, portMAX_DELAY);
    }
    xSemaphoreGive(callback_queue_mutex);
}

// Render nsamples of STEREO 16-bit music into buffer (2*nsamples int16s),
// firing scheduled register-write callbacks at their exact sample position.
// Called from the game's mixer task.
void OPL_ESP_Render(int16_t *buffer, unsigned int buffer_len)
{
    unsigned int filled = 0;

    if (!initialized) {
        memset(buffer, 0, buffer_len * 2 * sizeof(int16_t));
        return;
    }

    while (filled < buffer_len)
    {
        uint64_t nsamples;

        xSemaphoreTake(callback_queue_mutex, portMAX_DELAY);
        if (opl_esp_paused || OPL_Queue_IsEmpty(callback_queue))
        {
            nsamples = buffer_len - filled;
        }
        else
        {
            uint64_t next_callback_time = OPL_Queue_Peek(callback_queue) + pause_offset;
            nsamples = (next_callback_time - current_time) * mixing_freq;
            nsamples = (nsamples + OPL_SECOND - 1) / OPL_SECOND;
            if (nsamples > buffer_len - filled)
                nsamples = buffer_len - filled;
        }
        xSemaphoreGive(callback_queue_mutex);

        YM3812UpdateOne(opl_chip, opl_mono, nsamples);   /* mono -> L = R */
        for (unsigned i = 0; i < nsamples; i++) buffer[(filled + i) * 2] = buffer[(filled + i) * 2 + 1] = opl_mono[i];
        filled += nsamples;
        AdvanceTime(nsamples);
    }
}

static int OPL_ESP_Init(unsigned int port_base)
{
    (void)port_base;

    { extern int b84_up(void); b84_up(); }   /* B84: the mixer pumps OPL time; start it even if the sfx module was not chosen */
    opl_esp_paused = 0;
    pause_offset = 0;
    mixing_freq = opl_sample_rate;

    callback_queue = OPL_Queue_Create();
    current_time = 0;

    /* B84: INTERNAL alone may return LP SRAM (ALLOW_RTC_FAST_MEM_AS_HEAP): board B t3 bench 115 ms vs 3 ms (PSRAM) per 128 samples.
     * DMA-capable internal = HP L2MEM only. */
    if (!opl_mono) opl_mono = heap_caps_malloc(1024 * sizeof *opl_mono, MALLOC_CAP_SPIRAM);   /* >= the largest render (opl_task: 512) */
    if (!opl_chip) opl_chip = OPLCreate(3579545, mixing_freq);   /* YM3812 clock */
    if (!opl_chip || !opl_mono) return 0;
    printf("[b84] OPL2 (fmopl) at %u Hz\n", mixing_freq);
    opl_opl3mode = 0;

    callback_mutex = xSemaphoreCreateMutex();
    callback_queue_mutex = xSemaphoreCreateMutex();

    initialized = 1;
    return 1;
}

static void OPL_ESP_Shutdown(void)
{
    initialized = 0;
    if (callback_queue) { OPL_Queue_Destroy(callback_queue); callback_queue = NULL; }
    if (callback_mutex) { vSemaphoreDelete(callback_mutex); callback_mutex = NULL; }
    if (callback_queue_mutex) { vSemaphoreDelete(callback_queue_mutex); callback_queue_mutex = NULL; }
}

static unsigned int OPL_ESP_PortRead(opl_port_t port)
{
    unsigned int result = 0;
    if (port == OPL_REGISTER_PORT_OPL3)
        return 0xff;
    result = 0x06;   /* B84: present as OPL2 (fmopl) */
    if (timer1.enabled && current_time > timer1.expire_time)
        result |= 0x80 | 0x40;
    if (timer2.enabled && current_time > timer2.expire_time)
        result |= 0x80 | 0x20;
    return result;
}

static void OPLTimer_CalculateEndTime(opl_timer_t *timer)
{
    if (timer->enabled)
    {
        int tics = 0x100 - timer->value;
        timer->expire_time = current_time
                           + ((uint64_t) tics * OPL_SECOND) / timer->rate;
    }
}

static void WriteRegister(unsigned int reg_num, unsigned int value)
{
    switch (reg_num)
    {
        case OPL_REG_TIMER1:
            timer1.value = value;
            OPLTimer_CalculateEndTime(&timer1);
            break;
        case OPL_REG_TIMER2:
            timer2.value = value;
            OPLTimer_CalculateEndTime(&timer2);
            break;
        case OPL_REG_TIMER_CTRL:
            if (value & 0x80)
            {
                timer1.enabled = 0;
                timer2.enabled = 0;
            }
            else
            {
                if ((value & 0x40) == 0)
                {
                    timer1.enabled = (value & 0x01) != 0;
                    OPLTimer_CalculateEndTime(&timer1);
                }
                if ((value & 0x20) == 0)
                {
                    timer1.enabled = (value & 0x02) != 0;
                    OPLTimer_CalculateEndTime(&timer2);
                }
            }
            break;
        case OPL_REG_NEW:
            opl_opl3mode = value & 0x01;
            // fall through
        default:
            if (reg_num < 0x100) { OPLWrite(opl_chip, 0, reg_num); OPLWrite(opl_chip, 1, value); }   /* OPL2: the OPL3 bank is ignored */
            break;
    }
}

static void OPL_ESP_PortWrite(opl_port_t port, unsigned int value)
{
    if (port == OPL_REGISTER_PORT)
        register_num = value;
    else if (port == OPL_REGISTER_PORT_OPL3)
        register_num = value | 0x100;
    else if (port == OPL_DATA_PORT)
        WriteRegister(register_num, value);
}

static void OPL_ESP_SetCallback(uint64_t us, opl_callback_t callback, void *data)
{
    xSemaphoreTake(callback_queue_mutex, portMAX_DELAY);
    OPL_Queue_Push(callback_queue, callback, data,
                   current_time - pause_offset + us);
    xSemaphoreGive(callback_queue_mutex);
}

static void OPL_ESP_ClearCallbacks(void)
{
    xSemaphoreTake(callback_queue_mutex, portMAX_DELAY);
    OPL_Queue_Clear(callback_queue);
    xSemaphoreGive(callback_queue_mutex);
}

static void OPL_ESP_Lock(void)   { xSemaphoreTake(callback_mutex, portMAX_DELAY); }
static void OPL_ESP_Unlock(void) { xSemaphoreGive(callback_mutex); }
static void OPL_ESP_SetPaused(int paused) { opl_esp_paused = paused; }

static void OPL_ESP_AdjustCallbacks(float factor)
{
    xSemaphoreTake(callback_queue_mutex, portMAX_DELAY);
    OPL_Queue_AdjustCallbacks(callback_queue, current_time, factor);
    xSemaphoreGive(callback_queue_mutex);
}

static opl_driver_t const opl_esp_driver__b82i = {
    "ESP32-P4",
    OPL_ESP_Init,
    OPL_ESP_Shutdown,
    OPL_ESP_PortRead,
    OPL_ESP_PortWrite,
    OPL_ESP_SetCallback,
    OPL_ESP_ClearCallbacks,
    OPL_ESP_Lock,
    OPL_ESP_Unlock,
    OPL_ESP_SetPaused,
    OPL_ESP_AdjustCallbacks,
};  /* B82: initial value in flash */
opl_driver_t opl_esp_driver;  /* B82: live copy, .bss -> PSRAM */

/* B82_DATA2PSRAM: generated by lab:LEAD/B82/tools/b82_data2psram.py */
#include <string.h>
void b82i_opl_esp(void)
{
    memcpy((void *)&register_num, (const void *)&register_num__b82i, sizeof register_num__b82i);
    memcpy((void *)&timer1, (const void *)&timer1__b82i, sizeof timer1__b82i);
    memcpy((void *)&timer2, (const void *)&timer2__b82i, sizeof timer2__b82i);
    memcpy((void *)&initialized, (const void *)&initialized__b82i, sizeof initialized__b82i);
    memcpy((void *)&opl_esp_driver, (const void *)&opl_esp_driver__b82i, sizeof opl_esp_driver__b82i);
}

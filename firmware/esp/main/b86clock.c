/* B86 (owner 21 Nov): the Windows clock follows the board clock (RX8025T, local time ICT).
 * Windows 95 keeps the time of day in the BIOS tick count at 0040:006C (DOS CLOCK$ -> INT 1Ah). The host reads that dword every
 * 10 s from the emulator's main loop (no guest work) and compares it with the guest RTC view (host clock + CMOS offset, what
 * cold boot and P4TIME use). When it is more than 2 s off, or 10 min passed since the last sync, the host arms the existing P4TIME
 * port (misc.c p4time_read32; p4time.vxd in the card overlay polls it in its idle callback and sets the DOS/Windows clock +
 * WM_TIMECHANGE). Resume arms the same port (snapshot.c), so every boot path ends on one mechanism. */
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include "esp_rom_sys.h"

extern volatile int b61_time_flag;          /* misc.c: P4TIME armed */
extern volatile uint32_t b86_p4t_served;    /* misc.c: P4TIME deliveries */
extern int64_t b86_tofs(void);              /* misc.c: guest RTC offset (s) */
volatile int b86_host_stepped;              /* rtc.c: the host clock was corrected (refine / settime) */

#define DAY_MS 86400000LL
static int64_t last_arm_us = -1, last_sync_us = -1;
static uint32_t served_seen, arms;

/* guest - host time of day in ms (wrapped into +-12 h); bda = the tick dword */
static int64_t diff_ms(uint32_t bda, int64_t *host_ms, int64_t *guest_ms)
{
	struct timeval tv; gettimeofday(&tv, NULL);
	time_t t = tv.tv_sec + (time_t) b86_tofs(); struct tm tm; localtime_r(&t, &tm);
	*host_ms = ((tm.tm_hour * 60 + tm.tm_min) * 60 + tm.tm_sec) * 1000LL + tv.tv_usec / 1000;
	*guest_ms = (int64_t) bda * 65536000LL / 1193180;   /* 18.2065 ticks/s */
	int64_t d = (*guest_ms - *host_ms) % DAY_MS;
	if (d > DAY_MS / 2) d -= DAY_MS; else if (d <= -DAY_MS / 2) d += DAY_MS;
	return d;
}

static void hms(char *o, int64_t ms)   /* "HH:MM:SS.cc" */
{
	ms %= DAY_MS; int s = (int)(ms / 1000), c = (int)(ms % 1000) / 10;
	o[0] = '0' + s / 36000; o[1] = '0' + s / 3600 % 10; o[2] = ':'; o[3] = '0' + s / 600 % 6; o[4] = '0' + s / 60 % 10; o[5] = ':';
	o[6] = '0' + s % 60 / 10; o[7] = '0' + s % 10; o[8] = '.'; o[9] = '0' + c / 10; o[10] = '0' + c % 10; o[11] = 0;
}

static void report(const char *why, uint32_t bda, int64_t d, int64_t h, int64_t g)
{
	char hs[12], gs[12]; hms(hs, h); hms(gs, g);
	esp_rom_printf("[b86] %s host %s guest %s diff %s%d ms tofs %d served %u arms %u\r\n", why, hs, gs, d < 0 ? "-" : "+", (int)(d < 0 ? -d : d),
		       (int) b86_tofs(), (unsigned) b86_p4t_served, (unsigned) arms);
}

/* every 10 s from the main loop. active = Windows is up (desktop reached on a cold boot, or resumed); bda = guest dword 0x46C.
 */
void b86_clock_check(int active, uint32_t bda, int64_t now_us)
{
	int64_t h, g, d = diff_ms(bda, &h, &g);
	if (b86_p4t_served != served_seen) { served_seen = b86_p4t_served; last_sync_us = now_us; report("synced", bda, d, h, g); }
	if (!active) return;
	int stale = last_sync_us < 0 || now_us - last_sync_us >= 600000000LL;
	int off = d > 2000 || d < -2000;
	if ((off || stale || b86_host_stepped) && !b61_time_flag && (last_arm_us < 0 || now_us - last_arm_us >= 30000000LL)) {
		b86_host_stepped = 0; b61_time_flag = 1; last_arm_us = now_us; arms++;
		report(off ? "arm (off)" : stale ? "arm (10 min)" : "arm (host clock stepped)", bda, d, h, g);
	} else report("check", bda, d, h, g);
}
void b86_clock_arm(const char *why) { b61_time_flag = 1; arms++; esp_rom_printf("[b86] arm (%s) served %u arms %u\r\n", why, (unsigned) b86_p4t_served, (unsigned) arms); }

/* console `wclock`: one line now (the main loop's copy of the dword is read live) */
void b86_wclock(uint32_t bda) { int64_t h, g, d = diff_ms(bda, &h, &g); report("wclock", bda, d, h, g); }

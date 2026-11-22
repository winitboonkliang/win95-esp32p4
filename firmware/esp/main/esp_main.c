/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include "esp_rom_sys.h"
#include "esp_heap_caps.h"
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_psram.h"
#include "esp_partition.h"
#include "driver/uart.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"
#include "esp_timer.h"

#include "../../ini.h"
#include "../../pc.h"
#include "../../gfxcard.h"
void b29_present_start(PC *pc);
#include "../../snapshot.h"
#include <sys/stat.h>
#include "common.h"
extern volatile int card_quiet;   /* cardsvc.c: set while a screenshot streams over the console line */
#include "prof.h"

//
#include "esp_private/system_internal.h"
uint32_t get_uticks()
{
	return esp_system_get_time();
}

void *psmalloc(long size);
void *fbmalloc(long size);
void *bigmalloc(size_t size)
{
	return psmalloc(size);
}

static char *pcram;
static long pcram_off;
static long pcram_len;
uint32_t b86_bda(void) { return pcram ? *(volatile uint32_t *)(pcram - 0xa0000 + 0x46c) : 0; }   /* B86 console wclock: guest BIOS tick dword */
void *pcmalloc(long size)
{
	void *ret = pcram + pcram_off;

	size = (size + 31) / 32 * 32;
	if (pcram_off + size > pcram_len) {
		fprintf(stderr, "pcram error %ld %ld %ld\n", size, pcram_off, pcram_len);
		abort();
	}
	pcram_off += size;
	return ret;
}

void pcmalloc_init(void *ptr, long len)
{
	pcram = ptr;
	pcram_len = len;
}

int load_rom(void *phys_mem, const char *file, uword addr, int backward)
{
	if (file && file[0] == '/') {
		FILE *fp = fopen(file, "rb");
		assert(fp);
		fseek(fp, 0, SEEK_END);
		int len = ftell(fp);
		fprintf(stderr, "%s len %d\n", file, len);
		rewind(fp);
		if (backward)
			fread(phys_mem + addr - len, 1, len, fp);
		else
			fread(phys_mem + addr, 1, len, fp);
		fclose(fp);
		return len;
	}
	const esp_partition_t *part =
		esp_partition_find_first(ESP_PARTITION_TYPE_ANY,
					 ESP_PARTITION_SUBTYPE_ANY,
					 file);
	assert(part);
	int len = part->size;
	fprintf(stderr, "%s len %d\n", file, len);
	if (backward)
		esp_partition_read(part, 0, phys_mem + addr - len, len);
	else
		esp_partition_read(part, 0, phys_mem + addr, len);
	return len;
}

//
EventGroupHandle_t global_event_group;
struct Globals globals;

typedef struct {
	PC *pc;
	u8 *fb1;
	u8 *fb;
} Console;

#define NN 32
Console *console_init(int width, int height)
{
	Console *c = malloc(sizeof(Console));
#ifdef BOARD_FULLFRAME_PRESENT
	/* No strip buffer needed — the PPA reads the logical fb directly,
	 * and 64 KB of internal DMA heap is precious with USB running. */
	c->fb1 = NULL;
#else
	c->fb1 = fbmalloc(LCD_WIDTH * LCD_HEIGHT / NN * 2);
#endif
	if (globals.panel_fb) {
		/* Zero-copy: VGA renders directly into the RGB DMA frame buffer */
		c->fb = globals.panel_fb;
	} else {
		c->fb = bigmalloc(LCD_WIDTH * LCD_HEIGHT * 2);
	}
	return c;
}

void lcd_draw(int x_start, int y_start, int x_end, int y_end, void *src);
#ifdef BOARD_FULLFRAME_PRESENT
/* JC8012P4A1C: the PPA rotates the whole logical frame into the DPI
 * panel in one DMA pass — no strip copies needed.  vga_step is still
 * pumped NN times per redraw to keep the VGA raster timing identical
 * to the strip path (same fix the Wireless-Tag P4C5 fork uses). */
void lcd_present(void *fb);
static void redraw(void *opaque,
		   int x, int y, int w, int h)
{
	Console *s = opaque;
	for (int i = 0; i < NN; i++)
		vga_step(s->pc->vga);
	if (gfx_active()) return;   /* GFX95 linear mode: the b29_present task owns the panel */
	lcd_present(s->fb);
}
#else
static void redraw(void *opaque,
		   int x, int y, int w, int h)
{
	Console *s = opaque;
	for (int i = 0; i < NN; i++) {
		uint16_t *src = (uint16_t *) s->fb;
		src += LCD_WIDTH * LCD_HEIGHT / NN * i;
		memcpy(s->fb1, src, LCD_WIDTH * LCD_HEIGHT / NN * 2);
		lcd_draw(0, LCD_WIDTH / NN * i,
			 LCD_HEIGHT, LCD_WIDTH / NN * (i + 1),
			 s->fb1);
		vga_step(s->pc->vga);
		usleep(900);
	}
}
#endif

/* [x86] guest CPU speed counters, host-side only (the guest sees nothing: no state, no timing it reads).
 * busy = wall time of the i386 loop below MINUS halt episodes (i386.c: first halted step .. first running step,
 * i.e. the 512 poll spins + the HLT sleeps), MINUS synchronous file-image disk reads/writes (ide_read_us,
 * ide_write_us), MINUS the time this loop spends printing its own console reports (x86_rep_us).
 * So busy = instruction execution + device emulation done between instructions (pc_step polls, I/O port
 * handlers, VGA memory) + whatever preempts the i386 task on core 1 (ISRs, higher-priority tasks). */
extern unsigned long long x86_halt_us, ide_read_us, ide_write_us;
extern int64_t x86_halt_t0;
extern unsigned long ide_read_ops, ide_cache_hits;
static int64_t x86_t0, x86_rep_us, x86_desk_us;
static int x86_idle_win;            /* B61j: the last 2 s window was idle (halted >= X86_DESK_IDLE_PCT, no disk read); kept up after DESKTOP */
volatile uint32_t x86_busy_pub_us;   /* busy us, stored every loop pass for core-0 readers (p1); wraps at 71 min: use deltas */
volatile int x86_req;                /* console command "x86" sets it, the i386 loop prints and clears it */
#define X86_DESK_IDLE_PCT 70         /* measured base idle desktop is ~80-85% halted (board B profile log t=121-141 s), never 90% */
static int64_t x86_halt(int64_t now) { return x86_halt_us + (x86_halt_t0 ? now - x86_halt_t0 : 0); }
static int64_t x86_busy(int64_t now) { return now - x86_t0 - x86_halt(now) - (int64_t)(ide_read_us + ide_write_us) - x86_rep_us; }

/* internal heap now / min-ever / largest block (lead budget 16 Nov: >= 64 KB free at the idle desktop, min-ever >= 48 KB) */
#define X86_HEAP (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL), \
	(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)
/* runs after every pc_step on core 1: publish busy, DESKTOP detector, on-demand report */
static void x86_poll(PC *pc, int64_t now)
{
	static int64_t win_t, win_h, win_b;
	static unsigned long win_rd;
#ifdef B76
	{ extern void b76_poll(void *); b76_poll(pc); }   /* APPS B76: test keys enter the i8042 on this task only */
#endif
	x86_busy_pub_us = x86_busy(now);
	if (now - win_t >= 2000000) {
		/* DESKTOP = end of the first 2 s window that ends after t=20 s (t = esp_timer, as in [perf]) in which the guest
		 * was halted >= X86_DESK_IDLE_PCT % of (halted + busy) time and read no disk sector (cache hits count as reads) */
		int64_t h = x86_halt(now) - win_h, b = x86_busy(now) - win_b;
		unsigned long rd = ide_read_ops + ide_cache_hits;
		x86_idle_win = win_t && rd == win_rd && h * 100 >= (h + b) * X86_DESK_IDLE_PCT;   /* B61j: the golden needs this too */
		if (!x86_desk_us && win_t && now > 20000000 && x86_idle_win && !card_quiet) {
			x86_desk_us = now;
			fprintf(stderr, "[x86] DESKTOP t=%.1fs instr=%ld busy_ms=%lld halt_ms=%lld disk_ms=%lld (2 s window: halted %lld%%, 0 disk reads) heap_int free=%u min=%u largest=%u\r\n",
				now / 1e6, cpui386_get_cycle(pc->cpu), x86_busy(now) / 1000, x86_halt(now) / 1000,
				(long long)(ide_read_us + ide_write_us) / 1000, h * 100 / (h + b ? h + b : 1), X86_HEAP);
			x86_rep_us += esp_timer_get_time() - now;   /* printing is not guest work */
		}
		win_t = now; win_h = x86_halt(now); win_b = x86_busy(now); win_rd = rd;
	}
	if (x86_req && !card_quiet) {
		x86_req = 0;
		int64_t b = x86_busy(now);
		long cyc = cpui386_get_cycle(pc->cpu);
		fprintf(stderr, "[x86] since boot t=%.1fs instr=%ld busy_ms=%lld halt_ms=%lld disk_ms=%lld busy_mips=%.3f desktop_t=%.1fs heap_int free=%u min=%u largest=%u\r\n",
			now / 1e6, cyc, b / 1000, x86_halt(now) / 1000, (long long)(ide_read_us + ide_write_us) / 1000,
			b > 0 ? (double)cyc / b : 0.0, x86_desk_us / 1e6, X86_HEAP);
		x86_rep_us += esp_timer_get_time() - now;
	}
}

/* B61j diag: strings the stuck guest points at from its stack (far 16:16 pointers): names the text of a system-modal error box */
static void b61j_stack_strings(PC *pc)
{
	uint32_t st[32]; cpui386_get_state(pc->cpu, st); int n = 0;
	fprintf(stderr, "[b61j] stuck at %04x:%x ss=%04x esp=%x cpl=%u\r\n", (unsigned) st[17], (unsigned) st[22], (unsigned) st[18], (unsigned) st[4], (unsigned) st[25]);
	for (uint32_t o = 0; o < 0x1000 && n < 24; o += 2) {
		uint8_t w[4]; int ok = 1;
		for (int k = 0; k < 4 && ok; k++) { long ph = cpui386_lin2phys(pc->cpu, st[11] + ((st[4] + o + k) & 0xffff), 0); if (ph < 0 || ph >= (long) pc->phys_mem_size) ok = 0; else w[k] = pc->phys_mem[ph]; }
		if (!ok) continue;
		unsigned off = w[0] | w[1] << 8, sel = w[2] | w[3] << 8; uword base, lim;
		if (!(sel & 4) || !cpui386_sel_info(pc->cpu, sel, &base, &lim) || off > lim) continue;
		char s[81]; int i = 0;
		for (; i < 80 && off + i <= lim; i++) { long ph = cpui386_lin2phys(pc->cpu, base + off + i, 0); if (ph < 0 || ph >= (long) pc->phys_mem_size) break; char c = pc->phys_mem[ph]; if (!c) break; if (c < 32 || c > 126) { i = -1; break; } s[i] = c; }
		if (i >= 6) { s[i] = 0; n++; fprintf(stderr, "[b61j] stack+%03x -> %04x:%04x \"%s\"\r\n", (unsigned) o, sel, off, s); }
	}
}
static int pc_main(const char *file)
{
	PCConfig conf;
	memset(&conf, 0, sizeof(conf));
	conf.mem_size = 8 * 1024 * 1024;
	conf.vga_mem_size = 256 * 1024;
	conf.cpu_gen = 4;
	conf.fpu = 0;

	int err = ini_parse(file, parse_conf_ini, &conf);
	if (err) {
		fprintf(stderr, "error %d\n", err);
		return err;
	}

	if (conf.width != LCD_WIDTH || conf.height != LCD_HEIGHT) {
		fprintf(stderr, "fixing width/height mismatch %dx%d => %dx%d\n",
			conf.width, conf.height, LCD_WIDTH, LCD_HEIGHT);
		conf.width = LCD_WIDTH;
		conf.height = LCD_HEIGHT;
	}

	/* B61: appliance mode (default) = golden snapshot + read-only system disk + RAM overlay; maintenance = file /sdcard/b61_maint present:
	 * disk RW, cold boot, golden snapshot is (re)built at the first idle desktop. */
	int b61_maint = 0;
	{ struct stat stt; b61_maint = stat("/sdcard/b61_maint", &stt) == 0 || (stat("/sdcard/b61snap.A", &stt) != 0 && stat("/sdcard/b61snap.B", &stt) != 0); /* no golden yet = maintenance boot */ }
	setenv("B61_APPLIANCE", "1", 1);   /* hd.img is ALWAYS read-only (rule 16/33); maintenance writes go to the RAM overlay and are saved as the golden overlay file */
	Console *console = console_init(conf.width, conf.height);
	PC *pc = pc_new(redraw, console, console->fb, &conf);
	console->pc = pc;
	globals.pc = pc;
	globals.kbd = pc->kbd;
	globals.mouse = pc->mouse;
	xEventGroupSetBits(global_event_group, BIT0);

	load_bios_and_reset(pc);
	if (pc_gfx95(&conf)) b29_present_start(pc);   /* GFX95 is the default display ([pc] display = vga turns it off) */

	pc->boot_start_time = get_uticks();
	{ extern uint32_t snap_fw_id(void); fprintf(stderr, "[b61] firmware id %08lx (app ELF sha256): only a golden written by this exact firmware resumes\r\n", (unsigned long) snap_fw_id()); }
	/* B61: instant-on */
	int b61_snapped = 0, b61_resumed = 0; uint64_t b61_sig = 0; int64_t b61_t_boot = esp_timer_get_time();
	{ struct stat bst; if (stat("/sdcard/b61.bench", &bst) == 0) { extern void b61_bench(const char *); b61_bench("/sdcard/b61snap.A"); } }   /* decode bench on a real golden (B61d) */
	struct stat b61st; int b61_test = stat("/sdcard/b61.off", &b61st) == 0;   /* test mode: cold boot on image + overlays, golden neither read nor written */
	if (b61_test) { extern void b61_overlay_pristine(void); b61_overlay_pristine(); b61_maint = 1; b61_snapped = 1; fprintf(stderr, "[b61] test mode (/sdcard/b61.off): cold boot, no resume, no golden save\r\n"); }
	else {
		/* appliance: hd.img is never written, so no generation counter (something bumped b61.gen after the golden on board A, cause not found; hook stays disabled when g_gen_ready == 0) */
		{ extern int b61_overlay_load(const char *, uint32_t *); extern void b61_overlay_pristine(void);
		  uint32_t oc = 0; int64_t q0 = esp_timer_get_time(), q1 = 0, q2 = 0;   /* lead 21 Nov: ~1.3 s between "firmware id" and the resume start */
		  if (!b61_maint && b61_overlay_load("/sdcard/b61_disk.ovl", &oc) == 0) { q1 = esp_timer_get_time(); b61_sig = b61_disk_sig(conf.disks[0]) ^ ((uint64_t) oc << 32); }
		  else { q1 = esp_timer_get_time(); b61_overlay_pristine(); b61_sig = b61_disk_sig(conf.disks[0]); b61_maint = 1; }
		  q2 = esp_timer_get_time();
		  fprintf(stderr, "[b61] pre-resume: since boot %lld ms, from firmware id %lld ms, overlay load %lld ms, disk sig %lld ms\r\n", q0 / 1000, (q0 - b61_t_boot) / 1000, (q1 - q0) / 1000, (q2 - q1) / 1000); }   /* maintenance base = install overlay (e.g. p4time.vxd) on the pristine hd.img */
		SnapStats st; memset(&st, 0, sizeof st); int64_t t0 = esp_timer_get_time();
		int r = snap_pick_load(pc, "/sdcard/b61snap", b61_sig, 0, &st);   /* B86: wall 0 = the resume reads the live host clock (RX8025T); a time(NULL) taken here was ~1 s old after the load = Windows 1 s late */
		int64_t t1 = esp_timer_get_time();
		b61_resumed = r == SN_OK; b61_snapped = b61_resumed;
		if (b61_resumed) { extern volatile int b29_force_all; b29_force_all = 2; }   /* full re-present of the restored surface */
		{ extern int b61_ov_check(const char *); b61_ov_check(b61_resumed ? "after resume" : "after cold start"); }
		if (!b61_resumed) { extern void b61_overlay_pristine(void); b61_overlay_pristine(); b61_maint = 1; }   /* cold boot needs the pristine disk: drop the golden overlay, rebuild golden */
		fprintf(stderr, "[b61] mode=%s resume=%d (%s) gen=%llu total_ms=%.1f read_ms=%.1f crc_ms=%.1f decomp_ms=%.1f apply_ms=%.2f file=%lu raw=%lu nzpages=%lu\r\n",
			b61_maint ? "MAINT" : "APPLIANCE", r, b61_resumed ? "RESUMED" : "COLD BOOT", (unsigned long long) snap_gen_get(), (t1 - t0) / 1000.0,
			st.t_read_us / 1000.0, st.t_crc_us / 1000.0, st.t_decomp_us / 1000.0, st.t_apply_us / 1000.0, (unsigned long) st.file_bytes, (unsigned long) st.raw_bytes, (unsigned long) st.nz_pages);
		{ extern int b61_cfg_dec, b61_cfg_zfill, b61_cfg_rb, b61_cfg_nb, b61_cfg_dma, b61_cfg_crcw, b61_cfg_pf, b61_cfg_dual; extern uint32_t b61_t_park, b61_t_hdr;
		  fprintf(stderr, "[b61] split dec=%d zfill=%d rb=%dK nb=%d dma=%d crcw=%d pf=%d dual=%d | total_us=%lld zero_us=%lu wait_us=%lu crc_us=%lu dec_us=%lu apply_us=%lu prod_us=%lu dmaw_us=%lu pick_us=%lu park_us=%lu prod_bytes=%lu\r\n", b61_cfg_dec, b61_cfg_zfill, b61_cfg_rb, b61_cfg_nb, b61_cfg_dma, b61_cfg_crcw, b61_cfg_pf, b61_cfg_dual,
			(long long)(t1 - t0), (unsigned long) st.t_zero_us, (unsigned long) st.t_wait_us, (unsigned long) st.t_crc_us, (unsigned long) st.t_dec_us, (unsigned long) st.t_apply_us, (unsigned long) st.t_prod_us, (unsigned long) st.t_dmaw_us, (unsigned long) b61_t_hdr, (unsigned long) b61_t_park, (unsigned long) st.prod_bytes); }
		b61_t_boot = esp_timer_get_time();
	}
	extern uint32_t b61_overlay_sectors(void); extern unsigned long b61_ide_ops; extern int b61_overlay_full(void); extern void b61_overlay_reset(void);
	unsigned long b61_last_ops = b61_ide_ops; int64_t b61_last_io = esp_timer_get_time();

	/* Speed report on the serial console.  Without a number here every
	 * "is it faster?" question is guesswork; MIPS is what the guest
	 * actually feels, and the present counter shows whether the display
	 * is keeping up or eating the budget. */
	extern volatile unsigned long lcd_present_count;
	extern unsigned long cpu_halt_sleeps;
	extern unsigned long long ide_read_us;
	extern unsigned long ide_read_ops, ide_read_sectors;
	int64_t stat_us = esp_timer_get_time();
	long stat_cycle = cpui386_get_cycle(pc->cpu);
	unsigned long stat_pres = lcd_present_count;
	unsigned long stat_halt = cpu_halt_sleeps;
	unsigned long long stat_io = ide_read_us;
	unsigned long stat_ops = ide_read_ops, stat_sect = ide_read_sectors;
	x86_t0 = stat_us;
	int64_t stat_busy = 0, stat_hlt = 0, stat_dsk = 0;

	for (; pc->shutdown_state != 8;) {
		pc_step(pc);

		int64_t now = esp_timer_get_time();
		x86_poll(pc, now);
		{ static int64_t hb; if (now - hb > 5000000 && !card_quiet) { hb = now; { extern int b61_ov_check(const char *); b61_ov_check("alive"); }   /* B29s: console text inside a shot frame = CRC error + resend (was 1 per 5 s) */ uint32_t gs[27]; extern int vga_out_w, vga_out_h; cpui386_get_state(pc->cpu, gs);
		  esp_rom_printf("[b61] alive t=%us cyc=%u cs:eip=%04x:%x halt=%u vga=%dx%d ide_ops=%u ovl=%u stk_min_free=%u\r\n", (unsigned) (now / 1000000), (unsigned) cpui386_get_cycle(pc->cpu), (unsigned) gs[17], (unsigned) gs[22], (unsigned) gs[26], vga_out_w, vga_out_h, (unsigned) b61_ide_ops, (unsigned) b61_overlay_sectors(), (unsigned) uxTaskGetStackHighWaterMark(NULL));   /* lead 21 Nov: ROM printf, integer fields (newlib %.0f/%ld on i386_main's stack); cyc = low 32 bits */ } }   /* B82/B85: i386_main (4 KB task) stack low-water mark, bytes */
		if (b61_ide_ops != b61_last_ops) { b61_last_ops = b61_ide_ops; b61_last_io = now; }
		if (!b61_maint && b61_overlay_full()) {   /* overlay full: drop it, restart from the golden snapshot */
			SnapStats rs; { extern int b61_overlay_load(const char *, uint32_t *); b61_overlay_reset(); b61_overlay_load("/sdcard/b61_disk.ovl", NULL); }
			int r = snap_pick_load(pc, "/sdcard/b61snap", b61_sig, 0, &rs);
			{ extern volatile int b29_force_all; b29_force_all = 2; }
			fprintf(stderr, "[b61] overlay full -> restart from golden: %d (%.1f ms)\r\n", r, (rs.t_read_us + rs.t_crc_us + rs.t_decomp_us + rs.t_apply_us) / 1000.0);
		}
		extern int vga_out_w, vga_out_h;
		/* golden only at a graphics-mode idle desktop (>= 640x480; base Win95 desktop IS 640x480 VGA until B29a lands; PC run: same eip 00a8:e036 at the same instruction count = desktop, screenshot runs/RT2_0044.0.png) */
		/* B61j: AND the guest reached the [x86] DESKTOP condition and the last 2 s window is idle again: a timer + no-disk-I/O rule alone saved a golden of
		 * a boot that spun forever before the desktop (board B 20 Nov, cs 181f) and every later boot would have resumed into it */
		if (b61_maint && !b61_snapped && x86_desk_us && x86_idle_win && now - b61_t_boot > 40000000 && now - b61_last_io > 10000000 && (pc_gfx95(&conf) ? gfx_active() : (vga_out_w >= 640 && vga_out_h >= 480))) {   /* GFX95: the driver has set its mode */   /* idle desktop: build the golden */
			static int64_t busy_log;
			if (!snap_ready(pc)) { if (now - busy_log > 5000000) { busy_log = now; fprintf(stderr, "[b61] golden waits: device busy why=%x\r\n", snap_busy_why); } goto b61_skip; }
			fprintf(stderr, "[b61] golden: saving overlay + snapshot\r\n");
			SnapStats ss; int64_t t0 = esp_timer_get_time();
			extern int b61_overlay_save(const char *, uint32_t *); uint32_t oc = 0;
			extern uint64_t b40_now(void); extern void b40_warp_us(uint64_t); uint64_t w0 = b40_now();
			int ovr = b61_overlay_save("/sdcard/b61_disk.ovl", &oc);
			/* B86: the guest is stopped while the host saves; stop its clock too (warp back by the stall). Without this the PIT owed the stall as
			 * a tick backlog: ~11 s captured in every golden (+11 s after each resume) and ~55 s replayed at 4x after the save. */
			{ uint64_t w1 = b40_now(); b40_warp_us(w0 - w1); w0 = w1; } fprintf(stderr, "[b61s] overlay saved r=%d crc=%08x free_psram=%u\r\n", ovr, (unsigned) oc, (unsigned) heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
			snap_hdr_flags = SNF_DESKTOP;
			int r = ovr == 0 ? snap_save_slot(pc, "/sdcard/b61snap", 0, b61_disk_sig(conf.disks[0]) ^ ((uint64_t) oc << 32), &ss) : SN_IO;
			if (r == SN_OK) b61_sig = b61_disk_sig(conf.disks[0]) ^ ((uint64_t) oc << 32);
			if (r != SN_BUSY) {
				b61_snapped = 1;
				b40_warp_us(w0 - b40_now());   /* B86: the snapshot write stalled the guest too (see above) */
				{ extern void b86_clock_arm(const char *); b86_clock_arm("golden saved"); }   /* B86: the guest clock is behind by the save: set it now */
				fprintf(stderr, "[b61] GOLDEN snapshot written: result=%d gen=%llu file=%lu raw=%lu nzpages=%lu compress_ms=%.1f total_ms=%.1f\r\n", r, (unsigned long long) snap_gen_get(),
					(unsigned long) ss.file_bytes, (unsigned long) ss.raw_bytes, (unsigned long) ss.nz_pages, ss.t_comp_us / 1000.0, (esp_timer_get_time() - t0) / 1000.0);
			}
		}
		b61_skip:;
		/* B61j diag: no DESKTOP 150 s after boot = report once; with /sdcard/b61.hang present the hung machine is also dumped to /sdcard/b61hang.snp
		 * (same format as the golden, header without SNF_DESKTOP so it never resumes) for the PC-side module/loop analysis */
		{ static int hang_done; struct stat hs;
		  if (!hang_done && !x86_desk_us && now - b61_t_boot > 150000000 && !card_quiet) {
			int r = -1; snap_hdr_flags = 0;
			if (stat("/sdcard/b61.hang", &hs) == 0) { SnapStats hss; r = snap_save(pc, "/sdcard/b61hang.snp", 0, 0, 0, &hss); }
			if (r != SN_BUSY) { hang_done = 1; b61j_stack_strings(pc); fprintf(stderr, "[b61j] NO DESKTOP %.0f s after boot: guest state dump r=%d (/sdcard/b61hang.snp if b61.hang present)\r\n", (now - b61_t_boot) / 1e6, r); }
		} }
		if (now - stat_us >= 10000000 && !card_quiet) {   /* muted while a shot streams: any console text inside a frame corrupts it */
			long cyc = cpui386_get_cycle(pc->cpu);
			{	/* [x86]: this interval's guest instructions and busy/halt/disk wall time (see x86_busy) */
				int64_t b = x86_busy(now), h = x86_halt(now), d = ide_read_us + ide_write_us;
				fprintf(stderr, "[x86] t=%.0fs instr=%ld busy_ms=%lld halt_ms=%lld disk_ms=%lld busy_mips=%.3f\r\n",
					now / 1e6, cyc - stat_cycle, (b - stat_busy) / 1000, (h - stat_hlt) / 1000, (d - stat_dsk) / 1000,
					b > stat_busy ? (double)(cyc - stat_cycle) / (b - stat_busy) : 0.0);
				stat_busy = b; stat_hlt = h; stat_dsk = d;
			}
			unsigned long pres = lcd_present_count;
			double secs = (now - stat_us) / 1000000.0;
			unsigned long halt = cpu_halt_sleeps;
			unsigned long long io = ide_read_us;
			unsigned long ops = ide_read_ops;
			unsigned long sect = ide_read_sectors;
			extern unsigned long ide_cache_hits, ide_cache_misses;
			extern int vga_out_w, vga_out_h;	/* active VGA mode: 720x400 text, 1280x800 = Windows desktop (VBE) */
			static unsigned long stat_hit, stat_miss;
			unsigned long hit = ide_cache_hits - stat_hit;
			unsigned long miss = ide_cache_misses - stat_miss;
			stat_hit = ide_cache_hits;
			stat_miss = ide_cache_misses;
			if (!card_quiet) fprintf(stderr,
				"[perf] t=%.0fs %.2f MIPS %.1f fps | "
				"halt %.0f%% | disk %.0f%% %lu rd %lu sec | "
				"cache %lu%% (%lu/%lu) | heap %u | vga %dx%d\r\n",
				now / 1000000.0,
				(cyc - stat_cycle) / secs / 1000000.0,
				(pres - stat_pres) / secs,
				/* each sleep is one 1 ms FreeRTOS tick */
				(halt - stat_halt) / 10.0 / secs,
				(io - stat_io) / 10000.0 / secs,
				ops - stat_ops, sect - stat_sect,
				(hit + miss) ? hit * 100 / (hit + miss) : 0,
				hit, hit + miss,
				(unsigned)esp_get_free_heap_size(), vga_out_w, vga_out_h);
			{	/* guest state: where is Windows waiting, and which interrupts are pending/masked */
				uint32_t gs[27]; unsigned pp[6];
				extern void i8259_dump(PicState2 *, unsigned *);
				cpui386_get_state(pc->cpu, gs); i8259_dump(pc->pic, pp);
				fprintf(stderr, "[gst] cs:eip %04x:%08x fl %08x cpl %u halt %u cr0 %08x | pic1 irr %02x isr %02x imr %02x | pic2 irr %02x isr %02x imr %02x\r\n",
					(unsigned)gs[17], (unsigned)gs[22], (unsigned)gs[8], (unsigned)gs[25], (unsigned)gs[26], (unsigned)gs[24], pp[0], pp[1], pp[2], pp[3], pp[4], pp[5]);
			}
			{	/* a halted guest: show the 80x25 text page (what DOS/Windows printed, even under a logo) every 30 s */
				static int ntext;
				uint32_t gs2[27]; cpui386_get_state(pc->cpu, gs2);
				if (gs2[26] && ntext++ % 3 == 0) { extern void vga_dump_text(VGAState *); vga_dump_text(pc->vga); }
			}
			if (!card_quiet) fprintf(stderr, "[bda] t=%.0fs ticks %lu\r\n", now / 1000000.0, (unsigned long)*(volatile uint32_t *)(pcram - 0xa0000 + 0x46c));
			{ extern void b86_clock_check(int, uint32_t, int64_t); b86_clock_check(b61_resumed || x86_desk_us, *(volatile uint32_t *)(pcram - 0xa0000 + 0x46c), now); }   /* B86: Windows clock vs board clock */
			if (!card_quiet) { extern void b40_report(void); b40_report(); }
			stat_us = now;
			stat_cycle = cyc;
			stat_pres = pres;
			stat_halt = halt;
			stat_io = io;
			stat_ops = ops;
			stat_sect = sect;
			x86_rep_us += esp_timer_get_time() - now;   /* the whole report block is not guest work */
		}
	}
#ifdef B60_FW
	{ extern volatile int b60_fw_power_event;
	  /* every guest write is write-through (fflush per command): the image is consistent here */
	  fprintf(stderr, "[fw] power-off requested (event %d): disk image flushed, safe to unplug\r\n", b60_fw_power_event); }
#endif
	return 0;
}

//

void *esp_psram_get(size_t *size);
void vga_task(void *arg);
void i2s_main();
void wifi_main(const char *, const char *);
void storage_init(void);
void usb_setup(void);
#ifdef CARD_SERVICE
void card_service_boot(void);
void card_service_listener_start(void);
#endif

struct esp_ini_config {
	const char *filename;
	char ssid[16];
	char pass[32];
	int enable_usb;
};

static void i386_task(void *arg)
{
	struct esp_ini_config *config = arg;
	int core_id = esp_cpu_get_core_id();
	fprintf(stderr, "main runs on core %d\n", core_id);
	/* Wait for LCD panel (and panel_fb) to be ready before starting the
	 * PC emulator.  console_init() uses globals.panel_fb if set. */
	xEventGroupWaitBits(global_event_group,
	                    BIT1,
	                    pdFALSE,
	                    pdFALSE,
	                    portMAX_DELAY);
	pc_main(config->filename);
	vTaskDelete(NULL);
}

static void usb_boot_task(void *arg)
{
	usb_setup();
	vTaskDelete(NULL);
}

static char *psram;
static long psram_off;
static long psram_len;
void *psmalloc(long size)
{
	void *ret = psram + psram_off;

	size = (size + 4095) / 4096 * 4096;
	if (psram_off + size > psram_len) {
		fprintf(stderr, "psram error %ld %ld %ld\n", size, psram_off, psram_len);
		abort();
	}
	psram_off += size;
	return ret;
}

void *fbmalloc(long size)
{
	void *fb = (uint8_t *) heap_caps_calloc(1, size, MALLOC_CAP_DMA);
	if (!fb) {
		fprintf(stderr, "fbmalloc error %ld\n", size);
		abort();
	}
	return fb;
}

static int parse_ini(void* user, const char* section,
		     const char* name, const char* value)
{
	struct esp_ini_config *conf = user;
#define SEC(a) (strcmp(section, a) == 0)
#define NAME(a) (strcmp(name, a) == 0)
	if (SEC("esp")) {
		if (NAME("ssid")) {
			if (strlen(value) < sizeof(conf->ssid))
				strcpy(conf->ssid, value);
		} else if (NAME("pass")) {
			if (strlen(value) < sizeof(conf->pass))
				strcpy(conf->pass, value);
		} else if (NAME("enable_usb")) {
			conf->enable_usb = atoi(value);
		}
	}
#undef SEC
#undef NAME
	return 1;
}

int rtc_boot(void);
/* B82 (owner 21 Nov): Doom takes the whole machine. The Doom shortcut sets an RTC flag and restarts; this boot brings up only the panel,
 * SD, touch, USB keys and the engine (no emulator: all PSRAM, both cores, the PSRAM bus to itself). Quit restarts into Windows, which
 * comes back from its golden snapshot. */
int b82_solo_boot(void); int b82_pclk(void); void b82_solo_main(void); void lcd_hw_init(int pclk_mhz, int nfb); int b82_nfb(void); void gsl_start(void);
static void usb_boot_task(void *arg);
static void b82_boot(void)
{
#ifdef CARD_SERVICE
	card_service_listener_start();   /* console: doom stat / doomkey, and svc! for the card tools */
#endif
	i2s_main();
	rtc_boot();
	gsl_start();
	storage_init();
	lcd_hw_init(b82_pclk(), b82_nfb());
	xTaskCreatePinnedToCore(usb_boot_task, "usb_boot", 6144, NULL, 1, NULL, 0);
	b82_solo_main();
}

void app_main(void)
{
	setenv("TZ", "ICT-7", 1); /* Thailand UTC+7, no DST */
	tzset();
	global_event_group = xEventGroupCreate();
	{ extern void b82_usb_guard(void); b82_usb_guard(); }   /* both boots: USB OTG reset before any restart (B85 f716d76) */
	if (b82_solo_boot()) { b82_boot(); return; }
	{ extern void b82_give_bss(void); b82_give_bss(); }   /* Windows boot: Doom's PSRAM .bss back to the heap */
	/* B82 (lead 21 Nov, Doom quit -> Windows <= 2.5 s): the panel reset + JD9365 init (~0.55 s) ran only after the SD mount; start it now,
	 * on core 0, beside the codec/RTC/SD init. lcd_hw_init waits for BIT2 (SD mounted) before the SD overrides (rot180.txt, bl.txt), the
	 * frame buffers and the backlight. */
	{ extern int lcd_wait_sd; lcd_wait_sd = 1; }
	xTaskCreatePinnedToCore(vga_task, "vga_task", 4096, NULL, 0, NULL, 0);

#ifdef ESPDEBUG
	uart_config_t uart_config = {
		.baud_rate = 115200,
		.data_bits = UART_DATA_8_BITS,
		.parity	= UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT,
	};

	uart_param_config(UART_NUM_0, &uart_config);
	if (uart_driver_install(UART_NUM_0, 2 * 1024, 0, 0, NULL, 0) != ESP_OK) {
		assert(false);
	}
#endif

#ifdef CARD_SERVICE
	card_service_listener_start();
#endif
	i2s_main();
	if (rtc_boot()) { extern void rtc_refine_start(void); rtc_refine_start(); } /* RX8025T -> host clock; the guest CMOS reads host time (B86: + seconds-edge refine) */
	{ extern void gsl_start(void); gsl_start(); } /* B45: GSL3680 touch -> pointer engine (after codec + RTC on the shared I2C bus 0) */
	storage_init();
	xEventGroupSetBits(global_event_group, BIT2);   /* SD mount done (or failed): the early panel init may read its overrides now */
#ifdef CARD_SERVICE
	card_service_boot(); /* returns unless the previous boot asked for service mode */
#endif

	esp_psram_init();
#ifndef PSRAM_ALLOC_LEN
	// use the whole psram
	size_t len;
	psram = esp_psram_get(&len);
	psram_len = len;
#else
	psram_len = PSRAM_ALLOC_LEN;
	psram = heap_caps_calloc(1, psram_len, MALLOC_CAP_SPIRAM);
#endif

	const static char *files[] = {
		"/sdcard/b29.ini",   /* maintenance override: a file with this name wins, the owner's tiny386.ini stays untouched */
		"/sdcard/tiny386.ini",
		"/spiflash/tiny386.ini",
		NULL,
	};
	static struct esp_ini_config config;
#ifdef BOARD_USB_DEFAULT_ON
	/* JC8012P4A1C: USB keyboard/mouse are the primary input — default ON
	 * so an ini without `enable_usb = 1` still gets a working keyboard.
	 * The ini can still set enable_usb = 0 to override. */
	config.enable_usb = 1;
#endif
	for (int i = 0; files[i]; i++) {
		if (ini_parse(files[i], parse_ini, &config) == 0) {
			config.filename = files[i];
			break;
		}
	}


#ifndef BOARD_NO_WIFI
	if (config.ssid[0]) {
		wifi_main(config.ssid, config.pass);
	}
#endif

	prof_init();
	if (psram) {
		xTaskCreatePinnedToCore(i386_task, "i386_main", 6144, &config, 3, NULL, 1);   /* lead 21 Nov: 4 KB fell to 92 B free (stk_min_free) on a cold boot; +2 KB heap at create */
		/* vga_task already started at the top of app_main (B82 early panel init) */
	}

	/* USB host comes up beside the emulator (devices attach whenever they like);
	 * it no longer delays the start by 3 s. */
	if (config.enable_usb)
		xTaskCreatePinnedToCore(usb_boot_task, "usb_boot", 6144, NULL, 1, NULL, 0);
}

/* console command "text": dump the guest text page now (see cardsvc.c) */
void diag_text(void)
{
	extern void vga_dump_text(VGAState *);
	if (globals.pc) vga_dump_text(((PC *)globals.pc)->vga);
}

/* B61: firmware identity for the snapshot machine id = CRC of this app's ELF sha256 (any rebuild with a code change = new id = golden refused) */
#include "esp_app_desc.h"
uint32_t snap_fw_id(void)
{
	extern uint32_t snap_crc32(uint32_t crc, const uint8_t *p, size_t n);
	const esp_app_desc_t *d = esp_app_get_description();
	return snap_crc32(0, d->app_elf_sha256, sizeof d->app_elf_sha256);
}

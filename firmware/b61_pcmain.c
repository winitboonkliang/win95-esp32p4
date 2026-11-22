// B61 PC harness: deterministic virtual-time machine (instr / MIPS) + the instant-on logic under test.
// The product logic is: try resume (slot A/B, generation guard) -> else cold boot -> auto snapshot when the desktop is idle.
// Env (all optional):
//   B61_SECS      virtual seconds to run after start (default 60)
//   B61_MIPS      virtual guest speed (default 5, same as B60/B40 runs)
//   B61_BASE      snapshot slot base path (enables resume + autosnap); B61_GEN gen file; B61_HD hd image path (disk signature)
//   B61_AUTOSNAP  1 = snapshot when idle (default 1 if B61_BASE)
//   B61_IDLE_S    virtual seconds without IDE commands that mean "desktop idle" (default 6), B61_MINBOOT_S min (default 15)
//   B61_SNAPNOW   virtual second (since start) at which to force a snapshot regardless of idle (test)
//   B61_TOFF      virtual microseconds added to the clock at start (simulates power-off time before resume)
//   B61_TYPE / B61_KEYS  scripted input like B60 ("sec:text|sec:text", "sec:linuxcode,...")
//   B61_CUT       virtual second at which the process dies without any cleanup (power cut)
//   B61_DIGEST_AT virtual second(s), comma list, at which a state digest is printed
//   B61_SHOT      comma list of virtual seconds for a screenshot (ppm)
//   B61_PREFIX    output prefix for screenshots
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include "pc.h"
#include "snapshot.h"
#include "gfxcard.h"
uint32_t cpui386_get_cr0(struct CPUI386 *);

static PC *g_pc;
static double vt_us = 0;
static long cyc(void) { return g_pc ? cpui386_get_cycle(g_pc->cpu) : 0; }
uint32_t get_uticks() { return (uint32_t)(long long) vt_us; }

#include <sys/mman.h>
void *bigmalloc(size_t size) { return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); }
int load_rom(void *phys_mem, const char *file, uword addr, int backward)
{
	FILE *fp = fopen(file, "rb");
	if (!fp) { fprintf(stderr, "load_rom: open %s failed\n", file); abort(); }
	fseek(fp, 0, SEEK_END); int len = ftell(fp); rewind(fp);
	if (backward) fread((char *) phys_mem + addr - len, 1, len, fp); else fread((char *) phys_mem + addr, 1, len, fp);
	fclose(fp); return len;
}
static void redraw(void *opaque, int x, int y, int w, int h) {}

static int ascii_to_lk(int ch, int *sh)
{
	static const char row1[] = "qwertyuiop", row2[] = "asdfghjkl", row3[] = "zxcvbnm";
	static const int r1[] = {16,17,18,19,20,21,22,23,24,25}, r2[] = {30,31,32,33,34,35,36,37,38}, r3[] = {44,45,46,47,48,49,50};
	*sh = 0;
	if (ch >= 'A' && ch <= 'Z') { *sh = 1; ch += 32; }
	for (int i = 0; row1[i]; i++) if (row1[i] == ch) return r1[i];
	for (int i = 0; row2[i]; i++) if (row2[i] == ch) return r2[i];
	for (int i = 0; row3[i]; i++) if (row3[i] == ch) return r3[i];
	if (ch >= '1' && ch <= '9') return 2 + ch - '1';
	if (ch == '0') return 11;
	switch (ch) { case '\n': return 28; case ' ': return 57; case '.': return 52; case '/': return 53; case '\\': return 43; case '-': return 12; case ';': return 39; case ',': return 51; }
	return 0;
}

static void shot(PC *pc, PCConfig *conf, void *fb, const char *pfx, double sec)
{
	char fn[512]; snprintf(fn, sizeof fn, "%s_%06.1fs.ppm", pfx, sec);
	FILE *pf = fopen(fn, "wb"); if (!pf) return;
	fprintf(pf, "P6\n%d %d\n255\n", conf->width, conf->height);
	for (int i = 0; i < conf->width * conf->height; i++) { unsigned v = ((unsigned *) fb)[i]; unsigned char px[3] = {v >> 16, v >> 8, v}; fwrite(px, 1, 3, pf); }
	fclose(pf);
}
static uint32_t fbcrc(void *fb, int n) { extern uint32_t snap_crc32(uint32_t, const uint8_t *, size_t); return snap_crc32(0, fb, (size_t) n * 4); }
static void digest(PC *pc, void *fb, PCConfig *conf, double el, const char *tag)
{
	uint32_t st[32]; cpui386_get_state(pc->cpu, st);
	fprintf(stderr, "B61_DIGEST %s t=%.1f dig=%016llx fb=%08x cyc=%ld eip=%04x:%x eax=%08x\n", tag, el, (unsigned long long) snap_digest(pc), fbcrc(fb, conf->width * conf->height), cyc(), st[17], st[22], st[0]);
}
uint32_t snap_fw_id(void) { return getenv("B61_FWID") ? (uint32_t) strtoul(getenv("B61_FWID"), NULL, 0) : 0; }   /* test: simulate another firmware */
static double envd(const char *n, double d) { return getenv(n) ? atof(getenv(n)) : d; }

extern unsigned long b61_ide_ops;
extern int b61_overlay_full(void); extern void b61_overlay_reset(void), b61_overlay_pristine(void); extern uint32_t b61_overlay_sectors(void);
extern int b61_overlay_save(const char *, uint32_t *), b61_overlay_load(const char *, uint32_t *);
extern int ide_hda_rw; extern const char *b29_ovl_path;
extern volatile int b60_fw_power_event;
uint32_t b40_loop_ipus = 5;
unsigned long long b40_loop_skipped;

int main(int argc, char *argv[])
{
	PCConfig conf; memset(&conf, 0, sizeof(conf));
	conf.mem_size = 8 * 1024 * 1024; conf.vga_mem_size = 256 * 1024; conf.width = 720; conf.height = 480; conf.cpu_gen = 4;
	if (argc != 2) return 1;
	snap_hdr_flags = SNF_DESKTOP;   /* harness snapshots are taken at its own scripted idle points (tests keep their meaning) */
	if (ini_parse(argv[1], parse_conf_ini, &conf)) return 2;
	double mips = envd("B61_MIPS", 5), run_s = envd("B61_SECS", 60), idle_s = envd("B61_IDLE_S", 6), minboot_s = envd("B61_MINBOOT_S", 15);
	double restart_s = envd("B61_RESTART_AT", -1); int restarted = 0;
	double susp_s = envd("B61_SUSPEND_AT", -1); int ev_seen = 0;
	double snapnow = envd("B61_SNAPNOW", -1), cut_s = envd("B61_CUT", -1);
	const char *base = getenv("B61_BASE"), *genp = getenv("B61_GEN"), *hdp = getenv("B61_HD"), *pfx = getenv("B61_PREFIX") ? getenv("B61_PREFIX") : "shot";
	/* same logic as the board (esp_main.c): hd image ALWAYS read-only, guest writes -> RAM overlay; appliance = golden overlay + snapshot exist,
	 * maintenance (B61_MAINT or no golden) = cold boot on image + driver overlay, golden (overlay file + snapshot) written at the idle desktop */
	char ovlp[512]; snprintf(ovlp, sizeof ovlp, "%s.ovl", base ? base : "b61");
	if (getenv("B61_TEST")) { base = NULL; fprintf(stderr, "[b61] test mode: cold boot, no resume, no golden save\n"); }   /* = /sdcard/b61.off on the board */
	int appliance = base && !getenv("B61_MAINT");
	ide_hda_rw = 0; b29_ovl_path = getenv("B61_OVL");   /* driver overlay (GFX95 + P4PTR), e.g. ptr/img/ptr_%d.ovl */
	int autosnap;
	int64_t wall_now = (int64_t) envd("B61_WALL", 0);
	vt_us = envd("B61_TOFF", 0);
	double vt0 = vt_us;

	void *fb = bigmalloc(conf.width * conf.height * 4);
	PC *pc = pc_new(redraw, NULL, fb, &conf);
	g_pc = pc;
	load_bios_and_reset(pc);
	pc->boot_start_time = get_uticks();
	if (genp) snap_gen_init(genp);
	uint64_t sig = 0;
	{ uint32_t oc = 0;
	  if (appliance && b61_overlay_load(ovlp, &oc) == 0) sig = (hdp ? b61_disk_sig(hdp) : 0) ^ ((uint64_t) oc << 32);
	  else { b61_overlay_pristine(); sig = hdp ? b61_disk_sig(hdp) : 0; appliance = 0; } }

	/* ---- instant-on: resume if a valid snapshot of this disk generation exists ---- */
	int resumed = 0; SnapStats st; memset(&st, 0, sizeof st);
	if (base) {
		uint32_t t0 = 0; { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); t0 = t.tv_sec * 1000000u + t.tv_nsec / 1000; }
		int hs = getenv("B61_HOLE_SENT") != NULL; if (hs) memset((uint8_t *) pc->phys_mem + 0xa0000, 0xA5, 0x20000);   /* B61i test: stands for the board's live host objects in the hole */
		int r = snap_pick_load(pc, base, sig, wall_now, &st);
		if (hs) { const uint8_t *q = (const uint8_t *) pc->phys_mem + 0xa0000; int ok = 1; for (int k = 0; k < 0x20000; k++) if (q[k] != 0xA5) { ok = 0; break; } fprintf(stderr, "B61_HOLE kept=%d (result=%d)\n", ok, r); }
		uint32_t t1; { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); t1 = t.tv_sec * 1000000u + t.tv_nsec / 1000; }
		fprintf(stderr, "B61_RESUME result=%d (%s) gen=%llu total_us=%u read_us=%u crc_us=%u decomp_us=%u apply_us=%u file=%u raw=%u dev=%u nzpages=%u chunks=%u\n", r,
			r == SN_OK ? "RESUMED" : "COLD BOOT", (unsigned long long) snap_gen_get(), t1 - t0, st.t_read_us, st.t_crc_us, st.t_decomp_us, st.t_apply_us, st.file_bytes, st.raw_bytes, st.dev_bytes, st.nz_pages, st.chunks);
		resumed = r == SN_OK;
		if (!resumed) { b61_overlay_pristine(); appliance = 0; sig = hdp ? b61_disk_sig(hdp) : 0; }   /* cold boot needs the pristine disk */
		if (!resumed && r != SN_NOFILE) fprintf(stderr, "B61_RESUME refused reason=%d -> cold boot\n", r);
	}
	if (!resumed && getenv("B61_L2LOAD")) {   /* level 2 boot: post-VMM32 snapshot instead of the DOS stages */
		const char *l2 = getenv("B61_L2LOAD"); char op[600]; snprintf(op, sizeof op, "%s.ovl", l2); uint32_t oc = 0;
		int r = b61_overlay_load(op, &oc) < 0 ? SN_NOFILE : snap_load(pc, l2, sig ^ ((uint64_t) oc << 32), wall_now, &st);
		if (r) b61_overlay_pristine(); else sig ^= (uint64_t) oc << 32;
		fprintf(stderr, "B61_L2LOAD result=%d cyc=%ld read_us=%u decomp_us=%u file=%u\n", r, cyc(), st.t_read_us, st.t_decomp_us, st.file_bytes);
	}
	autosnap = base && !appliance && envd("B61_AUTOSNAP", 1) > 0;
	fprintf(stderr, "B61_MODE %s\n", appliance ? "APPLIANCE" : "MAINT");

	struct { double t; int code, sh; int st; } keys[512]; int nk = 0;
	if (getenv("B61_KEYS")) { char *m = strdup(getenv("B61_KEYS")); for (char *t = strtok(m, ","); t && nk < 512; t = strtok(NULL, ",")) { double tt; int c; if (sscanf(t, "%lf:%d", &tt, &c) == 2) { keys[nk].t = tt; keys[nk].code = c; keys[nk].sh = 0; keys[nk].st = 0; nk++; } } }
	if (getenv("B61_TYPE")) {
		char *m = strdup(getenv("B61_TYPE"));
		for (char *it = strtok(m, "|"); it; it = strtok(NULL, "|")) {
			double tt = atof(it); char *col = strchr(it, ':'); if (!col) continue;
			for (char *p = col + 1; *p && nk < 500; p++) { int sh, c = ascii_to_lk(*p, &sh); if (!c) continue; keys[nk].t = tt; keys[nk].code = c; keys[nk].sh = sh; keys[nk].st = 0; nk++; tt += 0.25; }
		}
	}
	double dig_at[16]; int ndig = 0, dig_done[16] = {0};
	if (getenv("B61_DIGEST_AT")) { char *m = strdup(getenv("B61_DIGEST_AT")); for (char *t = strtok(m, ","); t && ndig < 16; t = strtok(NULL, ",")) dig_at[ndig++] = atof(t); }
	long dcyc[16]; int ndc = 0, dc_done[16] = {0};
	if (getenv("B61_DIGEST_CYC")) { char *m = strdup(getenv("B61_DIGEST_CYC")); for (char *t = strtok(m, ","); t && ndc < 16; t = strtok(NULL, ",")) dcyc[ndc++] = atol(t); }
	double shot_at[16]; int nshot = 0, shot_done[16] = {0};
	if (getenv("B61_SHOT")) { char *m = strdup(getenv("B61_SHOT")); for (char *t = strtok(m, ","); t && nshot < 16; t = strtok(NULL, ",")) shot_at[nshot++] = atof(t); }

	long last_c = cyc(); double last_key_t = -1;
	unsigned long last_reads = b61_ide_ops; double last_io_el = 0; uint64_t last_gen = snap_gen_get();
	int snapped = resumed, forced = 0;     /* a resumed machine already has its snapshot */
	double el = 0;
	for (;;) {
		pc_step(pc); pc_vga_step(pc);
		long c = cyc(); long d = c - last_c; last_c = c;
		vt_us += d > 0 ? d / mips : 50.0;
		el = (vt_us - vt0) / 1e6;
		if (getenv("B61_PROGRESS")) { static unsigned long it; if ((++it & 0x3ff) == 0) { uint32_t st[32]; cpui386_get_state(pc->cpu, st); fprintf(stderr, "B61_PROGRESS it=%lu el=%.2f cyc=%ld cs:ip=%04x:%x halt=%u\n", it, el, c, st[17], st[22], st[26]); } }
		for (int k = 0; k < nk; k++) {
			if (keys[k].st == 0 && el >= keys[k].t && el >= last_key_t + 0.12) {
				if (keys[k].sh) ps2_put_keycode(pc->kbd, 1, 42);
				ps2_put_keycode(pc->kbd, 1, keys[k].code); keys[k].st = 1; last_key_t = el;
			} else if (keys[k].st == 1 && el >= keys[k].t + (keys[k].code == 29 ? 0.4 : 0.06)) {
				ps2_put_keycode(pc->kbd, 0, keys[k].code); if (keys[k].sh) ps2_put_keycode(pc->kbd, 0, 42); keys[k].st = 2;
			}
		}
		for (int i = 0; i < ndig; i++) if (!dig_done[i] && el >= dig_at[i]) { dig_done[i] = 1; char tg[32]; snprintf(tg, sizeof tg, "t%.0f", dig_at[i]); digest(pc, fb, &conf, el, tg); }
		for (int i = 0; i < ndc; i++) if (!dc_done[i] && c >= dcyc[i]) { dc_done[i] = 1; char tg[32]; snprintf(tg, sizeof tg, "c%ld", dcyc[i]); /* repaint first: the digest compares the presented frame too */ pc_vga_step(pc); digest(pc, fb, &conf, el, tg); }
		for (int i = 0; i < nshot; i++) if (!shot_done[i] && el >= shot_at[i]) { shot_done[i] = 1; shot(pc, &conf, fb, pfx, shot_at[i]); }
		/* disk activity tracking for the idle rule */
		if (b61_ide_ops != last_reads || snap_gen_get() != last_gen) { last_reads = b61_ide_ops; last_gen = snap_gen_get(); last_io_el = el; }
		{	/* level 2: snapshot the moment VMM32 turns paging on (32-bit PM, every DOS stage behind us) */
			static int l2done; const char *l2 = getenv("B61_L2SAVE");
			if (l2 && !l2done && (cpui386_get_cr0(pc->cpu) & 0x80000000u)) {
				char op[600]; snprintf(op, sizeof op, "%s.ovl", l2); uint32_t oc = 0; SnapStats ss; memset(&ss, 0, sizeof ss);
				int r = b61_overlay_save(op, &oc) < 0 ? SN_IO : snap_save(pc, l2, 1, (int64_t) time(NULL), sig ^ ((uint64_t) oc << 32), &ss);
				if (r != SN_BUSY) { l2done = 1; fprintf(stderr, "B61_L2SAVE result=%d t=%.2f cyc=%ld file=%u raw=%u nzpages=%u compress_us=%u\n", r, el, cyc(), ss.file_bytes, ss.raw_bytes, ss.nz_pages, ss.t_comp_us); }
			}
		}
		if (autosnap && ((!snapped && snapnow < 0 && el >= minboot_s && el - last_io_el >= idle_s && (!gfx_enabled() || gfx_active())) || (snapnow >= 0 && !forced && el >= snapnow))) {
			SnapStats ss; uint32_t oc = 0; int r = b61_overlay_save(ovlp, &oc) < 0 ? SN_IO : snap_save_slot(pc, base, wall_now > 0 ? wall_now : (int64_t) time(NULL), (hdp ? b61_disk_sig(hdp) : 0) ^ ((uint64_t) oc << 32), &ss);
			if (r == SN_OK) sig = (hdp ? b61_disk_sig(hdp) : 0) ^ ((uint64_t) oc << 32);
			if (r == SN_BUSY) { /* IDE busy: try again at the next step */ }
			else {
				fprintf(stderr, "B61_SNAP result=%d at t=%.1f cyc=%ld gen=%llu file=%u raw=%u dev=%u nzpages=%u chunks=%u stored=%u compress_us=%u write_us=%u\n", r, el, cyc(), (unsigned long long) snap_gen_get(),
					ss.file_bytes, ss.raw_bytes, ss.dev_bytes, ss.nz_pages, ss.chunks, ss.stored_chunks, ss.t_comp_us, ss.t_write_us);
				snapped = 1; forced = snapnow >= 0;
				if (getenv("B61_ROUNDTRIP") && r == SN_OK) {
					/* in-process round trip: digest, power-off gap, resume from the file just written, digest again */
					uint64_t d1 = snap_digest(pc); uint32_t f1 = fbcrc(fb, conf.width * conf.height);
					vt_us += envd("B61_GAP_US", 1e9);
					SnapStats rs; int rr = snap_pick_load(pc, base, sig, wall_now, &rs);
					uint64_t d2 = snap_digest(pc);
					fprintf(stderr, "B61_ROUNDTRIP load=%d digest_before=%016llx digest_after=%016llx %s read_us=%u crc_us=%u decomp_us=%u apply_us=%u\n", rr, (unsigned long long) d1, (unsigned long long) d2, d1 == d2 ? "BIT-EXACT" : "DIFFER", rs.t_read_us, rs.t_crc_us, rs.t_decomp_us, rs.t_apply_us);
					(void) f1; last_c = cyc(); vt0 += envd("B61_GAP_US", 1e9);
				}
				if (getenv("B61_EXIT_AFTER_SNAP")) { digest(pc, fb, &conf, el, "aftersnap"); return 0; }
			}
		}
		if (appliance && (b61_overlay_full() || (restart_s >= 0 && !restarted && el >= restart_s))) {
			/* overlay full (or test): drop it and restart from the golden snapshot; the guest never sees a failed disk twice */
			fprintf(stderr, "B61_RESTART t=%.1f overlay_sectors=%u reason=%s\n", el, b61_overlay_sectors(), b61_overlay_full() ? "overlay full" : "test"); restarted = 1;
			b61_overlay_load(ovlp, NULL); SnapStats rs; int rr = snap_pick_load(pc, base, sig, wall_now, &rs);
			fprintf(stderr, "B61_RESTART load=%d total_us=%u\n", rr, rs.t_read_us + rs.t_crc_us + rs.t_decomp_us + rs.t_apply_us);
			last_c = cyc();
		}
		if (susp_s >= 0 && !ev_seen && el >= susp_s) b60_fw_power_event = 2;   /* test injection of an APM suspend request */
		if (b60_fw_power_event == 2 && base) {      /* Windows Suspend = hibernate: snapshot now, power off; next start resumes */
			SnapStats ss; uint32_t oc = 0; int r = b61_overlay_save(ovlp, &oc) < 0 ? SN_IO : snap_save_slot(pc, base, wall_now > 0 ? wall_now : (int64_t) time(NULL), (hdp ? b61_disk_sig(hdp) : 0) ^ ((uint64_t) oc << 32), &ss);
			if (r == SN_BUSY) { /* IDE transfer in flight: retry on the next step */ }
			else { fprintf(stderr, "B61_HIBERNATE result=%d t=%.1f gen=%llu file=%u write_us=%u\n", r, el, (unsigned long long) snap_gen_get(), ss.file_bytes, ss.t_write_us); ev_seen = 1; b60_fw_power_event = 0; return 0; }
		}
		if (b60_fw_power_event == 1) { fprintf(stderr, "B61_SHUTDOWN t=%.1f gen=%llu (flushed, no snapshot: next start = cold boot + auto snapshot)\n", el, (unsigned long long) snap_gen_get()); return 0; }
		if (cut_s >= 0 && el >= cut_s) { fprintf(stderr, "B61_POWER_CUT at t=%.1f cyc=%ld gen=%llu\n", el, cyc(), (unsigned long long) snap_gen_get()); _exit(9); }
		if (el >= run_s || pc->shutdown_state == 8) break;
	}
	digest(pc, fb, &conf, el, "end");
	fprintf(stderr, "B61_OVERLAY sectors=%u (x512 B)\n", b61_overlay_sectors());
	fprintf(stderr, "B61_END t=%.1f cyc=%ld gen=%llu resumed=%d snapped=%d\n", el, cyc(), (unsigned long long) snap_gen_get(), resumed, snapped);
	if (getenv("B61_DUMP_RAM")) { FILE *df = fopen(getenv("B61_DUMP_RAM"), "wb"); if (df) { fwrite(pc->phys_mem, 1, pc->phys_mem_size, df); fclose(df); } }
	shot(pc, &conf, fb, pfx, 9999);
	return 0;
}

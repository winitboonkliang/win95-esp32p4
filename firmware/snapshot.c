// B61 snapshot container + orchestration. See DESIGN.md.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/stat.h>
#include "pc.h"
#include "snap.h"
#include "gfxcard.h"
#include "snapshot.h"
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
/* all snapshot buffers live in PSRAM: small mallocs would come from the (nearly full) internal heap and returned NULL on the board */
#define B61_MALLOC(n) heap_caps_malloc((n), MALLOC_CAP_SPIRAM)
#define B61_CALLOC(n) heap_caps_calloc(1, (n), MALLOC_CAP_SPIRAM)
#else
#define B61_MALLOC(n) malloc(n)
#define B61_CALLOC(n) calloc(1, (n))
#endif

extern uint32_t snap_crc32(uint32_t crc, const uint8_t *p, size_t n);
extern int snap_lz4_compress(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, uint16_t *tab);
extern int snap_lz4_decompress(const uint8_t *src, size_t n, uint8_t *dst, size_t cap);
extern uint32_t b61_crc_fast(uint32_t crc, const uint8_t *p, size_t n);
extern int snap_lz4_decompress_fast(const uint8_t *src, size_t n, uint8_t *dst, size_t cap);
extern int b40_snap(uint8_t *buf, size_t cap, int rd, uint64_t dt);
extern uint64_t b40_now(void);
int64_t cmos_get_tofs(CMOS *);
void cmos_set_tofs(CMOS *, int64_t);
extern const uint8_t b60_bios_start[] asm("_binary_bios_bin_start");
extern const uint8_t b60_bios_end[] asm("_binary_bios_bin_end");

#define MAGIC 0x53353957u /* "W95S" */
#define VERSION 3   /* 3: firmware id in machine_id; 2: GFX95 + B45 pointer device sections, fpu + display mode in machine_id */
#define PAGE 4096
#define RUN_PAGES 16
#define CHUNK (RUN_PAGES * PAGE)
#define CBOUND (CHUNK + CHUNK / 255 + 64)

typedef struct {            /* 128 bytes, little endian, natural alignment identical on x86-64 and riscv32 */
	uint32_t magic, version, machine_id, flags;
	uint64_t gen, seq, t_now, wall;
	uint32_t ram_size, vram_size, dev_len, dev_crc;
	uint32_t nz_pages, n_chunks, payload_len, payload_crc;
	uint64_t disk_sig;
	uint32_t pad[8];
	uint32_t hdr_crc;
	uint32_t pad2;
} Hdr;
_Static_assert(sizeof(Hdr) == 128, "hdr");

static int b61f(SnapStats *st, int r, int line)
{
	if (r != SN_NOFILE && getenv("B61_VERBOSE")) fprintf(stderr, "B61: snapshot refused reason=%d (snapshot.c:%d)\n", r, line);
	return st->reason = r;
}
extern volatile int b61_time_flag;   /* misc.c: P4TIME port 0xF1E8 armed on resume */
/* B61 resume tuning knobs (experiment file /sdcard/b61.cfg or env B61_CFG: "dec zfill rb_kb nb"; absent = defaults) */
int b61_cfg_dec = 2;      /* 0 = Rust oracle, 1 = asm decoder (+ asm CRC), 2 = asm decoder with PIE 16 B copies (default: measured fastest, bit-exact) */
int b61_cfg_dual = 0;     /* 1 = every second chunk is decoded by a worker thread on the other core (two cores overlap their PSRAM misses) */
int b61_cfg_pf = 0;       /* >0: a low-priority helper on the other core touches the destination pages of the next N chunks (their L2 lines are then filled before the decoder stores into them) */
int b61_cfg_crcw = 1;     /* 1 = payload CRC on a worker thread (producer core) while decoding, 0 = CRC in the decode loop */
int b61_cfg_dma = 0;      /* 1 = decode into 2 x 64 KB internal SRAM bounce buffers, AXI-GDMA copies each chunk to guest RAM (CPU never write-allocates PSRAM) */
int b61_cfg_zfill = 1;    /* 1 = zero only pages that are not in the snapshot, 0 = memset all RAM+VRAM first (old) */
int b61_cfg_rb = 256, b61_cfg_nb = 4;   /* reader block KB, ring depth */
/* B61i: the board's pcmalloc() puts the host's device objects (IDEIFState, IDEState, PCIDevice, VGAState, CPU TLB, ... with host pointers and
 * function pointers) INSIDE guest RAM, in the VGA hole 0xA0000-0xBFFFF (pc.c pcmalloc_init). The guest never sees that RAM (the VGA window covers it),
 * but a snapshot that saves/zero-fills/restores it overwrites live host objects with the golden boot's bytes (stale pointers, or zeros =
 * IDEIFState.set_irq NULL -> "Instruction access fault" MEPC=0 at ide_set_irq; or a stale Overlay pointer -> ov_slot load fault).
 * 1 = the hole is never saved, zeroed or restored (board default); 0 = old behaviour (PC: the hole is plain guest RAM; board experiment only). */
#ifdef ESP_PLATFORM
int b61_cfg_hole = 1;
#else
int b61_cfg_hole = 0;
#endif
#define HOLE_LO_PG (0xa0000 / PAGE)
#define HOLE_HI_PG (0xc0000 / PAGE)
static inline int hole_pg(uint32_t i) { return i >= HOLE_LO_PG && i < HOLE_HI_PG; }
static void cfg_load(void)
{
	int a = 2, b = 1, c = 256, d = 4, g = 0, w = 1, pf = 0, du = 0, ho = b61_cfg_hole; char l[96] = "";
#ifdef ESP_PLATFORM
	FILE *f = fopen("/sdcard/b61.cfg", "r");
#else
	const char *e = getenv("B61_CFG"); FILE *f = e ? fopen(e, "r") : NULL;
#endif
	if (f) { if (fgets(l, sizeof l, f)) sscanf(l, "%d %d %d %d %d %d %d %d %d", &a, &b, &c, &d, &g, &w, &pf, &du, &ho); fclose(f); }
	if (c < 16 || c > 512) c = 256;
	if (d < 2 || d > 8) d = 4;
	b61_cfg_dec = a; b61_cfg_zfill = b; b61_cfg_rb = c; b61_cfg_nb = d; b61_cfg_dma = g; b61_cfg_crcw = w; b61_cfg_dual = du; b61_cfg_hole = ho != 0; b61_cfg_pf = pf < 0 ? 0 : pf > 16 ? 16 : pf;
}
static uint32_t now_us(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (uint32_t)(t.tv_sec * 1000000u + t.tv_nsec / 1000); }

/* ---------------- disk generation file ---------------- */
static char gen_path[512];
static uint64_t g_gen = 1;
static int g_dirty = 1;            /* 1 = a write may have happened since the last snapshot point: bump before the next write */
static int g_gen_ready;
typedef struct { uint64_t gen; uint32_t crc, pad; } GenRec;

static int gen_rec_ok(const GenRec *r) { return r->crc == (snap_crc32(0, (const uint8_t *) &r->gen, 8) ^ 0x47454e31u); }
static int gen_persist(uint64_t g)
{
	GenRec r = { g, 0, 0 }; r.crc = snap_crc32(0, (const uint8_t *) &r.gen, 8) ^ 0x47454e31u;
	FILE *f = fopen(gen_path, "r+b"); if (!f) f = fopen(gen_path, "w+b"); if (!f) return -1;
	fseek(f, (long)(g & 1) * 16, SEEK_SET);          /* alternate slots: a torn write keeps the other record */
	fwrite(&r, 16, 1, f); fflush(f); fsync(fileno(f)); fclose(f);
	return 0;
}
int snap_gen_init(const char *path)
{
	snprintf(gen_path, sizeof gen_path, "%s", path);
	GenRec r[2]; memset(r, 0, sizeof r);
	FILE *f = fopen(gen_path, "rb"); uint64_t best = 0;
	if (f) { fread(r, 16, 2, f); fclose(f); for (int i = 0; i < 2; i++) if (gen_rec_ok(&r[i]) && r[i].gen > best) best = r[i].gen; }
	if (!best) { best = 1; gen_persist(1); }
	g_gen = best; g_dirty = 0; g_gen_ready = 1;
	return 0;
}
uint64_t snap_gen_get(void) { return g_gen; }
void snap_disk_write_hook(void)
{
	{ static int n; if (n < 3) { n++; fprintf(stderr, "[b61] disk write hook called ready=%d dirty=%d caller=%p\n", g_gen_ready, g_dirty, __builtin_return_address(0)); } }  /* B61: finds who bumps the generation */
	if (!g_gen_ready || g_dirty) return;
	g_gen++; g_dirty = 1;
	if (gen_persist(g_gen) < 0) fprintf(stderr, "B61: gen persist FAILED\n");
}
void snap_gen_force_dirty(void) { snap_disk_write_hook(); }
/* a snapshot taken now is valid for the current generation; any later write bumps it */
static void gen_clean_point(void) { g_dirty = 0; }
uint64_t snap_disk_sig(const char *p)
{
	/* disk fingerprint (replaces the generation counter): file size, FatFs/host mtime, CRC of the first 64 KB (MBR, boot sector,
	 * start of the FAT). Any external change of hd.img (imgsync, a copied image, a team) changes at least the mtime. Cost ~20 ms. */
	struct stat st; if (stat(p, &st)) return 0;
	uint8_t *b = malloc(65536); uint32_t c = 0;
	if (b) { FILE *f = fopen(p, "rb"); if (f) { size_t n = fread(b, 1, 65536, f); c = snap_crc32(0, b, n); fclose(f); } free(b); }
	uint64_t m = (uint64_t) st.st_mtime;
	return ((uint64_t) st.st_size) ^ (m * 0x9E3779B97F4A7C15ull) ^ ((uint64_t) c << 17);
}

/* firmware identity: a snapshot is only valid for the exact firmware that wrote it (device models, host state layout, function
 * tables all live in the app image). Board: CRC of the app ELF sha256 (esp_main.c). PC builds: 0 unless the harness overrides it. */
__attribute__((weak)) uint32_t snap_fw_id(void) { return 0; }
uint32_t snap_machine_id(PC *pc)
{
	extern int b29_display_mode;
	uint32_t v[6] = { (uint32_t) pc->phys_mem_size, (uint32_t) pc->vga_mem_size, VERSION, 0x4e4b5531, (uint32_t) gfx_enabled() | ((uint32_t) b29_display_mode << 8), cpui386_has_fpu(pc->cpu) };
	uint32_t c = snap_crc32(0, (const uint8_t *) v, sizeof v);
	uint32_t fw = snap_fw_id(); c = snap_crc32(c, (const uint8_t *) &fw, 4);
	return snap_crc32(c, b60_bios_start, b60_bios_end - b60_bios_start);
}

/* ---------------- machine state <-> bytes ---------------- */
static int devs_snap(PC *pc, Snap *sn)
{
	sn_tag(sn, 0x4d414331);
	SN(sn, pc->port92);
	int32_t sh = pc->shutdown_state; SN(sn, sh); if (sn->rd) pc->shutdown_state = sh;
	i8259_snap(pc->pic, sn);
	{ static uint8_t b[1024]; int32_t n = 0;   /* static: 4 KB task stack */
	  if (!sn->rd) { n = b40_snap(b, sizeof b, sn->norm ? 2 : 0, sn->now_norm64); if (n < 0) { sn->err = 1; return -1; } }
	  SN(sn, n);
	  if (n < 0 || n > (int) sizeof b) { sn->err = 1; return -1; }
	  SNA(sn, b, n);
	  if (sn->rd && !sn->err && b40_snap(b, n, 1, sn->dt64) < 0) { sn->err = 1; return -1; }
	}
	cmos_snap(pc->cmos, sn);
	if (ide_snap(pc->ide, sn) < 0) return -1;
	if (ide_snap(pc->ide2, sn) < 0) return -1;
	pci_snap(pc->i440fx, pc->pcibus, sn);
	vga_snap(pc->vga, sn);
	gfx_snap(sn);      /* after the VBE registers: GFX95 mode, pointer */
	ptrdev_snap(sn);   /* after gfx: the touch engine is synced to the restored pointer */
#ifdef B76
	{ extern void b76_snap_io(uint32_t *v, int wr); uint32_t v[6]; if (!sn->rd) b76_snap_io(v, 0);   /* APPS B76: resident stub registration (host side) */
	  SNA(sn, v, sizeof v); if (sn->rd && !sn->err) b76_snap_io(v, 1); }
#endif
	i8042_snap(pc->i8042, pc->kbd, pc->mouse, sn);
	pcspk_snap(pc->pcspk, sn);
	cpui386_snap(pc->cpu, sn);
	return sn->err ? -1 : 0;
}

static int nz_page(const uint8_t *p)
{
	const uint64_t *q = (const uint64_t *) p;
	for (int i = 0; i < PAGE / 8; i++) if (q[i]) return 1;
	return 0;
}
static uint8_t *page_ptr(PC *pc, uint32_t i)
{
	uint32_t rp = pc->phys_mem_size / PAGE;
	return i < rp ? (uint8_t *) pc->phys_mem + (size_t) i * PAGE : (uint8_t *) pc->vga_mem + (size_t)(i - rp) * PAGE;
}
/* run of consecutive non-zero pages starting at i, at most RUN_PAGES, never across the RAM/VRAM boundary */
static uint32_t run_len(PC *pc, const uint8_t *bm, uint32_t i, uint32_t npg)
{
	uint32_t rp = pc->phys_mem_size / PAGE, n = 1;
	while (n < RUN_PAGES && i + n < npg && (bm[(i + n) >> 3] & (1 << ((i + n) & 7))) && i + n != rp && i + n != HOLE_LO_PG && i + n != HOLE_HI_PG) n++;   /* a run never straddles the host-object hole */
	return n;
}

/* B61i permanent guard: a resume write must land inside the live guest RAM or VRAM block and, with the hole rule on, outside the host-object hole.
 * Violation = report, refuse (the caller falls back to a cold boot), never write. */
static int oob_ok(PC *pc, const char *what, const void *p, size_t len)
{
	const uint8_t *q = p, *r0 = (const uint8_t *) pc->phys_mem, *v0 = (const uint8_t *) pc->vga_mem;
	int in_ram = q >= r0 && len <= pc->phys_mem_size && q - r0 <= (ptrdiff_t) (pc->phys_mem_size - len);
	int in_vram = q >= v0 && len <= pc->vga_mem_size && q - v0 <= (ptrdiff_t) (pc->vga_mem_size - len);
	int hole = in_ram && b61_cfg_hole && (size_t) (q - r0) < (size_t) HOLE_HI_PG * PAGE && (size_t) (q - r0) + len > (size_t) HOLE_LO_PG * PAGE;
	if ((in_ram || in_vram) && !hole) return 1;
	fprintf(stderr, "[b61] OOB write %s off=%ld len=%lu ram=%p/%lu vram=%p/%lu\r\n", what, (long) (q - r0), (unsigned long) len, (const void *) r0, (unsigned long) pc->phys_mem_size, (const void *) v0, (unsigned long) pc->vga_mem_size);
	return 0;
}

void pc_cold_reset(PC *pc)
{
	if (b61_cfg_hole && pc->phys_mem_size > (size_t) HOLE_HI_PG * PAGE) {   /* the host-object hole stays alive */
		memset(pc->phys_mem, 0, (size_t) HOLE_LO_PG * PAGE);
		memset((uint8_t *) pc->phys_mem + (size_t) HOLE_HI_PG * PAGE, 0, pc->phys_mem_size - (size_t) HOLE_HI_PG * PAGE);
	} else memset(pc->phys_mem, 0, pc->phys_mem_size);
	memset(pc->vga_mem, 0, pc->vga_mem_size);
	pc->reset_request = 1;   /* pc_step runs the F26 device reset + load_bios_and_reset */
}

#define STEP(...) do { fprintf(stderr, "[b61s] " __VA_ARGS__); fprintf(stderr, " t_us=%u\r\n", (unsigned) now_us()); } while (0)   /* save-path trace (board hang hunt) */
uint32_t snap_hdr_flags;
static void hdr_seal(Hdr *h) { h->hdr_crc = snap_crc32(0, (const uint8_t *) h, offsetof(Hdr, hdr_crc)); }

int snap_save(PC *pc, const char *path, uint64_t seq, int64_t wall, uint64_t disk_sig, SnapStats *st)
{
	SnapStats z; if (!st) st = &z; memset(st, 0, sizeof *st);
	cfg_load();
	uint32_t t0 = now_us();
	uint32_t npg = (pc->phys_mem_size + pc->vga_mem_size) / PAGE;
	Snap sn = { B61_MALLOC(1 << 16), 0, 1 << 16, 0, 0, 0, 0 };
	STEP("save: devs (buf %p)", (void *) sn.p);
	if (devs_snap(pc, &sn) < 0) { free(sn.p); st->reason = SN_BUSY; return SN_BUSY; }
	uint32_t dev_len = sn.pos;
	Hdr h; memset(&h, 0, sizeof h);
	h.magic = MAGIC; h.version = VERSION; h.machine_id = snap_machine_id(pc); h.gen = snap_gen_get(); h.seq = seq;
	h.t_now = b40_now(); h.wall = (uint64_t) wall; h.ram_size = pc->phys_mem_size; h.vram_size = pc->vga_mem_size;
	h.dev_len = dev_len; h.dev_crc = snap_crc32(0, sn.p, dev_len); h.disk_sig = disk_sig; h.flags = snap_hdr_flags;
	STEP("save: fopen %s dev_len=%u", path, (unsigned) dev_len);
	FILE *f = fopen(path, "wb");
	if (!f) { free(sn.p); return SN_IO; }
	setvbuf(f, NULL, _IOFBF, 1 << 16);
	Hdr blank; memset(&blank, 0, sizeof blank);
	fwrite(&blank, sizeof blank, 1, f);               /* header last: a torn write leaves no valid header */
	fwrite(sn.p, 1, dev_len, f);
	free(sn.p);
	uint8_t *bm = B61_CALLOC((npg + 7) / 8);
	for (uint32_t i = 0; i < npg; i++) if (!(b61_cfg_hole && hole_pg(i)) && nz_page(page_ptr(pc, i))) { bm[i >> 3] |= 1 << (i & 7); h.nz_pages++; }
	STEP("save: bitmap nz=%u (bm %p)", (unsigned) h.nz_pages, (void *) bm);
	fwrite(bm, 1, (npg + 7) / 8, f);
	uint32_t payload_crc = 0, payload_len = 0, tc = 0;
	uint8_t *cb = B61_MALLOC(CBOUND); uint16_t *tab = B61_MALLOC(4096 * 2);
	for (uint32_t i = 0; i < npg;) {
		if (!(bm[i >> 3] & (1 << (i & 7)))) { i++; continue; }
		uint32_t n = run_len(pc, bm, i, npg);
		uint32_t t1 = now_us();
		int cl = snap_lz4_compress(page_ptr(pc, i), n * PAGE, cb, CBOUND, tab);
		tc += now_us() - t1;
		uint32_t rec; const uint8_t *src;
		if (cl < 0 || (uint32_t) cl >= n * PAGE) { rec = n * PAGE | 0x80000000u; src = page_ptr(pc, i); cl = n * PAGE; st->stored_chunks++; }
		else { rec = cl; src = cb; }
		fwrite(&rec, 4, 1, f); fwrite(src, 1, cl, f);
		payload_crc = snap_crc32(payload_crc, (uint8_t *) &rec, 4); payload_crc = snap_crc32(payload_crc, src, cl);
		payload_len += 4 + cl; h.n_chunks++; i += n;
	}
	STEP("save: payload done chunks=%u len=%u (cb %p tab %p)", (unsigned) h.n_chunks, (unsigned) payload_len, (void *) cb, (void *) tab);
	free(cb); free(tab);
	h.payload_len = payload_len; h.payload_crc = payload_crc; hdr_seal(&h);
	fflush(f); fseek(f, 0, SEEK_SET); fwrite(&h, sizeof h, 1, f); fflush(f); fsync(fileno(f));
	fclose(f); free(bm);
	STEP("save: closed");
	st->file_bytes = sizeof h + dev_len + (npg + 7) / 8 + payload_len;
	st->raw_bytes = h.nz_pages * PAGE; st->dev_bytes = dev_len; st->nz_pages = h.nz_pages; st->chunks = h.n_chunks;
	st->t_comp_us = tc; st->t_write_us = now_us() - t0;
	gen_clean_point();
	return SN_OK;
}


/* ---- block reader with read-ahead: the snapshot is read in large ALIGNED blocks with open()/read() (no stdio) by a producer
 * thread (board: runs on the other core while the emulation core decodes), ring of 4 x 256 KB, so total ~ max(read, decode).
 * FatFs only does multi-sector direct DMA when the destination is cache-line aligned (stdio path measured 1.8 MB/s). */
#include <fcntl.h>
#include <pthread.h>
#ifdef ESP_PLATFORM
#include "esp_pthread.h"
#endif
#define NBMAX 8
typedef struct {
	int fd; uint8_t *buf[NBMAX]; size_t len[NBMAX]; size_t rb; int nb; uint32_t t_prod, bytes;
	int head, tail, count, eof, stop, started;   /* ring state (under m) */
	size_t pos; int have;                        /* consumer: current block = buf[head], pos inside it */
	pthread_t th, wth; pthread_mutex_t m; pthread_cond_t cv;
	/* CRC worker (second thread on the producer core): CRC-32 over the file bytes from crc_from, block by block, while the consumer decodes; a block is released
	 * to the producer only after the consumer AND the worker are done with it. */
	int crc_on, crc_set, wstarted, wdone; uint64_t crc_from, foff; uint32_t pseq, cseq, wseq, crc;
	/* decode worker (other core): one job at a time = a chunk (src inside a ring block, dst in guest RAM); a block is not released while a job may still read it */
	pthread_t dth; int dstarted, jbusy, jgo, jbad; const uint8_t *jcp; uint32_t jlen, jcap; uint8_t *jdst;
} Rdr;
static void *rdr_decw(void *a);
static void *al_alloc(size_t n)
{
#ifdef ESP_PLATFORM
	return heap_caps_aligned_alloc(128, n, MALLOC_CAP_SPIRAM);
#else
	return aligned_alloc(128, n);
#endif
}
static void *rdr_producer(void *a)
{
	Rdr *r = a;
	for (;;) {
		pthread_mutex_lock(&r->m);
		while (r->count == r->nb && !r->stop) pthread_cond_wait(&r->cv, &r->m);
		int stop = r->stop, t = r->tail;
		pthread_mutex_unlock(&r->m);
		if (stop) break;
		size_t got = 0;
		uint32_t tp = now_us();
		while (got < r->rb) { ssize_t k = read(r->fd, r->buf[t] + got, r->rb - got); if (k <= 0) break; got += (size_t) k; }
		r->t_prod += now_us() - tp; r->bytes += got;
		pthread_mutex_lock(&r->m);
		r->len[t] = got; r->tail = (t + 1) % r->nb; r->count++; r->pseq++; if (!got) r->eof = 1;
		pthread_cond_broadcast(&r->cv);
		pthread_mutex_unlock(&r->m);
		if (!got) break;
	}
	return NULL;
}
static void *rdr_crcw(void *a)
{
	Rdr *r = a;
	pthread_mutex_lock(&r->m);
	while (!r->crc_set && !r->stop) pthread_cond_wait(&r->cv, &r->m);
	for (;;) {
		while (r->wseq == r->pseq && !r->eof && !r->stop) pthread_cond_wait(&r->cv, &r->m);
		if (r->stop || r->wseq == r->pseq) break;
		int sl = r->wseq % r->nb; size_t len = r->len[sl]; uint64_t fo = r->foff, from = r->crc_from; uint32_t c = r->crc;
		pthread_mutex_unlock(&r->m);
		if (len && fo + len > from) { size_t st0 = fo >= from ? 0 : (size_t)(from - fo); c = b61_crc_fast(c, r->buf[sl] + st0, len - st0); }
		pthread_mutex_lock(&r->m);
		r->crc = c; r->foff += len; r->wseq++; pthread_cond_broadcast(&r->cv);
	}
	r->wdone = 1; pthread_cond_broadcast(&r->cv); pthread_mutex_unlock(&r->m);
	return NULL;
}
static int rdr_open(Rdr *r, const char *path)
{
	memset(r, 0, sizeof *r); r->fd = -1;
	int fd = open(path, O_RDONLY);
	if (fd < 0) return -1;
	r->fd = fd;
	r->rb = (size_t) b61_cfg_rb * 1024; r->nb = b61_cfg_nb;
	for (int i = 0; i < r->nb; i++) { r->buf[i] = al_alloc(r->rb); if (!r->buf[i]) { for (int k = 0; k < r->nb; k++) free(r->buf[k]); close(fd); r->fd = -1; return -1; } }
	pthread_mutex_init(&r->m, NULL); pthread_cond_init(&r->cv, NULL);
	pthread_attr_t at; pthread_attr_init(&at); pthread_attr_setstacksize(&at, 8192);
#ifdef ESP_PLATFORM
	{ esp_pthread_cfg_t pcfg = esp_pthread_get_default_config(); pcfg.pin_to_core = 0; pcfg.stack_size = 8192; esp_pthread_set_cfg(&pcfg); }   /* the emulation task runs on core 1: reader + CRC worker on core 0 */
#endif
	if (pthread_create(&r->th, &at, rdr_producer, r) != 0) { for (int k = 0; k < r->nb; k++) free(r->buf[k]); close(fd); r->fd = -1; return -1; }
	r->started = 1;
	r->crc_on = b61_cfg_dec >= 1 && b61_cfg_crcw;
	if (r->crc_on) { if (pthread_create(&r->wth, &at, rdr_crcw, r) == 0) r->wstarted = 1; else r->crc_on = 0; }
	if (b61_cfg_dual && b61_cfg_dec >= 1) { if (pthread_create(&r->dth, &at, rdr_decw, r) == 0) r->dstarted = 1; }
	return 0;
}
static void rdr_close(Rdr *r)
{
	if (r->fd < 0) return;
	pthread_mutex_lock(&r->m); r->stop = 1; pthread_cond_broadcast(&r->cv); pthread_mutex_unlock(&r->m);
	if (r->started) pthread_join(r->th, NULL);
	if (r->wstarted) pthread_join(r->wth, NULL);
	if (r->dstarted) pthread_join(r->dth, NULL);
	close(r->fd); for (int k = 0; k < r->nb; k++) free(r->buf[k]); r->fd = -1;
}
static int rdr_next(Rdr *r)   /* release the consumed block, wait for the next one */
{
	pthread_mutex_lock(&r->m);
	if (r->have) {
		if (r->crc_on) while (r->wseq <= r->cseq && !r->stop) pthread_cond_wait(&r->cv, &r->m);   /* the CRC worker is still reading this block */
		while (r->jbusy && !r->stop) pthread_cond_wait(&r->cv, &r->m);   /* so is the decode worker */
		r->head = (r->head + 1) % r->nb; r->count--; r->cseq++; r->have = 0; pthread_cond_broadcast(&r->cv);
	}
	while (r->count == 0 && !r->eof) pthread_cond_wait(&r->cv, &r->m);
	int ok = r->count > 0 && r->len[r->head] > 0;
	if (ok) { r->have = 1; r->pos = 0; }
	pthread_mutex_unlock(&r->m);
	return ok;
}
static size_t rdr_read(Rdr *r, void *dst, size_t n)
{
	uint8_t *d = dst; size_t done = 0;
	while (done < n) {
		if (!r->have || r->pos >= r->len[r->head]) { if (!rdr_next(r)) break; }
		size_t k = r->len[r->head] - r->pos; if (k > n - done) k = n - done;
		memcpy(d + done, r->buf[r->head] + r->pos, k); r->pos += k; done += k;
	}
	return done;
}

/* zero-copy view of the next n bytes: a pointer into the current ring block when the record lies inside it (the block stays valid until the next rdr_next), else a copy into tmp */
static const uint8_t *rdr_view(Rdr *r, size_t n, uint8_t *tmp)
{
	if (r->have && r->pos + n <= r->len[r->head]) { const uint8_t *p = r->buf[r->head] + r->pos; r->pos += n; return p; }
	return rdr_read(r, tmp, n) == n ? tmp : NULL;
}

extern int snap_lz4_dec_pie(const uint8_t *src, size_t n, uint8_t *dst, size_t cap);
static inline int dec_call(const uint8_t *s, size_t n, uint8_t *d, size_t cap)
{
	return b61_cfg_dec == 0 ? snap_lz4_decompress(s, n, d, cap) : b61_cfg_dec == 2 ? snap_lz4_dec_pie(s, n, d, cap) : snap_lz4_decompress_fast(s, n, d, cap);
}

#ifdef ESP_PLATFORM
/* DMA write-out: the CPU decodes a chunk into an internal SRAM bounce buffer (L1-resident, no PSRAM read-for-ownership), the AXI GDMA copies it to
 * guest RAM in the background while the CPU decodes the next chunk. Guest RAM lines are invalidated first (zero-fill left dirty lines) and after. */
#include "esp_async_memcpy.h"
#include "esp_cache.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
static SemaphoreHandle_t wr_sem; static async_memcpy_handle_t wr_h; static int wr_inst;
static bool wr_cb(async_memcpy_handle_t h, async_memcpy_event_t *e, void *u) { BaseType_t hp = pdFALSE; xSemaphoreGiveFromISR(wr_sem, &hp); return hp == pdTRUE; }
static void msync_out(const void *p, size_t n, int flags)
{
	uintptr_t a = (uintptr_t) p & ~(uintptr_t) 127, e = ((uintptr_t) p + n + 127) & ~(uintptr_t) 127;
	esp_err_t r = esp_cache_msync((void *) a, e - a, flags);
	if (r != ESP_OK) fprintf(stderr, "[b61] msync fail %d %p %lu\r\n", (int) r, (void *) a, (unsigned long) (e - a));
}
/* prefetch helper: pure loads of the destination lines of chunk next..cons+ahead (core 0, priority 1: runs only while the reader / CRC threads wait) */
typedef struct { uint8_t **p; uint32_t *n; uint32_t nch, ahead; volatile uint32_t cons; volatile int stop, finished; } Pf;
static void pf_task(void *a)
{
	Pf *f = a; uint32_t next = 0; volatile uint32_t sink = 0;
	while (!f->stop && next < f->nch) {
		uint32_t target = f->cons + f->ahead; if (target > f->nch) target = f->nch;
		if (next >= target) { taskYIELD(); continue; }
		const volatile uint8_t *q = (const volatile uint8_t *)((uintptr_t) f->p[next] & ~(uintptr_t) 127);
		const volatile uint8_t *e = f->p[next] + (size_t) f->n[next] * PAGE;
		for (; q < e; q += 128) sink += *q;
		next++;
	}
	(void) sink; f->finished = 1; vTaskDelete(NULL);
}
static int wr_open(uint8_t *bounce[2])
{
	if (!wr_inst) { wr_sem = xSemaphoreCreateCounting(4, 0); async_memcpy_config_t cfg = ASYNC_MEMCPY_DEFAULT_CONFIG(); cfg.backlog = 4; wr_inst = esp_async_memcpy_install_gdma_axi(&cfg, &wr_h) == ESP_OK ? 1 : -1; }
	if (wr_inst < 0) return 0;
	bounce[0] = heap_caps_aligned_alloc(128, CHUNK, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
	bounce[1] = heap_caps_aligned_alloc(128, CHUNK, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
	if (!bounce[0] || !bounce[1]) { heap_caps_free(bounce[0]); heap_caps_free(bounce[1]); bounce[0] = bounce[1] = NULL; return 0; }
	while (xSemaphoreTake(wr_sem, 0) == pdTRUE) { }
	return 1;
}
#endif

static void *rdr_decw(void *a)
{
	Rdr *r = a;
	pthread_mutex_lock(&r->m);
	for (;;) {
		while (!r->jgo && !r->stop) pthread_cond_wait(&r->cv, &r->m);
		if (!r->jgo) break;
		const uint8_t *cp = r->jcp; uint32_t len = r->jlen, cap = r->jcap; uint8_t *dst = r->jdst; r->jgo = 0;
		pthread_mutex_unlock(&r->m);
		int d = dec_call(cp, len, dst, cap);
		pthread_mutex_lock(&r->m);
		if (d != (int) cap) r->jbad = 1;
		r->jbusy = 0; pthread_cond_broadcast(&r->cv);
	}
	pthread_mutex_unlock(&r->m);
	return NULL;
}

/* common reader: apply = 0 verifies only (decode into one scratch chunk), 1 applies */
/* board: the VGA refresh task (core 0) must not run while a snapshot is applied (board B t2: Store access fault in vga_draw_glyph9 on a
 * half-restored VGA state). The loader raises snap_hold and waits until that task reports it is parked. */
volatile int snap_hold, snap_parked;
uint32_t b61_t_park, b61_t_hdr;   /* resume split: waiting for the VGA task to park, picking the slot (2 x header + machine id) */
static int read_snap_in(PC *pc, const char *path, uint64_t disk_sig, int apply, int64_t wall_now, SnapStats *st, uint64_t *seq_out);
static int read_snap(PC *pc, const char *path, uint64_t disk_sig, int apply, int64_t wall_now, SnapStats *st, uint64_t *seq_out)
{
	if (!apply) return read_snap_in(pc, path, disk_sig, 0, wall_now, st, seq_out);
	snap_hold = 1;
#ifdef ESP_PLATFORM
	{ uint32_t tp = now_us(); for (int i = 0; i < 200 && !snap_parked; i++) usleep(1000); b61_t_park = now_us() - tp; }
#endif
	int r = read_snap_in(pc, path, disk_sig, 1, wall_now, st, seq_out);
	snap_hold = 0;
	return r;
}
static int read_snap_in(PC *pc, const char *path, uint64_t disk_sig, int apply, int64_t wall_now, SnapStats *st, uint64_t *seq_out)
{
	SnapStats z; if (!st) st = &z; memset(st, 0, sizeof *st);
	if (apply) cfg_load();
	uint32_t t0 = now_us();
	Rdr rd;
	if (rdr_open(&rd, path) < 0) return b61f(st, SN_NOFILE, __LINE__);
	Hdr h;
	if (rdr_read(&rd, &h, sizeof h) != sizeof h || h.magic != MAGIC || h.version != VERSION || h.hdr_crc != snap_crc32(0, (uint8_t *) &h, offsetof(Hdr, hdr_crc))) { rdr_close(&rd); return b61f(st, SN_HDR, __LINE__); }
	if (h.machine_id != snap_machine_id(pc) || h.ram_size != pc->phys_mem_size || h.vram_size != pc->vga_mem_size) { rdr_close(&rd); return b61f(st, SN_MACHINE, __LINE__); }
	if (h.gen != snap_gen_get()) { rdr_close(&rd); return b61f(st, SN_GEN, __LINE__); }
	if (apply && !(h.flags & SNF_DESKTOP)) { rdr_close(&rd); return b61f(st, SN_NOTDESK, __LINE__); }   /* never resume a state saved before the desktop */
	if (disk_sig && h.disk_sig && h.disk_sig != disk_sig) { rdr_close(&rd); return b61f(st, SN_DISKSIG, __LINE__); }
	uint32_t npg = (pc->phys_mem_size + pc->vga_mem_size) / PAGE;
	if (h.dev_len > (1 << 16) || h.nz_pages > npg || h.payload_len > (npg * PAGE + npg * 8)) { rdr_close(&rd); return b61f(st, SN_HDR, __LINE__); }
	if (seq_out) *seq_out = h.seq;
	uint8_t *dev = B61_MALLOC(h.dev_len ? h.dev_len : 1);
	uint8_t *bm = B61_MALLOC((npg + 7) / 8);
	if (rdr_read(&rd, dev, h.dev_len) != h.dev_len || h.dev_crc != snap_crc32(0, dev, h.dev_len) || rdr_read(&rd, bm, (npg + 7) / 8) != (npg + 7) / 8) { free(dev); free(bm); rdr_close(&rd); return b61f(st, SN_DEVCRC, __LINE__); }
	if (apply && b61_cfg_hole) {   /* a golden that carries the hole (written with hole=0) would overwrite live host objects: refuse before any write */
		int in = 0; for (uint32_t g = HOLE_LO_PG; g < HOLE_HI_PG && g < npg; g++) if (bm[g >> 3] & (1 << (g & 7))) in = 1;
		if (in && !oob_ok(pc, "golden-carries-host-object-hole", (uint8_t *) pc->phys_mem + (size_t) HOLE_LO_PG * PAGE, (size_t) (HOLE_HI_PG - HOLE_LO_PG) * PAGE)) { free(dev); free(bm); rdr_close(&rd); return b61f(st, SN_HDR, __LINE__); }
	}
	/* streamed: one chunk record at a time (the board has less free heap than the 6 MB payload), CRC folded in while
	 * decoding straight into guest RAM. A bad CRC or record after RAM was touched = cold reset (the machine is rebuilt anyway). */
	if (rd.crc_on) { pthread_mutex_lock(&rd.m); rd.crc_from = sizeof h + h.dev_len + (npg + 7) / 8; rd.crc_set = 1; pthread_cond_broadcast(&rd.cv); pthread_mutex_unlock(&rd.m); }
	uint32_t t1 = now_us(); st->t_read_us = t1 - t0;
	uint8_t *scratch = apply ? NULL : B61_MALLOC(CHUNK);
	uint8_t *cbuf = B61_MALLOC(CBOUND);
	if (!cbuf || (!apply && !scratch)) { free(dev); free(bm); free(cbuf); free(scratch); rdr_close(&rd); return b61f(st, SN_IO, __LINE__); }
	int oob = 0; uint8_t *hole_live = NULL;
	if (apply) {
		uint32_t tz = now_us();
		if (!b61_cfg_hole && pc->phys_mem_size > (size_t) HOLE_HI_PG * PAGE && (hole_live = B61_MALLOC((size_t) (HOLE_HI_PG - HOLE_LO_PG) * PAGE)))
			memcpy(hole_live, (uint8_t *) pc->phys_mem + (size_t) HOLE_LO_PG * PAGE, (size_t) (HOLE_HI_PG - HOLE_LO_PG) * PAGE);   /* diag: live host objects before the old-style restore */
		if (!b61_cfg_zfill) { memset(pc->phys_mem, 0, pc->phys_mem_size); memset(pc->vga_mem, 0, pc->vga_mem_size); }
		else {   /* the pages the snapshot restores are overwritten anyway: zero only the others (a failed load ends in pc_cold_reset, which clears everything) */
			uint32_t rp = pc->phys_mem_size / PAGE;
			for (uint32_t a = 0; a < npg; ) {
				if ((bm[a >> 3] & (1 << (a & 7))) || (b61_cfg_hole && hole_pg(a))) { a++; continue; }   /* hole: live host objects, never zeroed */
				uint32_t b = a + 1; while (b < npg && b != rp && !(bm[b >> 3] & (1 << (b & 7))) && !(b61_cfg_hole && hole_pg(b))) b++;
				if (!oob_ok(pc, "zero-fill", page_ptr(pc, a), (size_t)(b - a) * PAGE)) { oob = 1; break; }
				memset(page_ptr(pc, a), 0, (size_t)(b - a) * PAGE); a = b;
			}
		}
		st->t_zero_us = now_us() - tz;
	}
	uint32_t pp = 0, i = 0, nz = 0, crc = 0, t_rd = 0, t_dc = 0, t_cr = 0, t_de = 0; int bad = oob;
	uint32_t (*crcf)(uint32_t, const uint8_t *, size_t) = b61_cfg_dec ? b61_crc_fast : snap_crc32;
	uint8_t *bounce[2] = { NULL, NULL }; int use_dma = 0, slot = 0, infl = 0, dtog = 0; uint32_t t_dw = 0;
#ifdef ESP_PLATFORM
	Pf pf; memset(&pf, 0, sizeof pf); uint32_t chunk_no = 0;
	if (apply && b61_cfg_pf) {
		uint32_t cnt = 0; for (uint32_t a = 0; a < npg; ) { if (!(bm[a >> 3] & (1 << (a & 7)))) { a++; continue; } a += run_len(pc, bm, a, npg); cnt++; }
		pf.p = B61_MALLOC(cnt * sizeof(uint8_t *)); pf.n = B61_MALLOC(cnt * sizeof(uint32_t));
		if (pf.p && pf.n) {
			uint32_t k = 0; for (uint32_t a = 0; a < npg; ) { if (!(bm[a >> 3] & (1 << (a & 7)))) { a++; continue; } uint32_t n = run_len(pc, bm, a, npg); pf.p[k] = page_ptr(pc, a); pf.n[k++] = n; a += n; }
			pf.nch = cnt; pf.ahead = (uint32_t) b61_cfg_pf;
			if (xTaskCreatePinnedToCore(pf_task, "b61pf", 3072, &pf, 1, NULL, 0) != pdPASS) pf.finished = 1;
		} else pf.finished = 1;
	} else pf.finished = 1;
#endif
#ifdef ESP_PLATFORM
	if (apply && b61_cfg_dma && (use_dma = wr_open(bounce))) {
		msync_out(pc->phys_mem, pc->phys_mem_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_INVALIDATE);
		msync_out(pc->vga_mem, pc->vga_mem_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_INVALIDATE);
	}
#endif
	while (i < npg && !bad) {
		if (!(bm[i >> 3] & (1 << (i & 7)))) { i++; continue; }
		uint32_t n = run_len(pc, bm, i, npg);
		uint32_t ta = now_us();
		uint32_t rec;
		if (rdr_read(&rd, &rec, 4) != 4) { bad = 1; break; }
		uint32_t len = rec & 0x7fffffffu;
		if (len > CBOUND || len > h.payload_len - pp - 4) { bad = 1; break; }
		const uint8_t *cp = rdr_view(&rd, len, cbuf); if (!cp) { bad = 1; break; }
		uint32_t tb = now_us(); t_rd += tb - ta;
		if (!rd.crc_on) { crc = crcf(crc, (uint8_t *) &rec, 4); crc = crcf(crc, cp, len); }
		uint32_t tc = now_us(); t_cr += tc - tb;
		uint8_t *dst = apply ? page_ptr(pc, i) : scratch;
		if (apply && !oob_ok(pc, "decode", dst, (size_t) n * PAGE)) { oob = 1; bad = 1; break; }
		if (rec & 0x80000000u) { if (len != n * PAGE) { bad = 1; break; } memcpy(dst, cp, len); }
		else {
			int d;
#ifdef ESP_PLATFORM
			if (use_dma) {
				if (infl == 2) { uint32_t tw = now_us(); xSemaphoreTake(wr_sem, portMAX_DELAY); infl--; t_dw += now_us() - tw; }
				d = dec_call(cp, len, bounce[slot], n * PAGE);
				if (d == (int)(n * PAGE)) {
					msync_out(bounce[slot], n * PAGE, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
					if (esp_async_memcpy(wr_h, dst, bounce[slot], n * PAGE, wr_cb, NULL) == ESP_OK) { infl++; slot ^= 1; }
					else memcpy(dst, bounce[slot], n * PAGE);   /* queue full: copy by hand */
				}
			} else
#endif
			if (apply && rd.dstarted && (dtog ^= 1)) {   /* every second chunk goes to the worker on the other core */
				pthread_mutex_lock(&rd.m); while (rd.jbusy) pthread_cond_wait(&rd.cv, &rd.m);
				rd.jcp = cp; rd.jlen = len; rd.jcap = n * PAGE; rd.jdst = dst; rd.jbusy = 1; rd.jgo = 1; pthread_cond_broadcast(&rd.cv); pthread_mutex_unlock(&rd.m);
				d = (int)(n * PAGE);
			} else
			d = dec_call(cp, len, dst, n * PAGE);
			if (d != (int)(n * PAGE)) { if (getenv("B61_VERBOSE")) fprintf(stderr, "B61: chunk at page %lu n=%lu len=%lu decoded=%d\n", (unsigned long) i, (unsigned long) n, (unsigned long) len, d); bad = 1; break; }
		}
		{ uint32_t te = now_us(); t_dc += te - tb; t_de += te - tc; }
		pp += 4 + len; i += n; nz += n;
#ifdef ESP_PLATFORM
		pf.cons = ++chunk_no;
#endif
	}
#ifdef ESP_PLATFORM
	pf.stop = 1; while (!pf.finished) vTaskDelay(1);
	free(pf.p); free(pf.n);
#endif
#ifdef ESP_PLATFORM
	if (use_dma) {
		uint32_t tw = now_us(); while (infl > 0) { xSemaphoreTake(wr_sem, portMAX_DELAY); infl--; } t_dw += now_us() - tw;
		msync_out(pc->phys_mem, pc->phys_mem_size, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
		msync_out(pc->vga_mem, pc->vga_mem_size, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
		heap_caps_free(bounce[0]); heap_caps_free(bounce[1]);
	}
#endif
	st->t_dmaw_us = t_dw;
	if (rd.dstarted) { pthread_mutex_lock(&rd.m); while (rd.jbusy) pthread_cond_wait(&rd.cv, &rd.m); if (rd.jbad) bad = 1; pthread_mutex_unlock(&rd.m); }
	if (rd.crc_on && !bad) { uint32_t tw = now_us(); pthread_mutex_lock(&rd.m); while (!rd.wdone) pthread_cond_wait(&rd.cv, &rd.m); crc = rd.crc; pthread_mutex_unlock(&rd.m); t_cr += now_us() - tw; }
	rdr_close(&rd);
	free(scratch); free(bm); free(cbuf);
	st->t_read_us += t_rd; st->t_decomp_us = t_dc; st->t_crc_us = t_cr; st->t_dec_us = t_de; st->t_wait_us = t_rd; st->t_prod_us = rd.t_prod; st->prod_bytes = rd.bytes;
	if (hole_live) {   /* diag (hole=0 only): which words of the live host objects did the golden change? */
		const uint32_t *a = (const uint32_t *) hole_live, *b = (const uint32_t *) ((uint8_t *) pc->phys_mem + (size_t) HOLE_LO_PG * PAGE); uint32_t nd = 0, np = 0;
		for (uint32_t k = 0; k < (HOLE_HI_PG - HOLE_LO_PG) * PAGE / 4; k++) if (a[k] != b[k]) {
			nd++; if ((a[k] >> 28) == 4 || (b[k] >> 28) == 4 || (a[k] >> 24) == 0x4f || (b[k] >> 24) == 0x4f) np++;
			if (nd <= 12) fprintf(stderr, "[b61] HOLE diff +0x%05x live=%08lx golden=%08lx\r\n", (unsigned) (k * 4), (unsigned long) a[k], (unsigned long) b[k]);
		}
		fprintf(stderr, "[b61] HOLE diag: golden restore changed %lu words of the live host objects (%lu look like pointers)\r\n", (unsigned long) nd, (unsigned long) np);
		free(hole_live);
	}
	if (oob || bad || nz != h.nz_pages || pp != h.payload_len || crc != h.payload_crc) { free(dev); if (apply) pc_cold_reset(pc); return b61f(st, SN_PAYLOAD, __LINE__); }
	if (!apply) { free(dev); return SN_OK; }
	uint32_t t3 = now_us();
	uint64_t dt = b40_now() - h.t_now;
	Snap sn = { dev, 0, h.dev_len, 1, 0, (uint32_t) dt, dt };
	int r = devs_snap(pc, &sn);
	free(dev);
	if (r < 0) { pc_cold_reset(pc); return b61f(st, SN_DEVAPPLY, __LINE__); }
	/* clock fix-up (host side): the CMOS clock reads wall_now, or, if the real time is unknown, the time of the snapshot */
	/* P4TIME host contract: the RTC (CMOS, read live from host time + tofs) must show the REAL local time; then arm port 0xEA.
	 * Real time = host clock when it is valid (board: RX8025T via rtc_boot, PC: host), else the time of the snapshot. */
	{ int64_t now = (int64_t) time(NULL); int64_t w = wall_now > 0 ? wall_now : (now > 1700000000 ? now : (int64_t) h.wall); cmos_set_tofs(pc->cmos, w - now); b61_time_flag = 1; }
	st->t_apply_us = now_us() - t3; st->raw_bytes = nz * PAGE; st->nz_pages = nz;
	st->file_bytes = sizeof h + h.dev_len + (npg + 7) / 8 + h.payload_len; st->dev_bytes = h.dev_len; st->chunks = h.n_chunks;
	gen_clean_point();
	return SN_OK;
}

int snap_verify(PC *pc, const char *path, uint64_t disk_sig, uint64_t *seq_out) { return read_snap(pc, path, disk_sig, 0, 0, NULL, seq_out); }
int snap_load(PC *pc, const char *path, uint64_t disk_sig, int64_t wall_now, SnapStats *st) { return read_snap(pc, path, disk_sig, 1, wall_now, st, NULL); }

/* two slots base.A / base.B: write the one that is NOT the newest valid one, verify it; the other stays untouched */
int snap_save_slot(PC *pc, const char *base, int64_t wall, uint64_t disk_sig, SnapStats *st)
{
	static char a[96], b[96]; snprintf(a, sizeof a, "%s.A", base); snprintf(b, sizeof b, "%s.B", base);   /* static: i386_main has a 4 KB stack */
	uint64_t sa = 0, sb = 0; int va = snap_verify(pc, a, disk_sig, &sa) == SN_OK, vb = snap_verify(pc, b, disk_sig, &sb) == SN_OK;
	const char *tgt = (va && (!vb || sa > sb)) ? b : a;
	uint64_t seq = (va ? sa : 0) > (vb ? sb : 0) ? (va ? sa : 0) : (vb ? sb : 0);
	STEP("slot: verified old slots A=%d B=%d -> %s", va, vb, tgt);
	int r = snap_save(pc, tgt, seq + 1, wall, disk_sig, st);
	if (r != SN_OK) return r;
	STEP("slot: verify new");
	{ int vr = snap_verify(pc, tgt, disk_sig, NULL); if (vr != SN_OK) { fprintf(stderr, "B61: verify of new slot failed reason=%d\n", vr); remove(tgt); return SN_PAYLOAD; } }
	return SN_OK;
}
/* header-only check (128 B read): magic, version, header CRC, machine, generation, disk signature */
static int hdr_ok(PC *pc, const char *path, uint64_t disk_sig, uint64_t *seq)
{
	FILE *f = fopen(path, "rb"); Hdr h; if (!f) return 0;
	int ok = fread(&h, sizeof h, 1, f) == 1; fclose(f);
	if (!ok || h.magic != MAGIC || h.version != VERSION || h.hdr_crc != snap_crc32(0, (uint8_t *) &h, offsetof(Hdr, hdr_crc))) return 0;
	if (h.machine_id != snap_machine_id(pc) || h.ram_size != pc->phys_mem_size || h.vram_size != pc->vga_mem_size || h.gen != snap_gen_get()) return 0;
	if (disk_sig && h.disk_sig && h.disk_sig != disk_sig) return 0;
	if (!(h.flags & SNF_DESKTOP)) { fprintf(stderr, "[b61] %s refused: not taken at the desktop\r\n", path); return 0; }
	*seq = h.seq; return 1;
}
/* resume: newest slot whose header is valid is loaded in ONE streamed pass (CRC checked while decoding; a corrupt body
 * cold-resets the machine and the other slot is tried). Saves the full verify read that cost as much as the load itself. */
int snap_pick_load(PC *pc, const char *base, uint64_t disk_sig, int64_t wall_now, SnapStats *st)
{
	static char p[2][96]; snprintf(p[0], 96, "%s.A", base); snprintf(p[1], 96, "%s.B", base);
	uint32_t th0 = now_us();
	uint64_t sq[2] = { 0, 0 }; int v[2] = { hdr_ok(pc, p[0], disk_sig, &sq[0]), hdr_ok(pc, p[1], disk_sig, &sq[1]) };
	b61_t_hdr = now_us() - th0;
	if (!v[0] && !v[1]) return SN_NOFILE;
	int first = (v[0] && (!v[1] || sq[0] >= sq[1])) ? 0 : 1, r = SN_NOFILE;
	for (int k = 0; k < 2; k++) {
		int idx = k ? 1 - first : first;
		if (!v[idx]) continue;
		r = snap_load(pc, p[idx], disk_sig, wall_now, st);
		if (r == SN_OK) return r;
	}
	return r;
}

uint64_t snap_digest(PC *pc)
{
	uint32_t c = snap_crc32(0, (uint8_t *) pc->phys_mem, pc->phys_mem_size);
	uint32_t v = snap_crc32(0, (uint8_t *) pc->vga_mem, pc->vga_mem_size);
	Snap sn = { B61_MALLOC(1 << 16), 0, 1 << 16, 0, 0, 0, 0, 1, get_uticks(), b40_now() };
	int r = devs_snap(pc, &sn);
	uint32_t d = r < 0 ? 0xdeadbeef : snap_crc32(0, sn.p, sn.pos);
	free(sn.p);
	return ((uint64_t) c << 32) ^ ((uint64_t) v << 16) ^ d;
}

/* disk signature of the guest view = image fingerprint ^ driver-overlay records (b29_N.ovl preload): a new driver overlay refuses an old snapshot */
uint64_t b61_disk_sig(const char *hdpath) { extern uint32_t b61_preload_crc; return snap_disk_sig(hdpath) ^ ((uint64_t) b61_preload_crc * 0x100000001ull); }

/* cheap readiness probe before any file is written: every device can be serialized now (no IDE transfer in flight) */
int snap_ready(PC *pc)
{
	Snap sn = { B61_MALLOC(1 << 16), 0, 1 << 16, 0, 0, 0, 0 };
	if (!sn.p) return 0;
	int ok = devs_snap(pc, &sn) == 0; free(sn.p); return ok;
}

// B61 instant-on snapshot API (PC build: stdio; board: same code over the SD file layer)
#ifndef SNAPSHOT_H
#define SNAPSHOT_H
#include <stdint.h>
#include "pc.h"

typedef struct {
	uint32_t raw_bytes, file_bytes, dev_bytes, nz_pages, chunks, stored_chunks;
	uint32_t t_read_us, t_decomp_us, t_apply_us, t_crc_us, t_write_us, t_comp_us;
	uint32_t t_prod_us, prod_bytes, t_zero_us, t_wait_us, t_dec_us, t_dmaw_us;   /* resume split: producer time in read(), bytes read, zero-fill, consumer wait, pure decode */
	int reason;           /* snap_load: 0 ok, else why it was refused (see SN_*) */
} SnapStats;

enum { SN_OK = 0, SN_NOFILE = 1, SN_HDR = 2, SN_MACHINE = 3, SN_GEN = 4, SN_DISKSIG = 5, SN_DEVCRC = 6, SN_PAYLOAD = 7,
       SN_DEVAPPLY = 8, SN_BUSY = 9, SN_IO = 10, SN_NOTDESK = 11 };
/* Hdr.flags: SNF_DESKTOP = the caller saw the guest idle at the desktop when it saved; a resume refuses a snapshot without it
 * (B61j: board B 20 Nov wrote a golden of a boot that hung before the desktop, every later boot would have resumed into the hang) */
#define SNF_DESKTOP 1u
extern uint32_t snap_hdr_flags;                /* written into every saved header; set by the caller before snap_save / snap_save_slot */

/* generation file: <hd>.gen, two tear-safe records */
int  snap_gen_init(const char *path);              /* load or create; returns 0 */
uint64_t snap_gen_get(void);
void snap_gen_force_dirty(void);                   /* test helper: pretend a write happened */
uint64_t snap_disk_sig(const char *hdpath);        /* size + mtime of the image (0 if unavailable) */
uint64_t b61_disk_sig(const char *hdpath);        /* snap_disk_sig ^ driver overlay CRC */

uint32_t snap_machine_id(PC *pc);
int  snap_save(PC *pc, const char *path, uint64_t seq, int64_t wall, uint64_t disk_sig, SnapStats *st);
int  snap_verify(PC *pc, const char *path, uint64_t disk_sig, uint64_t *seq_out);   /* header + CRCs, nothing applied */
int  snap_load(PC *pc, const char *path, uint64_t disk_sig, int64_t guest_wall_now, SnapStats *st);
void pc_cold_reset(PC *pc);
int  snap_pick_load(PC *pc, const char *base, uint64_t disk_sig, int64_t wall_now, SnapStats *st); /* base.A / base.B, newest valid */
int  snap_save_slot(PC *pc, const char *base, int64_t wall, uint64_t disk_sig, SnapStats *st);     /* writes the other slot, verifies */
uint64_t snap_digest(PC *pc);
int snap_ready(PC *pc);                            /* 1 = devices quiescent, a snapshot can be taken now */
extern int snap_busy_why;                      /* CRC over RAM+VRAM+devices+CPU (state equality test) */
#endif

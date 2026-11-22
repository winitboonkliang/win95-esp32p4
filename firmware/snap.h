// B61 machine snapshot: one symmetric (de)serializer per device. Direction = s->rd.
// Format rule: only fixed-width fields, little endian, never a pointer or long; layout is identical on the
// PC (x86-64) and the P4 (riscv32), so a snapshot made on one machine model is valid on the other.
#ifndef SNAP_H
#define SNAP_H
#include <stdint.h>
#include <string.h>

typedef struct Snap {
	uint8_t *p;
	uint32_t pos, cap;
	int rd, err;
	uint32_t dt;       // resume only: get_uticks() now - get_uticks() at snapshot (u32 wrap), rebases device timestamps
	uint64_t dt64;     // same in the 64-bit host clock the PIT uses
	int norm;          // digest mode: timestamps are written relative to now_norm and the wall offset is dropped
	uint32_t now_norm;
	uint64_t now_norm64;
} Snap;

static inline void sn_io(Snap *s, void *x, uint32_t n)
{
	if (s->err || s->pos + n > s->cap) { s->err = 1; if (s->rd) memset(x, 0, n); return; }
	if (s->rd) memcpy(x, s->p + s->pos, n); else memcpy(s->p + s->pos, x, n);
	s->pos += n;
}
#define SN(s, x) sn_io((s), &(x), sizeof(x))
#define SNA(s, a, n) sn_io((s), (a), (n))
/* section tag: written, and verified on read (catches a schema mismatch at the first wrong device) */
static inline void sn_tag(Snap *s, uint32_t t)
{
	uint32_t v = t;
	sn_io(s, &v, 4);
	if (s->rd && v != t) s->err = 1;
}
/* timestamp in get_uticks() units: written raw, rebased by dt on read */
static inline void sn_time(Snap *s, uint32_t *x) { uint32_t v = s->norm ? *x - s->now_norm : *x; sn_io(s, &v, 4); if (s->rd) *x = v + s->dt; }

struct PicState2; struct CMOS; struct KBDState; struct PS2KbdState; struct PS2MouseState;
struct VGAState; struct IDEIFState; struct PCIBus; struct I440FXState; struct PCSpkState; struct CPUI386; struct FPU;
void i8259_snap(struct PicState2 *, Snap *);
void cmos_snap(struct CMOS *, Snap *);
void i8042_snap(struct KBDState *, struct PS2KbdState *, struct PS2MouseState *, Snap *);
void vga_snap(struct VGAState *, Snap *);
int ide_snap(struct IDEIFState *, Snap *);
void pci_snap(struct I440FXState *, struct PCIBus *, Snap *);
void pcspk_snap(struct PCSpkState *, Snap *);
void cpui386_snap(struct CPUI386 *, Snap *);
void fpu_snap(struct FPU *, Snap *);
void gfx_snap(Snap *);
void ptrdev_snap(Snap *);
int cpui386_has_fpu(struct CPUI386 *);

/* disk generation guard (snapshot.c) */
void snap_disk_write_hook(void);
#endif

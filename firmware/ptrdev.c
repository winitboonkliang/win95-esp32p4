/* BLUE B45 pointer device (glue only; the gesture/coalescing logic is the Rust engine core/src/lib.rs).
 * Guest side = our P4PTR.DRV:  IRQ 10 while records wait;  in eax,0xE4 = pop (low: USER flags, 0 = empty; high: x16);
 * in ax,0xE6 = y16 of that record;  out 0xE4,ax: 1 enable, 0 disable, 2 ISR finished.
 * Host side: ptrdev_touch / ptrdev_rel / ptrdev_tick feed the engine; ptrdev_overlay gives the pointer the host draws. */
#include <stdint.h>
#include <stdio.h>
#include "pc.h"
#include "ptrdev.h"

typedef struct { uint32_t t_us; uint16_t flags, px, py, x16, y16, pad; } B45Ev;
#ifdef PTRDEV_STUB
/* PC gate build without the Rust engine (tools/gate): the device exists but stays empty; the stock image never enables it */
void b45_init(const void *c) { (void)c; } void b45_touch(uint32_t t, int d, uint16_t x, uint16_t y) { (void)t; (void)d; (void)x; (void)y; }
void b45_rel(uint32_t t, int dx, int dy, uint8_t b) { (void)t; (void)dx; (void)dy; (void)b; } void b45_tick(uint32_t t) { (void)t; }
void b45_sync(uint16_t x, uint16_t y) { (void)x; (void)y; } int b45_pending(void) { return 0; } int b45_pop(B45Ev *o) { (void)o; return 0; }
uint32_t b45_overlay(uint32_t *s) { if (s) *s = 0; return 0; } void b45_stats(uint32_t *o) { (void)o; }
void b45_set_abs(uint32_t t, uint16_t x, uint16_t y, uint8_t b) { (void)t; (void)x; (void)y; (void)b; } int b45_idle(void) { return 1; }
#endif
void b45_init(const void *cfg);
void b45_touch(uint32_t t, int down, uint16_t rx, uint16_t ry);
void b45_rel(uint32_t t, int dx, int dy, uint8_t b);
void b45_tick(uint32_t t);
void b45_sync(uint16_t x, uint16_t y);
int b45_pending(void);
int b45_pop(B45Ev *out);
uint32_t b45_overlay(uint32_t *seq);
void b45_stats(uint32_t *out);


#ifdef BUILD_ESP32
#include "freertos/FreeRTOS.h"
static portMUX_TYPE plock = portMUX_INITIALIZER_UNLOCKED;
#define LOCK() taskENTER_CRITICAL(&plock)
#define UNLOCK() taskEXIT_CRITICAL(&plock)
#else
#define LOCK()
#define UNLOCK()
#endif

B45Dev b45;
void b45_set_abs(uint32_t t, uint16_t x, uint16_t y, uint8_t b);
int b45_idle(void);
#ifdef BUILD_ESP32
void b40_halt_wake(void);
#define WAKE() do { if (b45.enabled && b45_pending()) b40_halt_wake(); } while (0) /* A5: a halted guest takes the IRQ now, not after its 1 ms sleep */
#else
#define WAKE()
#endif
static PC *dpc;
static void (*dset_irq)(void *, int, int);
static void *dpic;
static uint16_t ylatch;

void ptrdev_init(void *pc, void *pic, void (*set_irq)(void *, int, int))
{
	dpc = (PC *)pc; dpic = pic; dset_irq = set_irq;
	b45_init(0);
}

/* raise IRQ 10 when records wait and the line is low (edge-triggered PIC: one edge per batch, the ISR drains all) */
static void kick_locked(void)
{
	if (b45.enabled && !b45.line && b45_pending()) {
		b45.line = 1; b45.raises++;
		b45.raise_cyc = cpui386_get_cycle(dpc->cpu);
		dset_irq(dpic, PTR_IRQ, 1);
	}
}
/* host calls run on another core on the board: the PIC write happens on the emulator core in ptrdev_poll() */
void ptrdev_touch(uint32_t t_us, int down, int rx, int ry) { LOCK(); b45_touch(t_us, down, rx, ry); b45.host_pend = 1; WAKE(); UNLOCK(); }
void ptrdev_rel(uint32_t t_us, int dx, int dy, int b) { LOCK(); b45_rel(t_us, dx, dy, (uint8_t)b); b45.host_pend = 1; WAKE(); UNLOCK(); }

/* lead addendum (B70 host-assisted window move): exact absolute pixel to Windows, no acceleration, no pacing, never merged */
void ptrdev_set_abs(uint32_t t_us, int x, int y, int b) { LOCK(); b45_set_abs(t_us, x < 0 ? 0 : x, y < 0 ? 0 : y, (uint8_t)b); b45.host_pend = 1; WAKE(); UNLOCK(); }
/* A4: the guest has consumed everything we queued: no record left, no paced move waiting, and USER was called for the last record read
 * (the ISR's done marker came after the last read). O(1). */
int pointer_abs_consumed(void) { LOCK(); int r = b45_idle() && !b45.host_pend && b45.isr_recs_seen == b45.popped; UNLOCK(); return r; }
void ptrdev_tick(uint32_t t_us) { LOCK(); b45_tick(t_us); b45.host_pend = 1; WAKE(); UNLOCK(); }
void ptrdev_sync(int x, int y) { LOCK(); b45_sync(x, y); UNLOCK(); }
uint32_t ptrdev_overlay(uint32_t *seq) { LOCK(); uint32_t v = b45_overlay(seq); UNLOCK(); return v; }
/* emulator core, every step batch */
void ptrdev_poll(void)
{
	if (!b45.host_pend) return;
	LOCK(); b45.host_pend = 0;
	if (!b45.enabled) { B45Ev e; while (b45_pop(&e)) b45.dropped++; } /* no driver yet (boot, DOS): input is not kept for later */
	else kick_locked();
	UNLOCK();
}

/* measurement only: raw records that bypass the engine (delivery cost of a record that changes nothing for Windows) */
static uint32_t rawq[64]; static uint16_t rawy[64]; static int rawh, rawn;
void ptrdev_inject_raw(uint16_t flags, uint16_t x16, uint16_t y16)
{
	LOCK();
	if (rawn < 64) { rawq[(rawh + rawn) & 63] = flags | (uint32_t)x16 << 16; rawy[(rawh + rawn) & 63] = y16; rawn++; }
	if (b45.enabled && !b45.line) { b45.line = 1; b45.raises++; b45.raise_cyc = cpui386_get_cycle(dpc->cpu); dset_irq(dpic, PTR_IRQ, 1); }
	UNLOCK();
}
uint32_t ptrdev_read32(void)
{
	B45Ev e; uint32_t v = 0;
	LOCK();
	if (rawn) { v = rawq[rawh & 63]; ylatch = rawy[rawh & 63]; rawh++; rawn--; b45.popped++; UNLOCK(); return v; }
	if (b45_pop(&e)) {
		ylatch = e.y16; v = e.flags | (uint32_t)e.x16 << 16; b45.popped++;
		if (b45.log) b45.log(&e);
	} else if (b45.line) { b45.line = 0; dset_irq(dpic, PTR_IRQ, 0); }
	UNLOCK();
	return v;
}
uint16_t ptrdev_read16(void) { return ylatch; }

void ptrdev_write16(uint16_t v)
{
	LOCK();
	if (v == 1) { b45.enabled = 1; b45.enables++; kick_locked(); }
	else if (v == 0) { b45.enabled = 0; if (b45.line) { b45.line = 0; dset_irq(dpic, PTR_IRQ, 0); } }
	else if (v == 2) {
		long c = cpui386_get_cycle(dpc->cpu);
		b45.isr_n++; b45.isr_instr += c - b45.raise_cyc; b45.isr_recs_seen = b45.popped;
		if (b45.done) b45.done(c - b45.raise_cyc);
		kick_locked(); /* records queued while the ISR ran: the edge comes after its EOI */
	}
	UNLOCK();
}

/* replay start: fresh engine (same start state as the PC trace test), nothing queued */
void ptrdev_reset(void)
{
	LOCK();
	b45_init(0);
	if (b45.line) { b45.line = 0; dset_irq(dpic, PTR_IRQ, 0); }
	UNLOCK();
}
int ptrdev_enabled(void) { return b45.enabled; }

/* B61 snapshot: device state only. The Rust gesture engine is restarted (a snapshot is taken at an idle desktop: no touch in progress)
 * and told where Windows has the pointer; records still queued at snapshot time are dropped. */
#include "snap.h"
#include "gfxcard.h"
void ptrdev_snap(Snap *sn)
{
	sn_tag(sn, 0x50545231);
	int32_t en = b45.enabled, ln = b45.line; SN(sn, en); SN(sn, ln); SN(sn, ylatch);
	if (sn->rd && !sn->err) {
		LOCK();
		b45_init(0); rawn = 0; b45.host_pend = 0; b45.enabled = en; b45.line = ln;
		b45.popped = b45.isr_recs_seen = 0;
		if (gfx.w > 0 && gfx.h > 0) b45_sync((uint16_t)(gfx.cx * 1280 / gfx.w), (uint16_t)(gfx.cy * 800 / gfx.h));
		UNLOCK();
	}
}

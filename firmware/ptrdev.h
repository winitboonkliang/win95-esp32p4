/* BLUE B45 pointer device: see ptrdev.c */
#ifndef PTRDEV_H
#define PTRDEV_H
#include <stdint.h>
#define PTR_IRQ 10
typedef struct {
	volatile int enabled, line, host_pend;
	long raise_cyc, raises, enables, popped, isr_n, isr_instr, isr_recs_seen, dropped;
	void (*log)(const void *ev);   /* optional: every record the guest pops */
	void (*done)(long instr);      /* optional: every ISR end, guest instructions since the IRQ was raised */
} B45Dev;
extern B45Dev b45;

void ptrdev_init(void *pc, void *pic, void (*set_irq)(void *, int, int));
void ptrdev_touch(uint32_t t_us, int down, int rx, int ry);
void ptrdev_rel(uint32_t t_us, int dx, int dy, int b);
void ptrdev_tick(uint32_t t_us);
void ptrdev_set_abs(uint32_t t_us, int x, int y, int b);
int pointer_abs_consumed(void);
void ptrdev_sync(int x, int y);
uint32_t ptrdev_overlay(uint32_t *seq);
void ptrdev_poll(void);
void ptrdev_reset(void);
uint32_t ptrdev_read32(void);
uint16_t ptrdev_read16(void);
void ptrdev_write16(uint16_t v);
#endif

/* B85: Doom player experience (controls, touch overlay, saves/settings, fps + latency metrics). Doom boot only. */
#pragma once
#include <stdint.h>

int b85_solo(void);                 /* b82doom.c: 1 in the Doom boot */
void b82_post(int down, int key);   /* b82doom.c key ring -> DG_GetKey */

/* b85pad.c: USB */
int b85_mouse(int dx, int dy, int btn);   /* 1 = consumed (Doom boot) */
void b85_dev(int kind, int d);            /* kind 1 keyboard, 2 mouse; d +1 in / -1 out */
void b85_pad_start(void);                 /* XInput client task (retries until the USB host is installed) */
void b85_rumble(int big, int small);

/* b85.c: engine side */
void b85_engine_init(const char *wad);   /* doom task, before doomgeneric_Create */
void b85_key_mark(void);                 /* a key went down (latency start) */
int b85_key(int down, int lc);           /* 1 = taken by B85 (fps toggle) */
int b85_touch(int n, const int *x, const int *y, const int *id);
void b85_overlay(uint16_t *stage, int w, int h);   /* after the 8->565 convert, before the msync */
void b85_frame_built(int slot);          /* DG_DrawFrame: the frame in this staging slot carries this tic's input time */
int64_t b85_take_in(int slot);           /* present task, when it dequeues the slot */
void b85_frame_shown(int64_t tin);       /* present task: right after lcd_show / lcd_swap_present */
int b85_console(const char *l);          /* 1 = handled */
void b85_quit(void);                     /* every way out of the Doom boot */

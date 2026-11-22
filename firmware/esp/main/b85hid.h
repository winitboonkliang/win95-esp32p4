/* B85: gamepad report decoding (b85hid.c). Pure C. */
#pragma once
#include <stdint.h>

/* logical buttons (Xbox layout) = bit numbers of b85pad_t.btn; 0..11 go to the engine as joystick buttons (joyb_* in default.cfg) */
enum { B85B_A, B85B_B, B85B_X, B85B_Y, B85B_LB, B85B_RB, B85B_BACK, B85B_START, B85B_LS, B85B_RS, B85B_LT, B85B_RT,
       B85B_UP, B85B_DOWN, B85B_LEFT, B85B_RIGHT, B85B_EXTRA };
typedef struct { uint32_t btn; int8_t lx, ly, rx, ry; uint8_t lt, rt; } b85pad_t;   /* sticks -127..127, y down-positive */

enum { B85F_X, B85F_Y, B85F_Z, B85F_RX, B85F_RY, B85F_RZ, B85F_HAT, B85F_ACC, B85F_BRAKE, B85F_BTN0, B85F_N = B85F_BTN0 + 32 };
#define B85_MAXRID 8
typedef struct { uint16_t off; uint8_t size, sgn; int32_t lmin, lmax; } b85field_t;
typedef struct { b85field_t f[B85_MAXRID][B85F_N]; uint8_t used[B85_MAXRID], nbtn[B85_MAXRID]; uint8_t has_id, ok; } b85hid_t;

int b85hid_parse(const uint8_t *desc, int len, b85hid_t *h);   /* 1 = looks like a gamepad */
int b85hid_decode(const b85hid_t *h, const uint8_t *rep, int len, b85pad_t *p);   /* 1 = p filled */
int b85x360_decode(const uint8_t *rep, int len, b85pad_t *p);

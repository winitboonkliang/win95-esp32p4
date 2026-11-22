/* B85: generic HID gamepad = report descriptor parse (no per-pad tables). Pure C, no IDF: tools/b85/test_hid.c runs it on the PC.
 * Finds per report ID: X Y Z Rx Ry Rz (+ Accelerator/Brake as triggers), the hat, buttons 1..32; decodes a report into a b85pad. */
#include <string.h>
#include "b85hid.h"

static void trig_btn(b85pad_t *p);
static uint32_t uval(const uint8_t *p, int n) { uint32_t v = 0; for (int i = 0; i < n; i++) v |= (uint32_t)p[i] << (8 * i); return v; }
static int32_t sval(const uint8_t *p, int n) { uint32_t v = uval(p, n); int b = 8 * n; return b < 32 && (v >> (b - 1)) & 1 ? (int32_t)(v | ~0u << b) : (int32_t)v; }

/* slot for a usage, -1 = not wanted */
static int slot(uint32_t page, uint32_t u)
{
	if (page == 1 && u >= 0x30 && u <= 0x35) return B85F_X + (int)(u - 0x30);
	if (page == 1 && u == 0x39) return B85F_HAT;
	if (page == 2 && u == 0xC4) return B85F_ACC;
	if (page == 2 && u == 0xC5) return B85F_BRAKE;
	if (page == 9 && u >= 1 && u <= 32) return B85F_BTN0 + (int)(u - 1);
	return -1;
}

int b85hid_parse(const uint8_t *d, int len, b85hid_t *h)
{
	memset(h, 0, sizeof *h);
	uint32_t page = 0, rsize = 0, rcount = 0, rid = 0; int32_t lmin = 0, lmax = 0;
	uint32_t use[16]; int nuse = 0; uint32_t umin = 0, umax = 0; int have_range = 0;
	uint32_t off[B85_MAXRID]; memset(off, 0, sizeof off);
	int found = 0;
	for (int i = 0; i < len;) {
		uint8_t p = d[i];
		if (p == 0xFE) { if (i + 1 >= len) break; i += 3 + d[i + 1]; continue; }   /* long item: skip */
		int n = (p & 3) == 3 ? 4 : (p & 3), type = (p >> 2) & 3, tag = p >> 4;
		if (i + 1 + n > len) break;
		const uint8_t *a = d + i + 1; i += 1 + n;
		if (type == 1) {   /* global */
			if (tag == 0) page = uval(a, n);
			else if (tag == 1) lmin = sval(a, n);
			else if (tag == 2) lmax = n ? (lmin < 0 ? sval(a, n) : (int32_t)uval(a, n)) : 0;
			else if (tag == 7) rsize = uval(a, n);
			else if (tag == 8) { rid = uval(a, n); h->has_id = 1; }
			else if (tag == 9) rcount = uval(a, n);
		} else if (type == 2) {   /* local */
			uint32_t v = uval(a, n);
			if (tag == 0 && nuse < 16) use[nuse++] = v;
			else if (tag == 1) { umin = v; have_range |= 1; }
			else if (tag == 2) { umax = v; have_range |= 2; }
		} else if (type == 0) {   /* main */
			if (tag == 8 && rid < B85_MAXRID) {   /* Input */
				uint32_t flags = n ? uval(a, n) : 0;
				for (uint32_t k = 0; k < rcount; k++) {
					uint32_t u, pg = page;
					if (have_range == 3) { u = umin + k; if (u > umax) u = umax; }
					else if (nuse) u = use[k < (uint32_t)nuse ? k : (uint32_t)nuse - 1];
					else u = 0;
					if (u >> 16) { pg = u >> 16; u &= 0xFFFF; }
					int s = !(flags & 1) ? slot(pg, u) : -1;   /* constant = padding */
					if (s >= 0 && rsize && rsize <= 32 && !h->f[rid][s].size) {
						b85field_t *f = &h->f[rid][s]; f->off = off[rid]; f->size = (uint8_t)rsize; f->lmin = lmin; f->lmax = lmax;
						f->sgn = lmin < 0; found |= 1 << (s >= B85F_BTN0 ? B85F_BTN0 : s);
						if (s >= B85F_BTN0 && s - B85F_BTN0 + 1 > h->nbtn[rid]) h->nbtn[rid] = (uint8_t)(s - B85F_BTN0 + 1);
					}
					off[rid] += rsize;
				}
				h->used[rid] = 1;
			} else if (tag == 9 || tag == 0xB) { if (rid < B85_MAXRID) off[rid] += rsize * rcount; }   /* Output / Feature: own layout, not ours */
			nuse = 0; have_range = 0;
		}
	}
	/* a pad = two axes or a hat, plus buttons */
	h->ok = (found & (1 << B85F_BTN0)) && ((found & 3) == 3 || (found & (1 << B85F_HAT)));
	return h->ok;
}

static uint32_t bits(const uint8_t *r, int len, uint32_t off, int size)
{
	uint32_t v = 0;
	for (int i = 0; i < size; i++) { uint32_t b = off + i; if ((int)(b >> 3) < len && (r[b >> 3] >> (b & 7)) & 1) v |= 1u << i; }
	return v;
}
static int fval(const b85field_t *f, const uint8_t *r, int len, int32_t *out)
{
	if (!f->size) return 0;
	uint32_t v = bits(r, len, f->off, f->size);
	*out = f->sgn && f->size < 32 && (v >> (f->size - 1)) & 1 ? (int32_t)(v | ~0u << f->size) : (int32_t)v;
	return 1;
}
/* logical range -> -127..127 (axes) */
static int axis(const b85field_t *f, const uint8_t *r, int len)
{
	int32_t v; if (!fval(f, r, len, &v)) return 0;
	int32_t lo = f->lmin, hi = f->lmax; if (hi <= lo) { lo = f->sgn ? -(1 << (f->size - 1)) : 0; hi = f->sgn ? (1 << (f->size - 1)) - 1 : (1 << f->size) - 1; }
	int64_t c = (int64_t)(v - lo) * 254 / (hi - lo) - 127;
	return c < -127 ? -127 : c > 127 ? 127 : (int)c;
}
static int trig(const b85field_t *f, const uint8_t *r, int len) { int a = axis(f, r, len); return f->size ? (a + 127) * 255 / 254 : 0; }

/* DirectInput button numbers (F310 D mode, DualShock, most generic pads) -> our logical Xbox layout */
static const uint8_t dinput_map[12] = { B85B_X, B85B_A, B85B_B, B85B_Y, B85B_LB, B85B_RB, B85B_LT, B85B_RT, B85B_BACK, B85B_START, B85B_LS, B85B_RS };

int b85hid_decode(const b85hid_t *h, const uint8_t *r, int len, b85pad_t *p)
{
	uint32_t rid = 0;
	if (h->has_id) { if (len < 1) return 0; rid = r[0]; r++; len--; }
	if (rid >= B85_MAXRID || !h->used[rid]) return 0;
	const b85field_t *f = h->f[rid];
	if (!f[B85F_X].size && !f[B85F_HAT].size && !h->nbtn[rid]) return 0;   /* another report of the device */
	memset(p, 0, sizeof *p);
	p->lx = axis(&f[B85F_X], r, len); p->ly = axis(&f[B85F_Y], r, len);
	if (f[B85F_Z].size && f[B85F_RZ].size) {   /* DInput: right stick = Z/Rz, Rx/Ry (DualShock 4) = analog triggers */
		p->rx = axis(&f[B85F_Z], r, len); p->ry = axis(&f[B85F_RZ], r, len);
		p->lt = trig(&f[B85F_RX], r, len); p->rt = trig(&f[B85F_RY], r, len);
	} else {   /* Rx/Ry right stick, Z = one shared trigger axis (old Xbox-HID style) */
		p->rx = axis(&f[B85F_RX], r, len); p->ry = axis(&f[B85F_RY], r, len);
		if (f[B85F_Z].size) { int z = axis(&f[B85F_Z], r, len); p->lt = z < 0 ? -z * 2 : 0; p->rt = z > 0 ? z * 2 : 0; }
	}
	if (f[B85F_ACC].size) p->rt = trig(&f[B85F_ACC], r, len);
	if (f[B85F_BRAKE].size) p->lt = trig(&f[B85F_BRAKE], r, len);
	trig_btn(p);
	int32_t hv;
	if (fval(&f[B85F_HAT], r, len, &hv)) {   /* 8-way hat; out of the logical range = centered */
		int32_t d8 = hv - f[B85F_HAT].lmin;
		if (d8 >= 0 && d8 < 8 && hv <= f[B85F_HAT].lmax) {
			static const uint8_t dir[8] = { 1, 1 | 8, 8, 2 | 8, 2, 2 | 4, 4, 1 | 4 };   /* N NE E SE S SW W NW -> up 1 down 2 left 4 right 8 */
			uint8_t m = dir[d8];
			p->btn |= (m & 1 ? 1u << B85B_UP : 0) | (m & 2 ? 1u << B85B_DOWN : 0) | (m & 4 ? 1u << B85B_LEFT : 0) | (m & 8 ? 1u << B85B_RIGHT : 0);
		}
	}
	for (int i = 0; i < h->nbtn[rid] && i < 32; i++) {
		int32_t b; if (!fval(&f[B85F_BTN0 + i], r, len, &b) || !b) continue;
		p->btn |= 1u << (i < 12 ? dinput_map[i] : B85B_EXTRA);
	}
	return 1;
}

static int8_t c127(int v) { return (int8_t)(v < -127 ? -127 : v > 127 ? 127 : v); }
static void trig_btn(b85pad_t *p) { if (p->lt > 76) { p->btn |= 1u << B85B_LT; } if (p->rt > 76) { p->btn |= 1u << B85B_RT; } }
/* Xbox 360 wired (XInput, also F310 X mode): 20-byte report type 0x00 */
int b85x360_decode(const uint8_t *r, int len, b85pad_t *p)
{
	if (len < 14 || r[0] != 0x00 || r[1] < 14) return 0;
	static const uint8_t m2[8] = { B85B_UP, B85B_DOWN, B85B_LEFT, B85B_RIGHT, B85B_START, B85B_BACK, B85B_LS, B85B_RS };
	static const uint8_t m3[8] = { B85B_LB, B85B_RB, B85B_EXTRA, 255, B85B_A, B85B_B, B85B_X, B85B_Y };
	memset(p, 0, sizeof *p);
	for (int i = 0; i < 8; i++) { if (r[2] >> i & 1) p->btn |= 1u << m2[i]; if ((r[3] >> i & 1) && m3[i] != 255) p->btn |= 1u << m3[i]; }
	p->lt = r[4]; p->rt = r[5];
	int16_t lx = (int16_t)(r[6] | r[7] << 8), ly = (int16_t)(r[8] | r[9] << 8), rx = (int16_t)(r[10] | r[11] << 8), ry = (int16_t)(r[12] | r[13] << 8);
	p->lx = c127(lx >> 8); p->ly = c127(-(ly >> 8)); p->rx = c127(rx >> 8); p->ry = c127(-(ry >> 8));   /* XInput Y is up-positive; ours (HID) is down-positive */
	trig_btn(p);
	return 1;
}

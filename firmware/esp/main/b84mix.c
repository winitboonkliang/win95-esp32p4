/* B84: Doom sound mixer core (portable C, see b84mix.h). Per sample, mono (the board has one speaker):
 *   voices: DMX 8-bit, linear interpolation 11025/22050 -> 44100, per-voice gain ramps (B84_RAMP) at start, stop, volume change and
 *           channel restart (the old sound becomes a fading ghost) - the old port stepped at all four (clicks);
 *   + OPL music (stereo -> mono, x8: Nuked peaks at ~8 % FS, old-port balance);
 *   B49 chain: missing-fundamental bass (|LP150| = 2nd harmonic of what the speaker cannot play), HP 150 Hz 4th order,
 *   speaker EQ (B49 measured inverse: +4 dB 350 Hz, -6 dB 1.3 kHz, +4 dB 3.8 kHz; milder than B49's full curve), LP 7 kHz
 *   (linear-interp images + harsh highs), peak limiter at -3 dBFS (instant attack, 80 ms release), master fade (start / quit). */
#include <math.h>
#include <string.h>
#include "b84mix.h"

static void bq_set(b84_bq_t *q, int type, float f, float Q, float db)   /* RBJ cookbook; type 0 LP, 1 HP, 2 peak */
{
	float w = 2.0f * (float)M_PI * f / B84_SR, c = cosf(w), al = sinf(w) / (2 * Q), A = powf(10, db / 40), a0;
	memset(q, 0, sizeof *q);
	if (type == 2) { a0 = 1 + al / A; q->b0 = (1 + al * A) / a0; q->b1 = -2 * c / a0; q->b2 = (1 - al * A) / a0; q->a2 = (1 - al / A) / a0; }
	else { a0 = 1 + al; float k = type ? (1 + c) / 2 : (1 - c) / 2; q->b0 = k / a0; q->b1 = (type ? -2 * k : 2 * k) / a0; q->b2 = k / a0; q->a2 = (1 - al) / a0; }
	q->a1 = -2 * c / a0;
}
static inline float bq(b84_bq_t *q, float x)
{
	float y = q->b0 * x + q->z1;
	q->z1 = q->b1 * x - q->a1 * y + q->z2; q->z2 = q->b2 * x - q->a2 * y;
	return y;
}

void b84_init(b84_mix_t *m, int eq)
{
	memset(m, 0, sizeof *m);
	m->eq = eq;
	bq_set(&m->hp[0], 1, 150, 0.5412f, 0); bq_set(&m->hp[1], 1, 150, 1.3066f, 0);   /* Butterworth 4th order */
	bq_set(&m->lpb, 0, 150, 0.7071f, 0); bq_set(&m->lp, 0, 7000, 0.7071f, 0);
	bq_set(&m->pk[0], 2, 350, 1.2f, 4); bq_set(&m->pk[1], 2, 1300, 1.0f, -6); bq_set(&m->pk[2], 2, 3800, 1.4f, 4);
	m->thr = 0.708f; m->rel = expf(-1.0f / (0.080f * B84_SR)); m->env = 0;
	m->mus_gain = 4.0f;   /* fmopl OPL2 mono: ~6 dB hotter than Nuked (host, same voice) - keeps the old music/SFX balance (Nuked x8) */ m->bass_gain = 0.5f; m->boost = 1;
	m->master = 0; m->mtg = 0;
}

void b84_fade(b84_mix_t *m, float to, int n)
{
	if (n < 1) n = 1;
	m->mtg = to; m->mdg = (to - m->master) / n; m->mn = n;
}

void b84_sfx_set(b84_sfx_t *s, const uint8_t *raw, int lumplen)
{
	s->len = 0;
	if (!raw || lumplen < 8) return;
	unsigned fmt = raw[0] | raw[1] << 8, rate = raw[2] | raw[3] << 8;
	uint32_t n = raw[4] | raw[5] << 8 | raw[6] << 16 | (uint32_t)raw[7] << 24;
	if (fmt != 3 || rate < 4000 || n > (uint32_t)lumplen - 8) return;
	if (n >= 48) { s->pcm = raw + 8 + 16; s->len = n - 32; }   /* DMX: 16 pad bytes each side */
	else { s->pcm = raw + 8; s->len = n; }
	s->step = (uint32_t)(((uint64_t)rate << 16) / B84_SR);
}

static void ramp(b84_voice_t *v, float t) { v->tg = t; v->dg = (t - v->g) / B84_RAMP; v->n = B84_RAMP; }
static float vgain(int vol) { if (vol < 0) vol = 0; if (vol > 127) vol = 127; return vol * (1.0f / (127 * 128)); }

static void ghost(b84_mix_t *m, int ch)
{
	for (int i = 0; i < B84_NV; i++) { b84_voice_t *v = &m->v[i]; if (v->on && v->ch == ch) { v->ch = -1; v->stop = 1; ramp(v, 0); } }
}

int b84_start(b84_mix_t *m, int ch, const b84_sfx_t *s, int vol)
{
	if (ch < 0 || ch >= B84_NCH || !s || !s->len) return -1;
	ghost(m, ch);
	b84_voice_t *f = 0;
	for (int i = 0; i < B84_NV && !f; i++) if (!m->v[i].on) f = &m->v[i];
	if (!f) {   /* all busy: take the quietest ghost */
		for (int i = 0; i < B84_NV; i++) if (m->v[i].ch < 0 && (!f || m->v[i].g < f->g)) f = &m->v[i];
		m->steals++;
		if (!f) return -1;
	}
	f->s = s; f->pos = 0; f->g = 0; f->ch = (int8_t)ch; f->stop = 0; ramp(f, vgain(vol)); f->on = 1;
	m->starts++;
	return ch;
}

void b84_param(b84_mix_t *m, int ch, int vol)
{
	for (int i = 0; i < B84_NV; i++) { b84_voice_t *v = &m->v[i]; if (v->on && v->ch == ch && !v->stop) ramp(v, vgain(vol)); }
}
void b84_stop(b84_mix_t *m, int ch) { if (ch >= 0) ghost(m, ch); }
int b84_playing(const b84_mix_t *m, int ch)
{
	for (int i = 0; i < B84_NV; i++) if (m->v[i].on && m->v[i].ch == ch) return 1;
	return 0;
}
int b84_active(const b84_mix_t *m)
{
	for (int i = 0; i < B84_NV; i++) if (m->v[i].on) return 1;
	return 0;
}

void b84_voices(b84_mix_t *m, float *acc)
{
	memset(acc, 0, B84_BLK * sizeof *acc);
	for (int k = 0; k < B84_NV; k++) {
		b84_voice_t *v = &m->v[k];
		if (!v->on) continue;
		const uint8_t *p = v->s->pcm; uint32_t len = v->s->len, pos = v->pos, st = v->s->step;
		float g = v->g, dg = v->dg; int n = v->n;
		for (int i = 0; i < B84_BLK; i++) {
			uint32_t ix = pos >> 16;
			if (ix >= len) { v->on = 0; break; }
			float a = (float)p[ix] - 128, b = ix + 1 < len ? (float)p[ix + 1] - 128 : 0;   /* the last sample glides to 0: soft natural end */
			if (n) { g += dg; if (!--n) g = v->tg; }
			acc[i] += (a + (b - a) * (float)(pos & 0xffff) * (1.0f / 65536)) * g;
			pos += st;
		}
		v->pos = pos; v->g = g; v->n = n;
		if (v->stop && !n) v->on = 0;   /* ghost faded out */
	}
}

void b84_finish(b84_mix_t *m, float *acc, const int16_t *mus, int16_t *out)
{
	const float mk = 0.5f / 32768 * m->mus_gain;
	for (int i = 0; i < B84_BLK; i++) {
		float x = acc[i];
		if (mus) x += ((float)mus[2 * i] + (float)mus[2 * i + 1]) * mk;
		x *= m->boost;
		if (m->eq) {
			x += m->bass_gain * fabsf(bq(&m->lpb, x));   /* rectified bass = its 2nd harmonic (DC goes in the HP below) */
			x = bq(&m->hp[1], bq(&m->hp[0], x));
			x = bq(&m->lp, bq(&m->pk[2], bq(&m->pk[1], bq(&m->pk[0], x))));
		}
		float a = fabsf(x);
		float d = m->env * m->rel;
		m->env = a > d ? a : d;   /* decay first, then the peak: env >= |x| always (host self-check caught the other order) */
		if (m->env > m->thr) x *= m->thr / m->env;
		if (m->mn) { m->master += m->mdg; if (!--m->mn) m->master = m->mtg; }
		x *= m->master;
		int s = (int)lrintf(x * 32767);
		if (s > 32767) s = 32767; else if (s < -32768) s = -32768;
		out[2 * i] = out[2 * i + 1] = (int16_t)s;
	}
}

/* B84: Doom sound mixer core - portable C (board: esp/main/b84snd.c; host A/B test: tools/b84/mixtest.c). No ESP headers here. */
#pragma once
#include <stdint.h>

#define B84_SR   44100
#define B84_BLK  128      /* frames per block = 2.9 ms */
#define B84_NCH  16       /* engine channels (snd_channels <= 16) */
#define B84_NV   24       /* voices: 16 + 8 ghosts (a restarted channel fades its old sound out instead of cutting it) */
#define B84_RAMP 64       /* samples per gain ramp (1.45 ms): starts, stops, volume changes - no step anywhere */

typedef struct { const uint8_t *pcm; uint32_t len; uint32_t step; } b84_sfx_t;   /* DMX 8-bit unsigned; step = 16.16 source per output sample */
typedef struct { const b84_sfx_t *s; uint32_t pos; float g, tg, dg; int n; int8_t ch; uint8_t on, stop; } b84_voice_t;
typedef struct { float b0, b1, b2, a1, a2, z1, z2; } b84_bq_t;

typedef struct {
	b84_voice_t v[B84_NV];
	b84_bq_t hp[2], lpb, lp, pk[3];   /* HP 150 Hz (4th order), bass LP 150 Hz (enhancer), LP 7 kHz, speaker EQ (B49 measured curve) */
	float env, rel, thr;              /* soft limiter */
	float master, mtg, mdg; int mn;   /* master fade (start / quit) */
	float mus_gain, bass_gain, boost;   /* boost: safety test only (doomloud) - limiter pinned at its ceiling */
	int eq;                           /* 0 = flat (old-port A/B), 1 = B84 chain */
	uint32_t starts, steals;
} b84_mix_t;

void b84_init(b84_mix_t *m, int eq);
void b84_sfx_set(b84_sfx_t *s, const uint8_t *lump, int lumplen);   /* returns s->len 0 if not a DMX lump */
int  b84_start(b84_mix_t *m, int ch, const b84_sfx_t *s, int vol);  /* vol 0..127 */
void b84_param(b84_mix_t *m, int ch, int vol);
void b84_stop(b84_mix_t *m, int ch);
int  b84_playing(const b84_mix_t *m, int ch);
int  b84_active(const b84_mix_t *m);
void b84_fade(b84_mix_t *m, float to, int samples);
/* one block in two steps: voices -> acc (the caller holds its lock: voice state is shared with the engine), then music + EQ +
 * limiter + master fade -> out (no lock). mus = stereo OPL s16 (B84_BLK frames) or 0; out = stereo s16 */
void b84_voices(b84_mix_t *m, float *acc);
void b84_finish(b84_mix_t *m, float *acc, const int16_t *mus, int16_t *out);

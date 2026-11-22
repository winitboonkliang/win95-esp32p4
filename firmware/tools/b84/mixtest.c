/* B84 host A/B: old Doom_P4_HD mixer (main/doom_sound.cpp loop: 22050 Hz, nearest sample, no ramps, hard clip) vs the B84 mixer
 * (esp/main/b84mix.c, the same file the board builds) on one scripted SFX sequence from a real WAD.
 *   gcc -O2 -I../../esp/main mixtest.c ../../esp/main/b84mix.c -lm -o mixtest && ./mixtest doom1.wad out/
 * Writes out/A_old.wav (22050), out/B_b84.wav (44100), out/B_flat.wav (B84 without EQ), out/events.csv; analyse with clicks.py.
 * Self-checks (assert): B output never exceeds the limiter ceiling; every voice gain step is <= 1/B84_RAMP of full. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "b84mix.h"

#define SECS 30
#define NCH 8
static uint8_t *wad; static int nsfx; static b84_sfx_t sfx[200];
typedef struct { int t_ms, kind, ch, s, vol; } ev_t;   /* kind 0 start 1 stop 2 volume */
static ev_t ev[4000]; static int nev;

static void wav(const char *fn, const int16_t *x, int n, int sr)
{
	FILE *f = fopen(fn, "wb"); uint32_t d = n * 2, r = 36 + d, br = sr * 2; uint16_t one = 1, bits = 16, ba = 2; uint32_t fmt = 16, srr = sr;
	fwrite("RIFF", 1, 4, f); fwrite(&r, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmt, 4, 1, f); fwrite(&one, 2, 1, f); fwrite(&one, 2, 1, f);
	fwrite(&srr, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&d, 4, 1, f);
	fwrite(x, 2, n, f); fclose(f);
}

/* ---- the jump an event itself puts into the waveform: render the event's channel alone from the mixer state just before the event,
 * once with the event and once without; d = with - without; jump = |d| at the first output instant (B: two 44.1k samples = one 22.05k
 * interval). Old: a cut/start/volume change is a step of the whole sample value; B84: a 1/64 ramp step. dBFS (full scale = 1.0). */
static double jumps[2][3][4000]; static int njump[2][3];
static void put_jump(int who, int kind, double j) { if (j > 0) jumps[who][kind][njump[who][kind]++] = 20 * log10(j); }

/* ---- A: the old port's mixer, verbatim logic */
typedef struct { const uint8_t *pcm; uint32_t len, step16, pos16; int vol, active; } och_t;
static void old_apply(och_t *c, const ev_t *v)
{
	och_t *h = &c[v->ch];
	if (v->kind == 0) { h->pcm = sfx[v->s].pcm; h->len = sfx[v->s].len; h->step16 = (uint32_t)(((uint64_t)sfx[v->s].step * 44100) / 22050); h->pos16 = 0; h->vol = v->vol; h->active = 1; }
	else if (v->kind == 1) h->active = 0;
	else h->vol = v->vol;
}
static double old_first(const och_t *h) { uint32_t ix = h->pos16 >> 16; return h->active && ix < h->len ? ((int)h->pcm[ix] - 128) * h->vol * 2 / 32768.0 : 0; }
static void old_probe(const och_t *c, const ev_t *v)
{
	och_t a = c[v->ch], b = c[v->ch], tmp[16]; tmp[v->ch] = a; old_apply(tmp, v); a = tmp[v->ch];
	put_jump(0, v->kind, fabs(old_first(&a) - old_first(&b)));
}
static void run_old(const char *fn)
{
	const int SR = 22050, CH = 512; int n = SECS * SR; int16_t *out = calloc(n, 2); och_t c[16] = { 0 }; int e = 0;
	for (int base = 0; base < n; base += CH) {
		while (e < nev && (int64_t)ev[e].t_ms * SR / 1000 < base + CH) {   /* old port: events land at chunk granularity (23 ms) */
			ev_t *v = &ev[e++]; old_probe(c, v); old_apply(c, v);
		}
		int acc[512] = { 0 };
		for (int k = 0; k < 16; k++) {
			och_t *h = &c[k]; if (!h->active) continue;
			for (int i = 0; i < CH; i++) { uint32_t ix = h->pos16 >> 16; if (ix >= h->len) { h->active = 0; break; } acc[i] += ((int)h->pcm[ix] - 128) * h->vol; h->pos16 += h->step16; }
		}
		for (int i = 0; i < CH && base + i < n; i++) { int v = acc[i] << 1; out[base + i] = v > 32767 ? 32767 : v < -32768 ? -32768 : v; }
	}
	wav(fn, out, n, SR); free(out);
}

/* ---- B: B84 mixer, events applied at block granularity (2.9 ms) */
static void b_apply(b84_mix_t *m, const ev_t *v)
{
	if (v->kind == 0) b84_start(m, v->ch, &sfx[v->s], v->vol); else if (v->kind == 1) b84_stop(m, v->ch); else b84_param(m, v->ch, v->vol);
}
static void b_probe(const b84_mix_t *m, const ev_t *v)
{
	static b84_mix_t a, b; float fa[B84_BLK], fb[B84_BLK];
	a = *m;
	for (int k = 0; k < B84_NV; k++) if (a.v[k].ch != v->ch) a.v[k].on = 0;   /* the channel alone (its ghosts are new: none yet) */
	b = a; b_apply(&a, v);
	b84_voices(&a, fa); b84_voices(&b, fb);
	put_jump(1, v->kind, fabs(fa[0] - fb[0]) + fabs(fa[1] - fb[1]));
}
static void run_b84(const char *fn, int eq)
{
	static b84_mix_t m; b84_init(&m, eq); m.master = m.mtg = 1;
	int n = SECS * B84_SR; int16_t *out = calloc(n, 2); int16_t st[2 * B84_BLK]; float acc[B84_BLK]; int e = 0;
	for (int base = 0; base + B84_BLK <= n; base += B84_BLK) {
		while (e < nev && (int64_t)ev[e].t_ms * B84_SR / 1000 < base + B84_BLK) {
			ev_t *v = &ev[e++]; if (eq) b_probe(&m, v); b_apply(&m, v);
		}
		for (int k = 0; k < B84_NV; k++) assert(!m.v[k].on || fabsf(m.v[k].dg) <= 1.0f / 128 / B84_RAMP + 1e-9f);
		b84_voices(&m, acc); b84_finish(&m, acc, NULL, st);
		for (int i = 0; i < B84_BLK; i++) { out[base + i] = st[2 * i]; if (abs(st[2 * i]) > (int)(0.708f * 32767) + 2) { fprintf(stderr, "over %d at %d env %f\n", st[2 * i], base + i, m.env); exit(1); } }
	}
	wav(fn, out, n, B84_SR); free(out);
	printf("%s: starts %u steals %u\n", fn, m.starts, m.steals);
}

int main(int argc, char **argv)
{
	if (argc < 3) { fprintf(stderr, "usage: mixtest <iwad> <outdir>\n"); return 2; }
	FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET); wad = malloc(sz); if (fread(wad, 1, sz, f) != (size_t)sz) return 1; fclose(f);
	int nl = *(int32_t *)(wad + 4), dir = *(int32_t *)(wad + 8);
	for (int i = 0; i < nl && nsfx < 200; i++) {
		uint8_t *d = wad + dir + 16 * i; int pos = *(int32_t *)d, len = *(int32_t *)(d + 4);
		if (d[8] == 'D' && d[9] == 'S') { b84_sfx_set(&sfx[nsfx], wad + pos, len); if (sfx[nsfx].len) nsfx++; }
	}
	printf("%d DMX sfx\n", nsfx);
	uint32_t r = 12345; char p[512];
	snprintf(p, sizeof p, "%s/events.csv", argv[2]); FILE *ec = fopen(p, "w"); fprintf(ec, "t_ms,kind,ch\n");
	for (int t = 200; t < SECS * 1000 - 500 && nev < 4000; ) {   /* a busy firefight: an event every 30-150 ms */
		r = r * 1103515245 + 12345; int k = (r >> 16) % 100; ev_t *v = &ev[nev++];
		r = r * 1103515245 + 12345; v->ch = (r >> 16) % NCH; r = r * 1103515245 + 12345; v->s = (r >> 16) % nsfx;
		r = r * 1103515245 + 12345; v->vol = 40 + (r >> 16) % 88; v->t_ms = t;
		v->kind = k < 60 ? 0 : k < 75 ? 1 : 2;
		fprintf(ec, "%d,%d,%d\n", t, v->kind, v->ch);
		r = r * 1103515245 + 12345; t += 30 + (r >> 16) % 120;
	}
	fclose(ec);
	snprintf(p, sizeof p, "%s/A_old.wav", argv[2]); run_old(p);
	snprintf(p, sizeof p, "%s/B_b84.wav", argv[2]); run_b84(p, 1);
	snprintf(p, sizeof p, "%s/B_flat.wav", argv[2]); run_b84(p, 0);
	printf("%d events, self-checks passed\n", nev);
	static const char *kn[3] = { "start", "stop", "volume" };
	printf("| event | n old | old jump median / p95 / max dBFS | n B84 | B84 jump median / p95 / max dBFS |\n|---|---|---|---|---|\n");
	for (int k = 0; k < 3; k++) {
		double st[2][3];
		for (int w = 0; w < 2; w++) {
			int n = njump[w][k]; double *j = jumps[w][k];
			for (int i = 1; i < n; i++) for (int q = i; q > 0 && j[q] < j[q - 1]; q--) { double t = j[q]; j[q] = j[q - 1]; j[q - 1] = t; }
			st[w][0] = n ? j[n / 2] : -999; st[w][1] = n ? j[n * 95 / 100] : -999; st[w][2] = n ? j[n - 1] : -999;
		}
		printf("| %s | %d | %.1f / %.1f / %.1f | %d | %.1f / %.1f / %.1f |\n", kn[k], njump[0][k], st[0][0], st[0][1], st[0][2], njump[1][k], st[1][0], st[1][1], st[1][2]);
	}
	return 0;
}

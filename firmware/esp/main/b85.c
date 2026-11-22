/* B85 (owner 21 Nov, lab:LEAD/B85/REPORT.md): the player side of the B82 Doom boot.
 *  - pad / mouse / touch -> engine events once per tic (doom_joy_get is called by I_GetEvent, i.e. once per built tic): ev_joystick
 *    with the button mask (joyb_* mapping in default.cfg, menus use bits 0/1 and joyb_menu_activate) + ev_mouse with every delta;
 *  - touch overlay drawn into the 640x400 RGB565 stage (PPA scales it with the frame), auto: shown only for 5 s after a touch (B87);
 *  - saves per WAD under /sdcard/DOOM/savegame/<wad>/, own settings in /sdcard/DOOM/b85.cfg, default.cfg written when a menu closes;
 *  - fps / frametime p50 p99 / input->photon (vsync-clock stamped) overlay + 5 s log lines.
 * The engine is not edited (B83 generates its Rust port from it): only its public symbols are used. State in PSRAM. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <dirent.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "doomkeys.h"
#include "doomstat.h"
#include "d_event.h"
#include "g_game.h"
#include "p_saveg.h"
#include "m_controls.h"
#include "b85.h"
#include "b85hid.h"
#include "m_config.h"   /* B83: M_SaveDefaults prototype before the b83api.h names */
#include "b83api.h"   /* B83: engine names -> the launch's engine (C or Rust); after the engine headers */

extern volatile b85pad_t b85_padst; extern volatile int64_t b85_pad_t, b85_mouse_t; extern volatile int32_t b85_mdx, b85_mdy, b85_mbtn;
extern volatile int b85_npad, b85_nkbd, b85_nmouse; extern volatile uint32_t b85_nrep; extern char b85_padname[];
extern volatile uint32_t lcd_refresh_n; extern volatile int64_t lcd_refresh_t;

/* ---- settings (/sdcard/DOOM/b85.cfg). detail/volumes/screen size/mouse sensitivity/joyb_* mapping live in the engine's default.cfg */
typedef struct { int touch, fps, sens, invy, dead, mousey, ver; } b85cfg_t;
static EXT_RAM_BSS_ATTR b85cfg_t cfg;
static const char *const cfgkey[] = { "touch_overlay", "fps_overlay", "pad_sensitivity", "pad_invert_y", "pad_deadzone", "mouse_move_y", "defaults_version" };
#define CFGFILE "/sdcard/DOOM/b85.cfg"
static void cfg_save(void)
{
	FILE *f = fopen(CFGFILE, "w"); if (!f) { fprintf(stderr, "[b85] %s: cannot write\r\n", CFGFILE); return; }
	const int *v = &cfg.touch;
	fprintf(f, "# B85 Doom settings. touch_overlay 0 off 1 auto (shown for 5 s after a touch) 2 always; pad_sensitivity 10 = 1.0x\n");
	for (int i = 0; i < 7; i++) fprintf(f, "%s %d\n", cfgkey[i], v[i]);
	fclose(f);
}
static void cfg_load(void)
{
	cfg = (b85cfg_t){ 1, 0, 10, 0, 9, 0, 0 };
	FILE *f = fopen(CFGFILE, "r"); if (!f) return;
	char ln[160], k[40]; int v;   /* line by line: the '#' header made a plain fscanf stop at once (nothing was ever read back, board B m2) */
	while (fgets(ln, sizeof ln, f)) if (ln[0] != '#' && sscanf(ln, "%39s %d", k, &v) == 2) for (int i = 0; i < 7; i++) if (!strcmp(k, cfgkey[i])) (&cfg.touch)[i] = v;
	fclose(f);
	cfg.sens = cfg.sens < 1 ? 1 : cfg.sens > 30 ? 30 : cfg.sens; cfg.dead = cfg.dead < 0 ? 0 : cfg.dead > 40 ? 40 : cfg.dead;
}

/* ---- latency + frame statistics */
#define NB 512   /* frametime bins of 0.25 ms (to 128 ms), latency bins of 1 ms (to 512 ms) */
static EXT_RAM_BSS_ATTR struct {
	uint16_t ft[NB], lat[NB]; uint32_t nft, nlat; int64_t last_show, t0;
	int64_t slot_in[4], pend_in; struct { int64_t tin; uint32_t r; } q[8]; uint8_t qn;
	uint64_t src_sum[4]; uint32_t src_n[4], src_max[4];   /* report -> tic: key, mouse, pad, touch */
	float fps, p50, p99, l50, l99; uint32_t frames; int64_t first_frame;
	uint32_t tot_ft[NB], tot_lat[NB];   /* whole session (quit summary) */
} st;
static float pct(const uint16_t *h, uint32_t n, float q, float bin)
{
	uint32_t want = (uint32_t)(n * q), c = 0;
	for (int i = 0; i < NB; i++) { c += h[i]; if (c > want) return (i + 0.5f) * bin; }
	return NB * bin;
}
static float pct32(const uint32_t *h, float q, float bin)
{
	uint64_t n = 0, c = 0; for (int i = 0; i < NB; i++) n += h[i];
	for (int i = 0; i < NB; i++) { c += h[i]; if (c > n * q) return (i + 0.5f) * bin; }
	return 0;
}
static EXT_RAM_BSS_ATTR int64_t kmark_t, touch_t, touch_seen;
static void sample(int src, int64_t t, int64_t now)
{
	if (now - t > 250000) return;   /* not a live input: a connect report or one taken while the engine was still loading */
	uint32_t us = (uint32_t)(now - t); st.src_sum[src] += us; st.src_n[src]++; if (us > st.src_max[src]) st.src_max[src] = us;
	if (!st.pend_in || t < st.pend_in) st.pend_in = t;
}
void b85_key_mark(void) { if (!kmark_t) kmark_t = esp_timer_get_time(); }

void b85_frame_built(int slot) { st.slot_in[slot & 3] = st.pend_in; st.pend_in = 0; }
int64_t b85_take_in(int slot) { int64_t t = st.slot_in[slot & 3]; st.slot_in[slot & 3] = 0; return t; }
static void resolve(void)
{
	/* the refresh after the one counted at show time scans the new buffer; photon = its start + half a refresh (mid panel) */
	uint32_t n = lcd_refresh_n; int64_t rt = lcd_refresh_t; int64_t T = 24000;   /* 48 MHz pclk = 41.6 Hz until the refresh clock says */
	{ static EXT_RAM_BSS_ATTR uint32_t n0; static EXT_RAM_BSS_ATTR int64_t t0; if (!t0) { n0 = n; t0 = rt; } else if (n - n0 > 40) T = (rt - t0) / (n - n0); }
	int k = 0;
	for (int i = 0; i < st.qn; i++) {
		if ((int32_t)(n - (st.q[i].r + 1)) < 0) { st.q[k++] = st.q[i]; continue; }
		int64_t vis = rt - (int64_t)(n - (st.q[i].r + 1)) * T + T / 2;
		uint32_t ms = (uint32_t)((vis - st.q[i].tin) / 1000); if (ms >= NB) ms = NB - 1;
		st.lat[ms]++; st.tot_lat[ms]++; st.nlat++;
	}
	st.qn = k;
}
void b85_frame_shown(int64_t tin)
{
	int64_t now = esp_timer_get_time();
	if (!st.first_frame) { st.first_frame = now; st.t0 = now; fprintf(stderr, "[b85] first frame %lld ms after boot\r\n", now / 1000); }
	resolve();
	if (tin && st.qn < 8) { st.q[st.qn].tin = tin; st.q[st.qn].r = lcd_refresh_n; st.qn++; }
	if (st.last_show) { uint32_t b = (uint32_t)((now - st.last_show) / 250); if (b >= NB) b = NB - 1; st.ft[b]++; st.tot_ft[b]++; st.nft++; }
	st.last_show = now; st.frames++;
	if (now - st.t0 >= 5000000 && st.nft) {
		st.fps = st.nft * 1e6f / (float)(now - st.t0); st.p50 = pct(st.ft, st.nft, .5f, .25f); st.p99 = pct(st.ft, st.nft, .99f, .25f);
		if (st.nlat) { st.l50 = pct(st.lat, st.nlat, .5f, 1); st.l99 = pct(st.lat, st.nlat, .99f, 1); }
		static const char *const sn[4] = { "key", "mouse", "pad", "touch" }; char s[160]; int o = 0;
		for (int i = 0; i < 4; i++) if (st.src_n[i]) o += snprintf(s + o, sizeof s - o, " %s %u/%u/%u", sn[i], (unsigned)st.src_n[i], (unsigned)(st.src_sum[i] / st.src_n[i] / 100), (unsigned)(st.src_max[i] / 100));
		fprintf(stderr, "[b85] fps %.1f ft p50 %.2f p99 %.2f ms | in->photon n %u p50 %.0f p99 %.0f ms | ->tic n/avg/max x0.1ms%s | PSRAM free %u internal free %u min %u\r\n",
			st.fps, st.p50, st.p99, (unsigned)st.nlat, st.l50, st.l99, o ? s : " -", (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
			(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
		memset(st.ft, 0, sizeof st.ft); memset(st.lat, 0, sizeof st.lat); st.nft = st.nlat = 0; st.t0 = now;
		memset(st.src_sum, 0, sizeof st.src_sum); memset(st.src_n, 0, sizeof st.src_n); memset(st.src_max, 0, sizeof st.src_max);
	}
}

/* ---- touch (gsl task, core 1): landscape 1280x800. Left = analog stick (move + strafe), right half = drag to turn, buttons */
enum { T_FIRE, T_USE, T_WPREV, T_WNEXT, T_MAP, T_MENU, T_FPS, T_N };
static const struct { int16_t x, y, r; const char *label; } tb[T_N] = {
	{ 1130, 620, 100, "FIRE" }, { 950, 715, 70, "USE" }, { 1010, 470, 52, "<" }, { 1190, 420, 52, ">" },
	{ 1200, 70, 56, "MAP" }, { 80, 70, 56, "MENU" }, { 640, 50, 40, "FPS" } };
#define SCX 230
#define SCY 590
#define SR 150
static EXT_RAM_BSS_ATTR volatile struct { int sx, sy, sid, lid, lx, held; int32_t look; uint32_t btn; uint8_t key[T_N]; } tch;
static int hit(int b, int x, int y) { int dx = x - tb[b].x, dy = y - tb[b].y, r = tb[b].r + 20; return dx * dx + dy * dy <= r * r; }
int b85_touch(int n, const int *x, const int *y, const int *id)
{
	if (!b85_solo()) return 0;
	int64_t now = esp_timer_get_time(); uint32_t want = 0; int sfound = 0, lfound = 0;
	if (n) touch_seen = now;
	for (int i = 0; i < n; i++) {
		int b = -1; for (int k = 0; k < T_N && b < 0; k++) if (hit(k, x[i], y[i])) b = k;
		if (b >= 0) { want |= 1u << b; continue; }
		if (x[i] < 560 && y[i] > 280 && (tch.sid < 0 || tch.sid == id[i]) && !sfound) {
			int dx = x[i] - SCX, dy = y[i] - SCY; float m = sqrtf((float)(dx * dx + dy * dy));
			if (m > SR) { dx = (int)(dx * SR / m); dy = (int)(dy * SR / m); }
			tch.sx = dx * 127 / SR; tch.sy = dy * 127 / SR; tch.sid = id[i]; sfound = 1;
		} else if (x[i] >= 600 && !lfound && (tch.lid < 0 || tch.lid == id[i])) {
			if (tch.lid == id[i]) tch.look += x[i] - tch.lx;
			tch.lid = id[i]; tch.lx = x[i]; lfound = 1;
		}
	}
	if (!sfound) { tch.sid = -1; tch.sx = tch.sy = 0; }
	if (!lfound) tch.lid = -1;
	for (uint32_t d = want ^ tch.held, b = 0; d; b++, d >>= 1) {
		if (!(d & 1)) continue;
		int down = (want >> b) & 1, k = 0;
		if (down) {
			if (b == T_FIRE) k = messageToPrint ? 'y' : menuactive ? KEY_ENTER : KEY_FIRE;
			else if (b == T_USE) k = messageToPrint ? 'n' : menuactive ? KEY_BACKSPACE : KEY_USE;
			else if (b == T_MAP) k = KEY_TAB;
			else if (b == T_MENU) k = KEY_ESCAPE;
			else if (b == T_FPS) { cfg.fps ^= 1; cfg_save(); }
			tch.key[b] = (uint8_t)k;
		} else k = tch.key[b];
		if (k) { b82_post(down, k); if (down && !touch_t) touch_t = now; }
	}
	tch.btn = (want >> T_WPREV & 1 ? 1u << B85B_LB : 0) | (want >> T_WNEXT & 1 ? 1u << B85B_RB : 0);
	if ((want & ~tch.held) || sfound || lfound) { if (!touch_t) touch_t = now; }
	tch.held = want;
	return 1;
}

/* ---- keyboard extras: ` (grave) toggles the fps overlay */
int b85_key(int down, int lc)
{
	if (lc == 41) { if (down) { cfg.fps ^= 1; cfg_save(); } return 1; }
	return 0;
}

/* ---- per tic (engine task) */
static EXT_RAM_BSS_ATTR char wadname[16], savedir[64];
static EXT_RAM_BSS_ATTR volatile int req_save, req_load, req_new;
static EXT_RAM_BSS_ATTR struct { int inited, last_active, menu_was, back, rumble_t, dmg; int32_t mdx, mdy, look; int mbtn; int64_t pad_t, mouse_t; } tk;
static int curve1(int v, int dead)   /* deadzone then signed square, -127..127 */
{
	int a = abs(v); if (a <= dead) return 0;
	float f = (a - dead) / (float)(127 - dead); if (f > 1) f = 1;
	int o = (int)(f * f * 127 + .5f); return v < 0 ? -o : o;
}
static void radial(int x, int y, int dead, int *ox, int *oy)   /* round deadzone on the move stick: diagonals are not slower */
{
	float m = sqrtf((float)(x * x + y * y)); if (m <= dead) { *ox = *oy = 0; return; }
	float f = (m - dead) / (127 - dead); if (f > 1) f = 1; f = f * f * 127 / m;
	*ox = (int)(x * f); *oy = (int)(y * f);
}
static int clamp127(int v) { return v < -127 ? -127 : v > 127 ? 127 : v; }
static void first_tic(void)
{
	/* per-WAD saves (doom1/doom/doom2 slots never collide) */
	mkdir("/sdcard/DOOM/savegame", 0755);
	snprintf(savedir, sizeof savedir, "/sdcard/DOOM/savegame/%s/", wadname); mkdir(savedir, 0755);
	savegamedir = savedir;
	if (cfg.ver < 1) {   /* once per card: twin-stick pad + modern mouse defaults, then remembered in default.cfg */
		joybfire = B85B_RT; joybuse = B85B_A; joybspeed = B85B_LT; joybstrafe = -1; joybprevweapon = B85B_LB; joybnextweapon = B85B_RB; joybmenu = B85B_START;
		mousebfire = 0; mousebuse = 1; mousebstrafe = -1; mousebforward = 2;
		cfg.ver = 1; cfg_save(); M_SaveDefaults();
	}
	fprintf(stderr, "[b85] saves %s, settings: touch %d fps %d sens %d invy %d dead %d mousey %d\r\n", savedir, cfg.touch, cfg.fps, cfg.sens, cfg.invy, cfg.dead, cfg.mousey);
}
void doom_joy_get(int *x, int *y, int *s)   /* i_input.c calls it once per I_GetEvent; we post our own events, it gets zeros */
{
	*x = *y = *s = 0;
	if (!tk.inited) { tk.inited = 1; first_tic(); }
	int64_t now = esp_timer_get_time();
	if (kmark_t) { sample(0, kmark_t, now); kmark_t = 0; }
	if (touch_t) { sample(3, touch_t, now); touch_t = 0; }
	if (b85_mouse_t != tk.mouse_t) { tk.mouse_t = b85_mouse_t; sample(1, tk.mouse_t, now); }
	if (b85_pad_t != tk.pad_t) { tk.pad_t = b85_pad_t; sample(2, tk.pad_t, now); }

	event_t ev;
	/* mouse + touch look: every delta since the last tic (producers only add to running totals) */
	int32_t mdx = b85_mdx, mdy = b85_mdy, lk = tch.look, mb = b85_mbtn;
	int dx = (mdx - tk.mdx) * 2 + (lk - tk.look) * 6, dy = cfg.mousey ? (mdy - tk.mdy) * 2 : 0;
	tk.mdx = mdx; tk.mdy = mdy; tk.look = lk;
	if (dx || dy || mb != tk.mbtn) { ev.type = ev_mouse; ev.data1 = mb & 7; ev.data2 = dx; ev.data3 = -dy; ev.data4 = 0; D_PostEvent(&ev); tk.mbtn = mb; }

	/* pad + touch stick -> one ev_joystick: data1 buttons, data2 turn, data3 forward (+ = back), data4 strafe */
	b85pad_t p; memcpy(&p, (const void *)&b85_padst, sizeof p);
	uint32_t b = p.btn | tch.btn;
	int hx = (b >> B85B_RIGHT & 1) * 127 - (b >> B85B_LEFT & 1) * 127, hy = (b >> B85B_DOWN & 1) * 127 - (b >> B85B_UP & 1) * 127;
	int dead = cfg.dead * 127 / 100, turn, fwd, str;
	if (menuactive) { turn = hx ? hx : p.lx; fwd = hy ? hy : p.ly + tch.sy; str = 0; }
	else {
		radial(p.lx, p.ly, dead, &str, &fwd);
		turn = curve1(p.rx, dead) * cfg.sens / 10 + hx; fwd += hy; if (cfg.invy) fwd = -fwd;
		fwd += tch.sy; str += tch.sx;
	}
	turn = clamp127(turn); fwd = clamp127(fwd); str = clamp127(str);
	int active = turn || fwd || str || (b & 0xFFF);
	if (active || tk.last_active) { ev.type = ev_joystick; ev.data1 = b & 0xFFF; ev.data2 = turn; ev.data3 = fwd; ev.data4 = str; D_PostEvent(&ev); }
	tk.last_active = active;
	/* Back = automap, Back + Y = fps overlay */
	int back = (p.btn >> B85B_BACK) & 1;
	if (back && (p.btn >> B85B_Y & 1) && !(tk.back & 2)) { cfg.fps ^= 1; cfg_save(); tk.back |= 2; }
	if (!(p.btn >> B85B_Y & 1)) tk.back &= ~2;
	if (back != (tk.back & 1)) { b82_post(back, KEY_TAB); tk.back = (tk.back & ~1) | back; }

	/* rumble on damage (XInput pads; others ignore it) */
	if (gamestate == GS_LEVEL) {
		int d = players[consoleplayer].damagecount;
		if (d > tk.dmg) { int a = (d - tk.dmg) * 24; b85_rumble(a > 255 ? 255 : a, 160); tk.rumble_t = 8; }
		else if (tk.rumble_t && !--tk.rumble_t) b85_rumble(0, 0);
		tk.dmg = d;
	}
	/* settings: written when a menu closes (detail, volumes, screen size, sensitivity: the engine's own default.cfg) */
	if (tk.menu_was && !menuactive) M_SaveDefaults();
	tk.menu_was = menuactive;
	if (req_save) { G_SaveGame(req_save - 1, "B85 SAVE"); req_save = 0; }
	if (req_load) { G_LoadGame(P_SaveGameFile(req_load - 1)); req_load = 0; }
	if (req_new) { G_DeferedInitNew(sk_medium, req_new / 100, req_new % 100); req_new = 0; }   /* console "doomnew E M": a real game (demos ignore input and saves) */
}

void b85_engine_init(const char *wad)
{
	const char *s = strrchr(wad, '/'); s = s ? s + 1 : wad;
	snprintf(wadname, sizeof wadname, "%s", s); char *dot = strchr(wadname, '.'); if (dot) *dot = 0;
	cfg_load(); tch.sid = tch.lid = -1;
	b85_pad_start();
}
void b85_quit(void)
{
	if (!b85_solo()) return;
	cfg_save();
	fprintf(stderr, "[b85] session: %u frames, ft p50 %.2f p99 %.2f ms, in->photon p50 %.0f p99 %.0f ms\r\n", (unsigned)st.frames,
		pct32(st.tot_ft, .5f, .25f), pct32(st.tot_ft, .99f, .25f), pct32(st.tot_lat, .5f, 1), pct32(st.tot_lat, .99f, 1));
}

/* ---- overlay drawing (640x400 stage; touch coordinates / 2) */
static const struct { char c; uint8_t r[7]; } font[] = {
	{'0',{14,17,19,21,25,17,14}},{'1',{4,12,4,4,4,4,14}},{'2',{14,17,1,2,4,8,31}},{'3',{31,2,4,2,1,17,14}},{'4',{2,6,10,18,31,2,2}},
	{'5',{31,16,30,1,1,17,14}},{'6',{6,8,16,30,17,17,14}},{'7',{31,1,2,4,8,8,8}},{'8',{14,17,17,14,17,17,14}},{'9',{14,17,17,15,1,2,12}},
	{'.',{0,0,0,0,0,12,12}},{':',{0,12,12,0,12,12,0}},{'<',{2,4,8,16,8,4,2}},{'>',{8,4,2,1,2,4,8}},{'/',{1,1,2,4,8,16,16}},{'-',{0,0,0,31,0,0,0}},
	{'A',{14,17,17,31,17,17,17}},{'D',{30,17,17,17,17,17,30}},{'E',{31,16,16,30,16,16,31}},{'F',{31,16,16,30,16,16,16}},{'I',{14,4,4,4,4,4,14}},
	{'L',{16,16,16,16,16,16,31}},{'M',{17,27,21,21,17,17,17}},{'N',{17,17,25,21,19,17,17}},{'P',{30,17,17,30,16,16,16}},{'R',{30,17,17,30,20,18,17}},
	{'S',{15,16,16,14,1,1,30}},{'T',{31,4,4,4,4,4,4}},{'U',{17,17,17,17,17,17,14}} };
static EXT_RAM_BSS_ATTR uint16_t *S; static EXT_RAM_BSS_ATTR int SW, SH;
static inline void px(int x, int y, uint16_t c, int a)   /* a: 1 = 25 %, 2 = 50 %, 4 = opaque */
{
	if ((unsigned)x >= (unsigned)SW || (unsigned)y >= (unsigned)SH) return;
	uint16_t *d = &S[y * SW + x];
	*d = a == 4 ? c : a == 2 ? ((*d & 0xF7DE) >> 1) + ((c & 0xF7DE) >> 1) : (uint16_t)(((*d >> 2) & 0x39E7) * 3 + ((c >> 2) & 0x39E7));
}
static void text(int x, int y, const char *s, int sc, uint16_t c, int a)
{
	for (; *s; s++, x += 6 * sc) {
		int g = -1; for (int i = 0; i < (int)(sizeof font / sizeof font[0]) && g < 0; i++) if (font[i].c == *s) g = i;
		if (g < 0) continue;
		for (int r = 0; r < 7; r++) for (int b = 0; b < 5; b++) if (font[g].r[r] >> (4 - b) & 1)
			for (int u = 0; u < sc; u++) for (int v = 0; v < sc; v++) px(x + b * sc + v, y + r * sc + u, c, a);
	}
}
static void disc(int cx, int cy, int r0, int r1, uint16_t c, int a)   /* ring r0..r1 (r0 = 0: filled), spans per row */
{
	for (int dy = -r1; dy <= r1; dy++) {
		int xo = (int)sqrtf((float)(r1 * r1 - dy * dy)), xi = r0 > abs(dy) ? (int)sqrtf((float)(r0 * r0 - dy * dy)) : -1;
		for (int dx = -xo; dx <= xo; dx++) if (dx < -xi || dx > xi) px(cx + dx, cy + dy, c, a);
	}
}
static void box(int x, int y, int w, int h, uint16_t c, int a) { for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) px(x + i, y + j, c, a); }
volatile int b85_ov_drawn;   /* B83 status-bar skip: anything drawn over the frame this time (touch overlay or fps box) */
void b85_overlay(uint16_t *stage, int w, int h)
{
	S = stage; SW = w; SH = h;
	int64_t now = esp_timer_get_time();
	int show = cfg.touch == 2 || (cfg.touch == 1 && touch_seen && now - touch_seen < 5000000);   /* B87 owner 22 Nov: auto = only for 5 s after a touch (clean picture in demos and pad play) */
	b85_ov_drawn = show;   /* the fps box sits in the top rows (always reconverted) */
	if (show) {
		const uint16_t W_ = 0xFFFF, R_ = 0xF800;
		disc(SCX / 2, SCY / 2, SR / 2 - 2, SR / 2, W_, 1);
		disc((SCX + tch.sx * SR / 127) / 2, (SCY + tch.sy * SR / 127) / 2, 0, 18, W_, tch.sid >= 0 ? 2 : 1);
		for (int k = 0; k < T_N; k++) {
			int cx = tb[k].x / 2, cy = tb[k].y / 2, r = tb[k].r / 2, on = tch.held >> k & 1;
			if (k == T_FPS && !cfg.fps && !on) { text(cx - 8, cy - 3, "FPS", 1, W_, 1); continue; }
			disc(cx, cy, r - 2, r, k == T_FIRE ? R_ : W_, on ? 2 : 1);
			if (on) disc(cx, cy, 0, r - 3, W_, 1);
			int len = (int)strlen(tb[k].label), sc = r > 30 ? 2 : 1;
			text(cx - len * 3 * sc + sc / 2, cy - 7 * sc / 2, tb[k].label, sc, W_, 2);
		}
	}
	if (cfg.fps) {
		char l1[32], l2[32], l3[32];
		snprintf(l1, sizeof l1, "%.1f FPS", st.fps); snprintf(l2, sizeof l2, "FT %.1f P99 %.1f", st.p50, st.p99);
		if (st.l50 > 0) snprintf(l3, sizeof l3, "LAT %.0f P99 %.0f MS", st.l50, st.l99); else l3[0] = 0;
		int y0 = show ? 110 : 8;   /* below the MENU button while the overlay is up */
		box(6, y0 - 2, 120, l3[0] ? 34 : 24, 0, 2);
		text(10, y0, l1, 1, 0x07E0, 4); text(10, y0 + 10, l2, 1, 0xFFFF, 4); if (l3[0]) text(10, y0 + 20, l3, 1, 0xFFE0, 4);
	}
}

/* ---- console: "doom st", "doom fps 0|1", "doom ov 0|1|2", "doom sens N", "doom invy 0|1", "doomsave N", "doomload N", "doom ls",
 * "doompad x360 <hex>" / "doompad f310 <hex>" (feed one report through the real decoder: pad tests without a pad) */
static const uint8_t f310d[] = {   /* Logitech F310 D mode report descriptor (tools/b85/test_hid.c) */
	0x05,0x01,0x09,0x04,0xA1,0x01,0xA1,0x02,0x75,0x08,0x95,0x04,0x15,0x00,0x26,0xFF,0x00,0x35,0x00,0x46,0xFF,0x00,
	0x09,0x30,0x09,0x31,0x09,0x32,0x09,0x35,0x81,0x02,0x75,0x04,0x95,0x01,0x25,0x07,0x46,0x3B,0x01,0x65,0x14,0x09,0x39,
	0x81,0x42,0x65,0x00,0x75,0x01,0x95,0x0C,0x25,0x01,0x45,0x01,0x05,0x09,0x19,0x01,0x29,0x0C,0x81,0x02,0x06,0x00,0xFF,
	0x75,0x01,0x95,0x10,0x25,0x01,0x45,0x01,0x09,0x01,0x81,0x02,0xC0,0xA1,0x02,0x75,0x08,0x95,0x07,0x46,0xFF,0x00,0x26,
	0xFF,0x00,0x09,0x02,0x91,0x02,0xC0,0xC0 };
static void touch_task(void *a)
{
	const int *t = a; int x = t[0], y = t[1], id = 1;
	for (int ms = 0; ms < t[2]; ms += 10) { b85_touch(1, &x, &y, &id); vTaskDelay(pdMS_TO_TICKS(10)); }
	b85_touch(0, &x, &y, &id); vTaskDelete(NULL);
}
int b85_console(const char *l)
{
	if (!b85_solo()) return 0;
	int v;
	if (sscanf(l, "doomsave %d", &v) == 1) { req_save = v + 1; return 1; }
	if (sscanf(l, "doomload %d", &v) == 1) { req_load = v + 1; return 1; }
	{ int e, m; if (sscanf(l, "doomnew %d %d", &e, &m) == 2) { req_new = e * 100 + m; return 1; } }
	if (sscanf(l, "doom fps %d", &v) == 1) { cfg.fps = v; cfg_save(); return 1; }
	if (sscanf(l, "doom ov %d", &v) == 1) { cfg.touch = v; cfg_save(); return 1; }
	if (sscanf(l, "doom sens %d", &v) == 1) { cfg.sens = v; cfg_save(); return 1; }
	if (sscanf(l, "doom invy %d", &v) == 1) { cfg.invy = v; cfg_save(); return 1; }
	if (sscanf(l, "doom padlog %d", &v) == 1) { extern volatile int b85_padlog; b85_padlog = v; return 1; }
	/* short forms (cardsvc keeps 39 chars a line): "doompx <14 report bytes>" = XInput, "doompf <8 bytes>" = F310 D mode */
	int px = !strncmp(l, "doompx ", 7), pf = !strncmp(l, "doompf ", 7);
	if (px || pf || !strncmp(l, "doompad ", 8)) {
		uint8_t r[32]; int n = 0; const char *h = px || pf ? l + 6 : strchr(l + 8, ' ');
		for (h = h ? h + 1 : ""; n < 32 && sscanf(h, "%2hhx", &r[n]) == 1; h += 2) n++;
		b85pad_t p; int ok;
		if (px || (!pf && !strncmp(l + 8, "x360", 4))) ok = b85x360_decode(r, n, &p);
		else { static EXT_RAM_BSS_ATTR b85hid_t d; ok = b85hid_parse(f310d, sizeof f310d, &d) && b85hid_decode(&d, r, n, &p); }
		if (ok) { memcpy((void *)&b85_padst, &p, sizeof p); b85_pad_t = esp_timer_get_time(); }
		fprintf(stderr, "[b85] doompad %d B ok %d: btn %05x lx %d ly %d rx %d ry %d lt %d rt %d\r\n", n, ok, (unsigned)p.btn, p.lx, p.ly, p.rx, p.ry, p.lt, p.rt);
		return 1;
	}
	{	/* "doomtouch x y ms" one finger (landscape px) for ms; "doommouse dx dy btn" one report */
		static EXT_RAM_BSS_ATTR int ta[3]; int a, b, c;
		if (sscanf(l, "doomtouch %d %d %d", &a, &b, &c) == 3) { ta[0] = a; ta[1] = b; ta[2] = c; xTaskCreatePinnedToCore(touch_task, "b85t", 3072, ta, 4, NULL, 1); return 1; }
		if (sscanf(l, "doommouse %d %d %d", &a, &b, &c) == 3) { b85_mouse(a, b, c); return 1; }
	}
	if (!strcmp(l, "doom ls")) {
		DIR *d = opendir(savedir); struct dirent *e; struct stat sb; char f[384];
		fprintf(stderr, "[b85] %s:\r\n", savedir);
		while (d && (e = readdir(d))) { snprintf(f, sizeof f, "%s%s", savedir, e->d_name); if (!stat(f, &sb)) fprintf(stderr, "[b85]   %s %ld B mtime %lld\r\n", e->d_name, (long)sb.st_size, (long long)sb.st_mtime); }
		if (d) closedir(d);
		return 1;
	}
	if (!strcmp(l, "doom st")) {
		player_t *pl = &players[consoleplayer];
		int px = pl->mo ? pl->mo->x >> 16 : 0, py = pl->mo ? pl->mo->y >> 16 : 0, an = pl->mo ? (int)(pl->mo->angle >> 22) : 0;   /* angle in 1/1024 turn */
		fprintf(stderr, "[b85] st gs %d menu %d E%dM%d leveltime %d pos %d %d ang %d health %d armor %d ammo %d kills %d weapon %d | pads %d (%s) kbd %d mouse %d reps %u | cfg touch %d fps %d sens %d invy %d | fps %.1f p50 %.2f p99 %.2f lat %.0f/%.0f\r\n",
			(int)gamestate, (int)menuactive, gameepisode, gamemap, leveltime, px, py, an, pl->health, pl->armorpoints, pl->ammo[0], pl->killcount, (int)pl->readyweapon,
			b85_npad, b85_npad ? b85_padname : "-", b85_nkbd, b85_nmouse, (unsigned)b85_nrep, cfg.touch, cfg.fps, cfg.sens, cfg.invy, st.fps, st.p50, st.p99, st.l50, st.l99);
		return 1;
	}
	return 0;
}

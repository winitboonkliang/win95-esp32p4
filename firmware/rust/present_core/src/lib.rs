//! B20 present pipeline + B23 pointer overlay (team BLUE).
//!
//! Source (any mode) -> persistent column-major RGB565 cache `C[sx*h+sy]` (updated only where the guest wrote)
//! -> pointer AND/XOR applied in the cache (save-under in the cache, never in guest VRAM)
//! -> dirty source rects mapped to panel rects -> gather/expand straight into the 800x1280 panel buffer
//! (rotate 90 CW + nearest scale in one pass; two panel buffers, buffer-age-2 damage list).
#![cfg_attr(target_os = "none", no_std)]
#[cfg(target_os = "none")]
#[panic_handler]
fn panic(_: &core::panic::PanicInfo) -> ! { loop {} }

pub mod kern;
pub mod capi;
pub mod linear;
#[cfg(target_os = "none")]
pub mod bench;
#[cfg(target_os = "none")]
pub mod bench_linear;

/// CRC-32 (IEEE) used by the board self-checks and host tools
pub fn crc32(data: &[u8], mut c: u32) -> u32 {
    c = !c;
    for &b in data {
        c ^= b as u32;
        for _ in 0..8 { c = if c & 1 != 0 { 0xEDB88320 ^ (c >> 1) } else { c >> 1 }; }
    }
    !c
}
pub fn crc32_u16(d: &[u16], mut c: u32) -> u32 {
    for &x in d { c = crc32(&x.to_le_bytes(), c); }
    c
}

/// classic outlined arrow (11 x 17 in a 32 x 32 shape): X = black (and 0, xor 0), . = white (and 0, xor 1), ' ' = transparent
pub fn arrow_shape() -> CursorShape {
    const ART: [&str; 17] = [
        "X          ", "XX         ", "X.X        ", "X..X       ", "X...X      ", "X....X     ", "X.....X    ", "X......X   ",
        "X.......X  ", "X........X ", "X.....XXXXX", "X..X..X    ", "X.X X..X   ", "XX  X..X   ", "X    X..X  ", "     X..X  ", "      XX   ",
    ];
    let mut s = CursorShape { w: 32, h: 32, hx: 0, hy: 0, and: [u64::MAX; CMAX], xor: [0; CMAX] };
    for (j, row) in ART.iter().enumerate() {
        for (i, c) in row.bytes().enumerate() {
            match c {
                b'X' => { s.and[j] &= !(1 << i); }
                b'.' => { s.and[j] &= !(1 << i); s.xor[j] |= 1 << i; }
                _ => {}
            }
        }
    }
    s
}

pub const PW: usize = 800; // panel
pub const PH: usize = 1280;
pub const CW: usize = 1280; // console (pre-rotation)
pub const CH: usize = 800;
pub const TILE: usize = 16; // dirty tile edge in source pixels
pub const MAXRECT: usize = 24;

/// console pixel -> source pixel maps. Rule (documented): src = floor(c * size / console)  (nearest, top-left bias):
/// every output pixel is exactly one source pixel (for scale factors up to 2 per axis that is a pixel of its 2x2 neighbourhood).
pub struct Maps {
    pub w: u16,
    pub h: u16,
    pub xmap: [u16; CW], // indexed by panel y (= console x)
    pub yrev: [u16; PW], // indexed by panel x (= 799 - console y)
    pub ykern: [u16; PW], // what the production kernel wants: byte offsets (yrev*2) on riscv32, yrev elsewhere
}
impl Maps {
    pub fn new(w: usize, h: usize) -> Maps {
        assert!(w >= 1 && w <= CW && h >= 1 && h <= CH);
        let mut m = Maps { w: w as u16, h: h as u16, xmap: [0; CW], yrev: [0; PW], ykern: [0; PW] };
        for cx in 0..CW { m.xmap[cx] = (cx * w / CW) as u16; }
        for px in 0..PW { m.yrev[px] = ((CH - 1 - px) * h / CH) as u16; m.ykern[px] = if cfg!(target_arch = "riscv32") { m.yrev[px] * 2 } else { m.yrev[px] }; }
        m
    }
}

#[derive(Clone, Copy, Default, Debug, PartialEq, Eq)]
pub struct Rect { pub x0: u16, pub y0: u16, pub x1: u16, pub y1: u16 } // half open
impl Rect {
    pub fn is_empty(&self) -> bool { self.x0 >= self.x1 || self.y0 >= self.y1 }
    pub fn area(&self) -> u32 { if self.is_empty() { 0 } else { (self.x1 - self.x0) as u32 * (self.y1 - self.y0) as u32 } }
    pub fn union(&self, o: &Rect) -> Rect {
        if self.is_empty() { return *o; }
        if o.is_empty() { return *self; }
        Rect { x0: self.x0.min(o.x0), y0: self.y0.min(o.y0), x1: self.x1.max(o.x1), y1: self.y1.max(o.y1) }
    }
}

impl Maps {
    /// source rect -> exact set of panel pixels whose value comes from it (ceil mapping, see Maps doc)
    pub fn src_to_panel(&self, r: &Rect) -> Rect {
        if r.is_empty() { return Rect::default(); }
        let (w, h) = (self.w as usize, self.h as usize);
        let cx0 = (r.x0 as usize * CW + w - 1) / w;
        let cx1 = (r.x1 as usize * CW + w - 1) / w;
        let cy0 = (r.y0 as usize * CH + h - 1) / h;
        let cy1 = (r.y1 as usize * CH + h - 1) / h;
        Rect { x0: (PW - cy1) as u16, x1: (PW - cy0) as u16, y0: cx0 as u16, y1: cx1 as u16 }
    }
}

// ---------------------------------------------------------------- source converters -> cache
const fn mk_t() -> [u32; 256] {
    let mut t = [0u32; 256];
    let mut b = 0;
    while b < 256 {
        let mut i = 0;
        let mut v = 0u32;
        while i < 8 { v |= (((b >> (7 - i)) & 1) as u32) << (4 * i); i += 1; }
        t[b] = v;
        b += 1;
    }
    t
}
/// byte of one plane -> 8 nibbles (pixel i in bits 4i..4i+3), nibble = plane bit
pub static T: [u32; 256] = mk_t();

/// RGB565 palette for the 16 colour planar mode: AR/DAC resolved exactly as reference update_palette16 (BPP16)
pub fn palette16(ar: &[u8; 21], dac: &[u8; 768]) -> [u16; 16] {
    let mut p = [0u16; 16];
    for i in 0..16 {
        let mut v = ar[i] as u32;
        if ar[0x10] & 0x80 != 0 { v = ((ar[0x14] as u32 & 0xf) << 4) | (v & 0xf); }
        else { v = ((ar[0x14] as u32 & 0xc) << 4) | (v & 0x3f); }
        let v = (v * 3) as usize;
        let col = (dac[v + 2] as u32 >> 1) | ((dac[v + 1] as u32) << 5) | ((dac[v] as u32 >> 1) << 11);
        p[i] = col as u16;
    }
    p
}

/// Planar 16 colour, shift_control 0, plane mask 0xF, no CGA address fold, no multiscan (checked by the caller).
/// vram: 4 bytes per address (byte p = plane p). `base` = byte offset of line 0, `line` = bytes per line.
/// Converts source rect (x multiples of 8) into the column-major cache.
pub fn planar16_to_cache(vram: &[u8], base: usize, line: usize, pal: &[u16; 16], cache: &mut [u16], h: usize, r: &Rect) {
    let (gx0, gx1) = (r.x0 as usize / 8, (r.x1 as usize + 7) / 8);
    for y in r.y0 as usize..r.y1 as usize {
        let row = base + y * line;
        for g in gx0..gx1 {
            let p = row + 4 * g;
            let n = T[vram[p] as usize] | (T[vram[p + 1] as usize] << 1) | (T[vram[p + 2] as usize] << 2) | (T[vram[p + 3] as usize] << 3);
            let c = &mut cache[g * 8 * h + y..];
            for i in 0..8 { c[i * h] = pal[((n >> (4 * i)) & 15) as usize]; }
        }
    }
}

/// Linear modes as vga.c with BPP=16. bpp in {8,15,16,24,32}; pal only for 8.
pub fn linear_to_cache(vram: &[u8], base: usize, pitch: usize, bpp: u32, pal: &[u16; 256], cache: &mut [u16], h: usize, r: &Rect) {
    for y in r.y0 as usize..r.y1 as usize {
        let row = base + y * pitch;
        for x in r.x0 as usize..r.x1 as usize {
            let c = match bpp {
                8 => pal[vram[row + x] as usize],
                15 => { let k = vram[row + 2 * x] as u32 | (vram[row + 2 * x + 1] as u32) << 8; ((k & 0x1f) | ((k & !0x1f) << 1)) as u16 }
                16 => vram[row + 2 * x] as u16 | (vram[row + 2 * x + 1] as u16) << 8,
                24 | 32 => {
                    let o = row + (bpp as usize / 8) * x;
                    (((vram[o] as u32 >> 3) | ((vram[o + 1] as u32 >> 2) << 5) | ((vram[o + 2] as u32 >> 3) << 11)) & 0xffff) as u16
                }
                _ => 0,
            };
            cache[x * h + y] = c;
        }
    }
}

/// 8bpp palette from DAC (reference update_palette256, BPP16)
pub fn palette256(dac: &[u8; 768], dac_8bit: bool) -> [u16; 256] {
    let mut p = [0u16; 256];
    for i in 0..256 {
        let (r, g, b) = (dac[3 * i] as u32, dac[3 * i + 1] as u32, dac[3 * i + 2] as u32);
        p[i] = if dac_8bit { ((b >> 3) | ((g >> 2) << 5) | ((r >> 3) << 11)) as u16 } else { ((b >> 1) | (g << 5) | ((r >> 1) << 11)) as u16 };
    }
    p
}

// ---------------------------------------------------------------- pointer (B23)
pub const CMAX: usize = 64;
#[derive(Clone)]
pub struct CursorShape {
    pub w: u8,
    pub h: u8,
    pub hx: i16,
    pub hy: i16,
    pub and: [u64; CMAX], // row j, pixel i = bit i
    pub xor: [u64; CMAX],
}
/// exact Windows formula on one RGB565 pixel: (bg AND andmask) XOR xormask, masks expanded to 16 bits
#[inline]
pub fn cursor_px(bg: u16, a: bool, x: bool) -> u16 { (bg & if a { 0xffff } else { 0 }) ^ if x { 0xffff } else { 0 } }

/// visible (screen-clipped) source rect of the pointer
pub fn cursor_rect(s: &CursorShape, px: i32, py: i32, sw: usize, sh: usize) -> Rect {
    let ox = px - s.hx as i32;
    let oy = py - s.hy as i32;
    let x0 = ox.max(0).min(sw as i32);
    let y0 = oy.max(0).min(sh as i32);
    let x1 = (ox + s.w as i32).max(0).min(sw as i32);
    let y1 = (oy + s.h as i32).max(0).min(sh as i32);
    if x0 >= x1 || y0 >= y1 { return Rect::default(); }
    Rect { x0: x0 as u16, y0: y0 as u16, x1: x1 as u16, y1: y1 as u16 }
}

pub struct CursorState {
    pub shape: Option<CursorShape>,
    pub x: i32,
    pub y: i32,
    pub visible: bool,
    shown: Rect,
    saved: [u16; CMAX * CMAX],
    shown_valid: bool,
}
impl CursorState {
    pub fn new() -> Self { CursorState { shape: None, x: 0, y: 0, visible: false, shown: Rect::default(), saved: [0; CMAX * CMAX], shown_valid: false } }
}

// ---------------------------------------------------------------- presenter
#[derive(Default, Clone, Copy, Debug)]
pub struct Report {
    pub src_bytes: u32,   // source bytes read to refresh the cache (converted tiles only)
    pub panel_bytes: u32, // bytes written into panel buffers this present (both buffers' catch-up counted)
    pub src_rects: u16,
    pub panel_rects: u16,
    pub full: bool,
    pub idle: bool,
}

pub struct Presenter<'a> {
    pub maps: Maps,
    pub cache: &'a mut [u16],
    tiles: [u8; (CW / TILE) * (CH / TILE)],
    tw: usize,
    th: usize,
    ndirty: u32, // flagged tiles (skip all scanning when 0)
    ty0: usize,
    ty1: usize,
    all: bool,
    pub cur: CursorState,
    cur_dirty: bool,
    hist: [[Rect; MAXRECT]; 2], // damage of the last two presents (buffer age up to 3)
    nhist: [usize; 2],
    nfb: u8,
    age: u8,
    /// source bytes read per source pixel, x1000 (planar16: 500, 16bpp: 2000)
    pub src_milli_bytes_per_px: u32,
}

impl<'a> Presenter<'a> {
    /// nfb = number of panel buffers (2 or 3)
    pub fn new(w: usize, h: usize, cache: &'a mut [u16], nfb: u8) -> Self {
        assert!(cache.len() >= w * h && (nfb == 2 || nfb == 3));
        Presenter {
            maps: Maps::new(w, h), cache,
            tiles: [0; (CW / TILE) * (CH / TILE)],
            tw: (w + TILE - 1) / TILE, th: (h + TILE - 1) / TILE, ndirty: 0, ty0: usize::MAX, ty1: 0,
            all: true, cur: CursorState::new(), cur_dirty: false,
            hist: [[Rect::default(); MAXRECT]; 2], nhist: [0; 2], nfb, age: 0, src_milli_bytes_per_px: 500,
        }
    }
    pub fn mark_all(&mut self) { self.all = true; }
    pub fn mark_rect(&mut self, r: &Rect) {
        if r.is_empty() { return; }
        let (x1, y1) = (r.x1.min(self.maps.w), r.y1.min(self.maps.h));
        for ty in r.y0 as usize / TILE..(y1 as usize + TILE - 1) / TILE {
            for tx in r.x0 as usize / TILE..(x1 as usize + TILE - 1) / TILE {
                let t = &mut self.tiles[ty * self.tw + tx];
                if *t == 0 { *t = 1; self.ndirty += 1; }
            }
            self.ty0 = self.ty0.min(ty);
            self.ty1 = self.ty1.max(ty);
        }
    }
    pub fn set_cursor(&mut self, shape: Option<&CursorShape>, x: i32, y: i32, visible: bool) {
        if let Some(s) = shape { self.cur.shape = Some(s.clone()); self.cur_dirty = true; }
        if x != self.cur.x || y != self.cur.y || visible != self.cur.visible { self.cur_dirty = true; }
        self.cur.x = x; self.cur.y = y; self.cur.visible = visible;
    }

    fn cursor_restore(&mut self) -> Rect {
        let c = &mut self.cur;
        if !c.shown_valid { return Rect::default(); }
        let h = self.maps.h as usize;
        let r = c.shown;
        for i in 0..(r.x1 - r.x0) as usize {
            for j in 0..(r.y1 - r.y0) as usize {
                self.cache[(r.x0 as usize + i) * h + r.y0 as usize + j] = c.saved[i * CMAX + j];
            }
        }
        c.shown_valid = false;
        r
    }
    fn cursor_apply(&mut self) -> Rect {
        let (w, h) = (self.maps.w as usize, self.maps.h as usize);
        let c = &mut self.cur;
        let s = match (&c.shape, c.visible) { (Some(s), true) => s, _ => return Rect::default() };
        let r = cursor_rect(s, c.x, c.y, w, h);
        if r.is_empty() { return r; }
        let ox = c.x - s.hx as i32;
        let oy = c.y - s.hy as i32;
        for x in r.x0 as usize..r.x1 as usize {
            let i = (x as i32 - ox) as usize;
            for y in r.y0 as usize..r.y1 as usize {
                let j = (y as i32 - oy) as usize;
                let idx = x * h + y;
                let bg = self.cache[idx];
                c.saved[(x - r.x0 as usize) * CMAX + (y - r.y0 as usize)] = bg;
                self.cache[idx] = cursor_px(bg, (s.and[j] >> i) & 1 != 0, (s.xor[j] >> i) & 1 != 0);
            }
        }
        c.shown = r;
        c.shown_valid = true;
        r
    }

    fn tile_rects(&mut self, out: &mut [Rect; MAXRECT]) -> usize {
        let (w, h) = (self.maps.w, self.maps.h);
        let mut n = 0;
        if self.ndirty == 0 { return 0; }
        for ty in self.ty0..=self.ty1.min(self.th - 1) {
            let mut tx = 0;
            while tx < self.tw {
                if self.tiles[ty * self.tw + tx] == 0 { tx += 1; continue; }
                let t0 = tx;
                while tx < self.tw && self.tiles[ty * self.tw + tx] != 0 { self.tiles[ty * self.tw + tx] = 0; tx += 1; }
                let r = Rect { x0: (t0 * TILE) as u16, x1: ((tx * TILE) as u16).min(w), y0: (ty * TILE) as u16, y1: (((ty + 1) * TILE) as u16).min(h) };
                if n > 0 && out[n - 1].x0 == r.x0 && out[n - 1].x1 == r.x1 && out[n - 1].y1 == r.y0 { out[n - 1].y1 = r.y1; continue; }
                if n == MAXRECT { out[n - 1] = out[n - 1].union(&r); } else { out[n] = r; n += 1; }
            }
        }
        self.ndirty = 0;
        self.ty0 = usize::MAX;
        self.ty1 = 0;
        n
    }

    /// One present. `conv` refreshes the cache from the guest surface for a source rect (planar16_to_cache etc.).
    /// `back` = panel buffer that is NOT being scanned (800 x 1280 u16, stride PW). Caller swaps after and cache-syncs `rects_out[..n]`.
    pub fn present<F: FnMut(&mut [u16], &Rect)>(&mut self, mut conv: F, back: &mut [u16], rects_out: &mut [Rect; MAXRECT]) -> (Report, usize) {
        let mut rep = Report::default();
        let (w, h) = (self.maps.w, self.maps.h);
        let any_tile = self.ndirty != 0;
        if !self.all && !any_tile && !self.cur_dirty {
            rep.idle = true;
            return (rep, 0);
        }
        let mut s_rects = [Rect::default(); MAXRECT];
        let mut ns;
        let old = self.cursor_restore();
        if self.all {
            let full = Rect { x0: 0, y0: 0, x1: w, y1: h };
            conv(self.cache, &full);
            s_rects[0] = full;
            ns = 1;
            rep.full = true;
            for t in self.tiles.iter_mut() { *t = 0; }
            self.ndirty = 0;
            self.ty0 = usize::MAX;
            self.ty1 = 0;
            self.all = false;
        } else {
            ns = self.tile_rects(&mut s_rects);
            for i in 0..ns { conv(self.cache, &s_rects[i]); }
        }
        for i in 0..ns { rep.src_bytes += (s_rects[i].area() * self.src_milli_bytes_per_px + 999) / 1000; }
        let new = self.cursor_apply();
        self.cur_dirty = false;
        fn push(r: Rect, s: &mut [Rect; MAXRECT], n: &mut usize) {
            if r.is_empty() { return; }
            if *n == MAXRECT { s[MAXRECT - 1] = s[MAXRECT - 1].union(&r); } else { s[*n] = r; *n += 1; }
        }
        push(old, &mut s_rects, &mut ns);
        push(new, &mut s_rects, &mut ns);
        rep.src_rects = ns as u16;
        let mut cur = [Rect::default(); MAXRECT];
        let mut nc = 0;
        for i in 0..ns { let pr = self.maps.src_to_panel(&s_rects[i]); push(pr, &mut cur, &mut nc); }
        // the back buffer is nfb-1 presents old: it needs the damage of the last nfb-1 presents plus this one
        let mut todo = cur;
        let mut nt = nc;
        if self.age < self.nfb {
            todo[0] = Rect { x0: 0, y0: 0, x1: PW as u16, y1: PH as u16 };
            nt = 1;
        } else {
            for k in 0..(self.nfb as usize - 1) { for i in 0..self.nhist[k] { push(self.hist[k][i], &mut todo, &mut nt); } }
        }
        if self.age < self.nfb { self.age += 1; }
        self.hist[1] = self.hist[0];
        self.nhist[1] = self.nhist[0];
        self.hist[0] = cur;
        self.nhist[0] = nc;
        for i in 0..nt {
            let r = todo[i];
            kern::expand_rect(self.cache, h as usize, &self.maps.xmap, &self.maps.ykern, &r, back, PW);
            rep.panel_bytes += r.area() * 2;
            rects_out[i] = r;
        }
        rep.panel_rects = nt as u16;
        (rep, nt)
    }
}

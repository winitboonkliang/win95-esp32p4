//! Present from a LINEAR RGB565 1280x800 guest surface (row-major, stride 1280) + damage rects (lead redirect 14 Nov, B29a driver).
//! Panel pixel (px,py) = S[(799-px)*1280 + py]  (exact 90 deg CW rotation). Large rects go through a PPA callback
//! (rotate 90 CW of the changed block only), small rects and the pointer use the CPU. nfb = 2 or 3 buffers, buffer-age damage lists.
//! Pointer overlay API for the driver: set_cursor(shape, x, y, visible); composited into the panel buffers from the CLEAN surface.
use crate::*;

pub const SW: usize = 1280;
pub const SH: usize = 800;
/// below this many source pixels the CPU rotates the rect itself (PPA job setup costs more than the work)
pub const PPA_MIN_PX: u32 = 4096;

pub struct LinearPresenter {
    /// guest surface size (stride = width) and scale to the 1280x800 panel in quarters: 4 = 1:1, 5 = 1.25x (1024x640), 8 = 2x (640x400)
    sw: usize,
    sh: usize,
    s4: usize,
    nfb: u8,
    age: u8,
    dmg: [Rect; MAXRECT],
    ndmg: usize,
    hist: [[Rect; MAXRECT]; 2],
    nhist: [usize; 2],
    pub shape: Option<CursorShape>,
    cx: i32,
    cy: i32,
    vis: bool,
    shown: Rect, // pointer rect drawn by the most recent present (source coords)
    cur_dirty: bool,
}

/// panel rect of a source rect (exact for the aligned rects the presenter produces: 4-aligned y at 1:1, 16-aligned x and y when scaled)
pub fn panel_of(r: &Rect, s4: usize) -> Rect {
    let m = |v: u16| (v as usize * s4 / 4) as u16;
    Rect { x0: (PW as u16) - m(r.y1), x1: (PW as u16) - m(r.y0), y0: m(r.x0), y1: m(r.x1) }
}

fn contains(a: &Rect, b: &Rect) -> bool { a.x0 <= b.x0 && a.y0 <= b.y0 && a.x1 >= b.x1 && a.y1 >= b.y1 }
fn push(r: Rect, s: &mut [Rect; MAXRECT], n: &mut usize) {
    if r.is_empty() { return; }
    // containment dedupe: a full-frame damage must not be repainted 3x (buffer-age lists)
    for i in 0..*n { if contains(&s[i], &r) { return; } }
    let mut k = 0;
    for i in 0..*n { if !contains(&r, &s[i]) { s[k] = s[i]; k += 1; } }
    *n = k;
    if *n == MAXRECT { s[MAXRECT - 1] = s[MAXRECT - 1].union(&r); } else { s[*n] = r; *n += 1; }
}
/// widen a source rect so its PPA job is legal: at 1:1 the panel x (= 800 - y) and width must be multiples of 4 (esp-idf #19096); when scaled both axes are
/// widened to 16 px, which scale to 20 / 32 px (multiples of 4) exactly, so the scaled block lands on whole panel pixels
fn align_rect(r: &Rect, s4: usize, sw: usize, sh: usize) -> Rect {
    if r.is_empty() { return *r; }
    if s4 == 4 { return Rect { x0: r.x0, x1: r.x1, y0: r.y0 & !3, y1: ((r.y1 + 3) & !3).min(sh as u16) }; }
    Rect { x0: r.x0 & !15, x1: ((r.x1 + 15) & !15).min(sw as u16), y0: r.y0 & !15, y1: ((r.y1 + 15) & !15).min(sh as u16) }
}

impl LinearPresenter {
    pub fn new(nfb: u8) -> Self {
        assert!(nfb == 2 || nfb == 3);
        LinearPresenter { sw: SW, sh: SH, s4: 4, nfb, age: 0, dmg: [Rect::default(); MAXRECT], ndmg: 0, hist: [[Rect::default(); MAXRECT]; 2], nhist: [0; 2], shape: None, cx: 0, cy: 0, vis: false, shown: Rect::default(), cur_dirty: false }
    }
    /// guest mode change: 1280x800 (1:1), 1024x640 (1.25x) or 640x400 (2x); forgets all damage history and repaints everything
    pub fn set_mode(&mut self, sw: usize, sh: usize) {
        self.s4 = match sw { 1024 => 5, 640 => 8, _ => 4 };
        self.sw = if self.s4 == 4 { SW } else { sw };
        self.sh = if self.s4 == 4 { SH } else { sh };
        self.age = 0; self.ndmg = 0; self.nhist = [0; 2]; self.shown = Rect::default(); self.cur_dirty = true;
        self.damage_all();
    }
    pub fn scale4(&self) -> usize { self.s4 }
    pub fn dims(&self) -> (usize, usize) { (self.sw, self.sh) }
    /// the guest/driver changed surface pixels in r
    pub fn damage(&mut self, r: &Rect) {
        let r = Rect { x0: r.x0.min(self.sw as u16), x1: r.x1.min(self.sw as u16), y0: r.y0.min(self.sh as u16), y1: r.y1.min(self.sh as u16) };
        let mut n = self.ndmg;
        push(r, &mut self.dmg, &mut n);
        self.ndmg = n;
    }
    pub fn damage_all(&mut self) { let r = Rect { x0: 0, y0: 0, x1: self.sw as u16, y1: self.sh as u16 }; self.damage(&r); }
    /// SetCursor (shape = Some) / MoveCursor (None)
    pub fn set_cursor(&mut self, shape: Option<&CursorShape>, x: i32, y: i32, visible: bool) {
        if let Some(s) = shape { self.shape = Some(s.clone()); self.cur_dirty = true; }
        if x != self.cx || y != self.cy || visible != self.vis { self.cur_dirty = true; }
        self.cx = x;
        self.cy = y;
        self.vis = visible;
    }

    /// One present. `ppa(src_rect_aligned, back)` must rotate that source block of the surface into `back` (PPA job on board).
    /// Returns (rect count, source px rotated); `out[..count]` = panel rects to cache-sync. count 0 = idle (do not swap).
    pub fn present<F: FnMut(&Rect, &mut [u16])>(&mut self, s: &[u16], back: &mut [u16], mut ppa: F, out: &mut [Rect; MAXRECT]) -> (usize, u32) {
        if self.ndmg == 0 && !self.cur_dirty { return (0, 0); }
        let new = match (&self.shape, self.vis) { (Some(sh), true) => cursor_rect(sh, self.cx, self.cy, self.sw, self.sh), _ => Rect::default() };
        let mut cur = [Rect::default(); MAXRECT];
        let mut nc = 0;
        for i in 0..self.ndmg { push(align_rect(&self.dmg[i], self.s4, self.sw, self.sh), &mut cur, &mut nc); }
        push(align_rect(&self.shown, self.s4, self.sw, self.sh), &mut cur, &mut nc);
        push(align_rect(&new, self.s4, self.sw, self.sh), &mut cur, &mut nc);
        let mut todo = cur;
        let mut nt = nc;
        if self.age < self.nfb {
            todo[0] = Rect { x0: 0, y0: 0, x1: self.sw as u16, y1: self.sh as u16 };
            nt = 1;
        } else {
            for k in 0..self.nfb as usize - 1 { for i in 0..self.nhist[k] { push(self.hist[k][i], &mut todo, &mut nt); } }
        }
        if self.age < self.nfb { self.age += 1; }
        self.hist[1] = self.hist[0];
        self.nhist[1] = self.nhist[0];
        self.hist[0] = cur;
        self.nhist[0] = nc;
        let mut px = 0u32;
        for i in 0..nt {
            let r = todo[i];
            if r.area() >= PPA_MIN_PX || self.s4 != 4 { ppa(&r, back); } else { rot_cpu(s, &r, back); }
            px += r.area();
            out[i] = panel_of(&r, self.s4);
        }
        if !new.is_empty() { self.draw_cursor(s, &new, back); }
        self.shown = new;
        self.ndmg = 0;
        self.cur_dirty = false;
        (nt, px)
    }

    /// final panel pixel at (px, py): the (scaled) surface pixel with the pointer composited when it covers it
    fn final_px(&self, s: &[u16], px: usize, py: usize) -> u16 {
        let s4 = self.s4;
        let (x, y) = (py, PW - 1 - px);                       // scaled-surface coordinates
        let v = s[(y * 4 / s4) * self.sw + x * 4 / s4];
        if !self.vis { return v; }
        let sh = match &self.shape { Some(sh) => sh, None => return v };
        let m = |t: i32| t * s4 as i32 / 4;
        let (ox, oy) = (m(self.cx - sh.hx as i32), m(self.cy - sh.hy as i32));
        let (i, j) = ((x as i32 - ox) * 4 / s4 as i32, (y as i32 - oy) * 4 / s4 as i32);
        if (x as i32) < ox || (y as i32) < oy || i < 0 || j < 0 || i >= sh.w as i32 || j >= sh.h as i32 { return v; }
        cursor_px(v, (sh.and[j as usize] >> i) & 1 != 0, (sh.xor[j as usize] >> i) & 1 != 0)
    }

    /// panel rect that covers a source rect completely (ceil on the far edges), clamped to the panel
    fn panel_cover(&self, r: &Rect) -> Rect {
        if r.is_empty() { return Rect::default(); }
        let s4 = self.s4;
        let fl = |v: u16| (v as usize * s4 / 4) as u16;
        let ce = |v: u16| ((v as usize * s4 + 3) / 4).min(PH.max(PW)) as u16;
        Rect { x0: (PW as u16).saturating_sub(ce(r.y1)), x1: (PW as u16).saturating_sub(fl(r.y0)).min(PW as u16), y0: fl(r.x0), y1: ce(r.x1).min(PH as u16) }
    }

    /// POINTER FAST PATH: the pointer moved or changed shape and nothing else did.  Writes the final pixels of the old and the new pointer rect into BOTH panel buffers at once
    /// (each pixel exactly once: no erase-then-draw state is ever visible, the scanned buffer may be written while it is on screen), without waiting for a refresh or a flip.
    /// Returns the number of panel rects written (cache-sync them in both buffers); 0 = not possible now (history not built up yet) -> use the normal present.
    pub fn cursor_fast(&mut self, s: &[u16], b0: &mut [u16], b1: &mut [u16], out: &mut [Rect; 2]) -> usize {
        if self.age < self.nfb || self.shape.is_none() || !self.cur_dirty { return 0; }
        let new = match (&self.shape, self.vis) { (Some(sh), true) => cursor_rect(sh, self.cx, self.cy, self.sw, self.sh), _ => Rect::default() };
        let rects = [self.panel_cover(&self.shown), self.panel_cover(&new)];
        let mut n = 0;
        for r in rects.iter() {
            if r.is_empty() { continue; }
            for py in r.y0 as usize..r.y1 as usize {
                for px in r.x0 as usize..r.x1 as usize {
                    let v = self.final_px(s, px, py);
                    b0[py * PW + px] = v;
                    b1[py * PW + px] = v;
                }
            }
            out[n] = *r; n += 1;
        }
        self.shown = new;
        self.cur_dirty = false;
        n
    }

    fn draw_cursor(&self, s: &[u16], r: &Rect, back: &mut [u16]) {
        let sh = self.shape.as_ref().unwrap();
        if self.s4 == 4 {
            let (ox, oy) = (self.cx - sh.hx as i32, self.cy - sh.hy as i32);
            for y in r.y0 as usize..r.y1 as usize {
                let j = (y as i32 - oy) as usize;
                for x in r.x0 as usize..r.x1 as usize {
                    let i = (x as i32 - ox) as usize;
                    back[x * PW + (PW - 1 - y)] = cursor_px(s[y * SW + x], (sh.and[j] >> i) & 1 != 0, (sh.xor[j] >> i) & 1 != 0);
                }
            }
            return;
        }
        // scaled: composite in panel space over the already scaled background (the covering rect was repainted just before); the shape is scaled the
        // same way as the surface (nearest), so a 32 px arrow is 40 px at 1.25x and 64 px at 2x
        let s4 = self.s4 as i32;
        let m = |v: i32| v * s4 / 4;
        let (ox, oy) = (m(self.cx - sh.hx as i32), m(self.cy - sh.hy as i32));
        let pr = panel_of(r, self.s4);
        for py in pr.y0 as usize..pr.y1 as usize {
            let xs = py as i32;                       // scaled-surface x
            let i = (xs - ox) * 4 / s4;
            if xs < ox || i < 0 || i >= sh.w as i32 { continue; }
            for px in pr.x0 as usize..pr.x1 as usize {
                let ys = PW as i32 - 1 - px as i32;   // scaled-surface y
                let j = (ys - oy) * 4 / s4;
                if ys < oy || j < 0 || j >= sh.h as i32 { continue; }
                let q = py * PW + px;
                back[q] = cursor_px(back[q], (sh.and[j as usize] >> i) & 1 != 0, (sh.xor[j as usize] >> i) & 1 != 0);
            }
        }
    }
}

/// CPU rotate of one block (also the reference for the PPA path)
pub fn rot_cpu(s: &[u16], r: &Rect, back: &mut [u16]) {
    for x in r.x0 as usize..r.x1 as usize {
        let d = &mut back[x * PW..x * PW + PW];
        for y in r.y0 as usize..r.y1 as usize { d[PW - 1 - y] = s[y * SW + x]; }
    }
}

/// CPU reference of the scaled rotate (nearest, dest d <- src floor(d*4/s4)): what the PPA job is expected to produce; used by the tests
pub fn rot_cpu_scaled(s: &[u16], sw: usize, s4: usize, r: &Rect, back: &mut [u16]) {
    let pr = panel_of(r, s4);
    for py in pr.y0 as usize..pr.y1 as usize {
        let sx = py * 4 / s4;
        for px in pr.x0 as usize..pr.x1 as usize {
            let sy = (PW - 1 - px) * 4 / s4;
            back[py * PW + px] = s[sy * sw + sx];
        }
    }
}

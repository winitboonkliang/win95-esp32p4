//! BLUE B45 pointer engine: touch reports (GSL3680) and USB relative reports -> the input events Windows receives.
//! One path for every pointing device: the host decides the gesture, keeps the overlay position, and queues
//! absolute records (flags, x16, y16) that our MOUSE.DRV hands straight to USER's mouse event entry.
//! Pure logic, no_std, no heap, no float: the same code runs in the PC trace test and on the board (bit-exact).
#![cfg_attr(target_os = "none", no_std)]

/// USER mouse event flags (Win 3.x/95 MOUSE.DRV event-proc contract, AX register).
pub const SF_MOVE: u16 = 0x0001;
pub const SF_LDOWN: u16 = 0x0002;
pub const SF_LUP: u16 = 0x0004;
pub const SF_RDOWN: u16 = 0x0008;
pub const SF_RUP: u16 = 0x0010;
pub const SF_ABS: u16 = 0x8000;

/// One event as Windows receives it. px/py = the screen pixel it must land on (test oracle), x16/y16 = what USER gets.
#[repr(C)]
#[derive(Clone, Copy, PartialEq, Eq, Debug, Default)]
pub struct Ev {
    pub t_us: u32,
    pub flags: u16,
    pub px: u16,
    pub py: u16,
    pub x16: u16,
    pub y16: u16,
    pub _pad: u16,
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct Cfg {
    pub raw_w: u16,     // chip x range (portrait panel: 800)
    pub raw_h: u16,     // chip y range (1280)
    pub scr_w: u16,     // guest screen (1280)
    pub scr_h: u16,     // (800)
    pub rotate180: u8,  // BOARD_ROTATE_180
    pub _pad: u8,
    pub dead_zone: u16, // px a finger may wander before a touch becomes a drag
    pub dbl_px: u16,    // second tap this close (px) and this soon -> snapped onto the first tap (Windows then sees a double click)
    pub _pad2: u16,
    pub long_us: u32,   // hold this long inside the dead zone -> right click
    pub dbl_us: u32,
    pub move_us: u32,   // min spacing of move-only events to Windows (30 Hz)
    pub hold_us: u32,   // a tap's / long press's button stays down this long for Windows (apps check the button is still down)
}

pub const DEFAULT_CFG: Cfg = Cfg {
    raw_w: 800, raw_h: 1280, scr_w: 1280, scr_h: 800, rotate180: 1, _pad: 0,
    dead_zone: 12, dbl_px: 24, _pad2: 0, long_us: 650_000, dbl_us: 450_000, move_us: 33_334, hold_us: 60_000,
};

/// Absolute coordinate for pixel p on a screen of n pixels: a quarter pixel into the pixel, so floor or round of
/// x16*n/65536 (and of x16*n/65535, MulDiv style) all give p back (proved for every p in tests::abs16_roundtrip).
#[inline]
pub fn abs16(p: u16, n: u16) -> u16 {
    let d = 4 * n as u32;
    let v = (((4 * p as u32 + 1) << 16) + d - 1) / d;
    if v > 0xffff { 0xffff } else { v as u16 }
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum St {
    Idle,
    Pend, // finger down, gesture not decided yet
    Drag, // left button held for Windows
    Long, // right click sent, waiting for the finger to lift
}

const Q: usize = 64;
pub const BTN_L: u8 = 1;
pub const BTN_R: u8 = 2;

pub struct Engine {
    pub cfg: Cfg,
    st: St,
    land: (u16, u16),  // where the finger landed (dead-zone origin)
    press: (u16, u16), // where Windows gets the press (landing point, or the snapped first-tap point)
    t0: u32,
    fing: (u16, u16), // last finger position
    last_tap: Option<(u32, u16, u16)>,
    up_btn: u8, // button of a tap / long press still to be released
    up_due: u32,
    up_pos: (u16, u16),
    pos: (u16, u16),     // pointer position the host shows (overlay) and last told / to tell Windows
    sent: (u16, u16),    // last position Windows got
    move_pend: bool,
    last_move: u32,
    tbtn: u8, // touch buttons
    ubtn: u8, // usb buttons
    abtn: u8, // buttons from set_pointer_abs (B70 host-assisted window move)
    wbtn: u8, // buttons Windows believes are down
    q: [Ev; Q],
    qh: usize,
    qn: usize,
    pub ovl_seq: u32,
    pub overflow: u32,
    pub taps: u32,
    pub drags: u32,
    pub longs: u32,
}

impl Engine {
    pub const fn new(cfg: Cfg) -> Self {
        Engine {
            cfg, st: St::Idle, land: (0, 0), press: (0, 0), t0: 0, fing: (0, 0), last_tap: None, up_btn: 0, up_due: 0, up_pos: (0, 0),
            pos: (cfg.scr_w / 2, cfg.scr_h / 2), sent: (cfg.scr_w / 2, cfg.scr_h / 2), move_pend: false, last_move: 0,
            tbtn: 0, ubtn: 0, abtn: 0, wbtn: 0,
            q: [Ev { t_us: 0, flags: 0, px: 0, py: 0, x16: 0, y16: 0, _pad: 0 }; Q], qh: 0, qn: 0,
            ovl_seq: 0, overflow: 0, taps: 0, drags: 0, longs: 0,
        }
    }

    /// chip coordinates -> guest screen pixel (portrait chip, landscape screen, optional 180 degrees)
    pub fn map(&self, rx: u16, ry: u16) -> (u16, u16) {
        let c = &self.cfg;
        let rx = if rx >= c.raw_w { c.raw_w - 1 } else { rx };
        let ry = if ry >= c.raw_h { c.raw_h - 1 } else { ry };
        // landscape: x along the chip's long axis, y = mirrored short axis (same transform the board's Doom port proved)
        let mut x = (ry as u32 * c.scr_w as u32 / c.raw_h as u32) as u16;
        let mut y = ((c.raw_w - 1 - rx) as u32 * c.scr_h as u32 / c.raw_w as u32) as u16;
        if c.rotate180 != 0 {
            x = c.scr_w - 1 - x;
            y = c.scr_h - 1 - y;
        }
        (x, y)
    }

    pub fn overlay(&self) -> (u16, u16) { self.pos }

    fn push(&mut self, t: u32, flags: u16, p: (u16, u16)) {
        let ev = Ev { t_us: t, flags: flags | SF_ABS | SF_MOVE, px: p.0, py: p.1,
            x16: abs16(p.0, self.cfg.scr_w), y16: abs16(p.1, self.cfg.scr_h), _pad: 0 };
        self.sent = p;
        // coalesce: a move-only record still waiting in the queue is replaced by the newer position
        // B87: never while a button is held: a drag (Paint pencil, window drag) needs every point, merging cut the corners (owner demo 22 Nov)
        if flags == 0 && self.qn > 0 && self.wbtn == 0 {
            let tail = (self.qh + self.qn - 1) % Q;
            if self.q[tail].flags == SF_ABS | SF_MOVE { self.q[tail] = ev; return; }
        }
        if self.qn == Q {
            self.overflow += 1;
            if flags == 0 { return; } // never drop a button record: drop the oldest move instead
            let mut i = 0;
            while i < Q && self.q[(self.qh + i) % Q].flags != SF_ABS | SF_MOVE { i += 1; }
            if i == Q { return; }
            let mut j = i;
            while j > 0 { self.q[(self.qh + j) % Q] = self.q[(self.qh + j - 1) % Q]; j -= 1; }
            self.qh = (self.qh + 1) % Q; self.qn -= 1;
        }
        self.q[(self.qh + self.qn) % Q] = ev;
        self.qn += 1;
    }

    /// buttons Windows should see = touch OR usb; one record per changed button, all at position p
    fn buttons(&mut self, t: u32, p: (u16, u16)) {
        let want = self.tbtn | self.ubtn | self.abtn;
        let ch = want ^ self.wbtn;
        if ch == 0 { return; }
        let mut f = 0;
        if ch & BTN_L != 0 { f |= if want & BTN_L != 0 { SF_LDOWN } else { SF_LUP }; }
        if ch & BTN_R != 0 { f |= if want & BTN_R != 0 { SF_RDOWN } else { SF_RUP }; }
        self.wbtn = want;
        self.move_pend = false;
        self.last_move = t;
        // USER applies the button of a record BEFORE its move (measured: PC run s1/s1b): the exact position goes first, alone
        if p != self.sent { self.push(t, 0, p); }
        self.push(t, f, p);
    }

    /// release the held tap / long-press button now (due, or a new gesture starts)
    fn flush_up(&mut self, t: u32) {
        if self.up_btn != 0 { let m = self.up_btn; self.up_btn = 0; self.tbtn &= !m; let p = self.up_pos; self.buttons(t, p); }
    }
    /// press button m at p and schedule its release after hold_us
    fn click(&mut self, t: u32, m: u8, p: (u16, u16)) {
        self.flush_up(t);
        self.tbtn |= m; self.buttons(t, p);
        self.up_btn = m; self.up_due = t.wrapping_add(self.cfg.hold_us); self.up_pos = p;
    }

    /// time passes: held-button release, long-press timeout and the 30 Hz move pacing
    pub fn tick(&mut self, t: u32) {
        if self.up_btn != 0 && t.wrapping_sub(self.up_due) as i32 >= 0 { self.flush_up(t); }
        if self.st == St::Pend && t.wrapping_sub(self.t0) >= self.cfg.long_us {
            let p = self.press;
            self.click(t, BTN_R, p);
            self.st = St::Long; self.last_tap = None; self.longs += 1;
        }
        if self.move_pend && t.wrapping_sub(self.last_move) >= self.cfg.move_us {
            self.move_pend = false; self.last_move = t;
            let p = self.pos;
            if p != self.sent { self.push(t, 0, p); }
        }
    }

    /// one touch report: down = a finger is on the panel (first finger only), raw chip coordinates
    pub fn touch(&mut self, t: u32, down: bool, rx: u16, ry: u16) {
        self.tick(t);
        if !down {
            match self.st {
                St::Pend => {
                    let p = self.press;
                    self.click(t, BTN_L, p);
                    self.last_tap = Some((t, p.0, p.1)); self.taps += 1;
                }
                St::Drag => {
                    let p = self.fing; self.pos = p;
                    self.tbtn &= !BTN_L; self.buttons(t, p); // exact release position in the same record
                    self.last_tap = None;
                }
                _ => {}
            }
            self.st = St::Idle;
            return;
        }
        let f = self.map(rx, ry);
        self.fing = f;
        self.pos = f; // overlay follows the finger at once
        self.ovl_seq = self.ovl_seq.wrapping_add(1);
        match self.st {
            St::Idle => {
                self.flush_up(t);
                self.land = f; self.t0 = t; self.st = St::Pend;
                self.press = f;
                if let Some((tt, x, y)) = self.last_tap {
                    if t.wrapping_sub(tt) <= self.cfg.dbl_us && dist2(f, (x, y)) <= sq(self.cfg.dbl_px) { self.press = (x, y); }
                }
            }
            St::Pend => {
                if dist2(f, self.land) > sq(self.cfg.dead_zone) {
                    self.st = St::Drag; self.drags += 1;
                    let p = self.press;
                    self.tbtn |= BTN_L; self.buttons(t, p); // press where the finger landed
                    self.move_pend = true;
                }
            }
            St::Drag => { self.move_pend = true; self.tick(t); }
            St::Long => {}
        }
    }

    /// USB (later BLE) relative report: host applies the Win95 default acceleration and sends absolute positions
    pub fn rel(&mut self, t: u32, dx: i32, dy: i32, b: u8) {
        self.tick(t);
        let acc = |d: i32| if d > 2 || d < -2 { 2 * d } else { d };
        let cl = |v: i32, n: u16| if v < 0 { 0 } else if v >= n as i32 { (n - 1) as u16 } else { v as u16 };
        let p = (cl(self.pos.0 as i32 + acc(dx), self.cfg.scr_w), cl(self.pos.1 as i32 + acc(dy), self.cfg.scr_h));
        let moved = p != self.pos;
        self.pos = p;
        if moved { self.ovl_seq = self.ovl_seq.wrapping_add(1); }
        if (b & 3) != self.ubtn { self.flush_up(t); }
        self.ubtn = b & 3;
        if (self.tbtn | self.ubtn | self.abtn) != self.wbtn { self.buttons(t, p); }
        else if moved && self.wbtn != 0 { self.move_pend = false; self.last_move = t; self.push(t, 0, p); }   // B87: held button = exact path, no pacing
        else if moved { self.move_pend = true; self.tick(t); }
    }

    /// set_pointer_abs (lead addendum A1-A5, B70): exact pixel, no acceleration, no pacing (A1). A button change goes out as
    /// move-then-button records behind every earlier position (A2: button records are never merged or moved). Positions not yet
    /// read by the guest coalesce into the newest one (A3). O(1) (A5).
    pub fn set_abs(&mut self, t: u32, x: u16, y: u16, b: u8) {
        self.tick(t);
        let p = (if x >= self.cfg.scr_w { self.cfg.scr_w - 1 } else { x }, if y >= self.cfg.scr_h { self.cfg.scr_h - 1 } else { y });
        if p != self.pos { self.ovl_seq = self.ovl_seq.wrapping_add(1); }
        self.pos = p;
        self.abtn = b & 3;
        if (self.tbtn | self.ubtn | self.abtn) != self.wbtn { self.buttons(t, p); }
        else if p != self.sent { self.move_pend = false; self.last_move = t; self.push(t, 0, p); }
    }

    /// Windows moved the pointer itself (SetCursorPos): adopt it when nothing of ours is in flight
    pub fn sync(&mut self, x: u16, y: u16) {
        if self.st == St::Idle && !self.move_pend && self.qn == 0 && (x, y) != self.pos {
            self.pos = (x, y); self.sent = (x, y); self.ovl_seq = self.ovl_seq.wrapping_add(1);
        }
    }

    pub fn pending(&self) -> usize { self.qn }
    /// nothing left for the guest: queue empty and no paced move waiting (A4 consumed = this + the guest ISR finished)
    pub fn idle(&self) -> bool { self.qn == 0 && !self.move_pend }

    /// guest reads the next record
    pub fn pop(&mut self) -> Option<Ev> {
        if self.qn == 0 { return None; }
        let e = self.q[self.qh];
        self.qh = (self.qh + 1) % Q; self.qn -= 1;
        Some(e)
    }
}

#[inline]
fn sq(v: u16) -> u32 { v as u32 * v as u32 }
#[inline]
fn dist2(a: (u16, u16), b: (u16, u16)) -> u32 {
    let dx = a.0 as i32 - b.0 as i32;
    let dy = a.1 as i32 - b.1 as i32;
    (dx * dx + dy * dy) as u32
}

// ---------------- C ABI (one engine per board; caller serialises calls) ----------------
#[cfg(feature = "capi")]
mod capi {
    use super::*;
    static mut E: Engine = Engine::new(DEFAULT_CFG);
    #[allow(static_mut_refs)]
    fn e() -> &'static mut Engine { unsafe { &mut E } }
    #[no_mangle] pub extern "C" fn b45_init(c: *const Cfg) { let c = if c.is_null() { DEFAULT_CFG } else { unsafe { *c } }; *e() = Engine::new(c); }
    #[no_mangle] pub extern "C" fn b45_touch(t: u32, down: i32, rx: u16, ry: u16) { e().touch(t, down != 0, rx, ry) }
    #[no_mangle] pub extern "C" fn b45_rel(t: u32, dx: i32, dy: i32, b: u8) { e().rel(t, dx, dy, b) }
    #[no_mangle] pub extern "C" fn b45_set_abs(t: u32, x: u16, y: u16, b: u8) { e().set_abs(t, x, y, b) }
    #[no_mangle] pub extern "C" fn b45_tick(t: u32) { e().tick(t) }
    #[no_mangle] pub extern "C" fn b45_sync(x: u16, y: u16) { e().sync(x, y) }
    #[no_mangle] pub extern "C" fn b45_idle() -> i32 { e().idle() as i32 }
    #[no_mangle] pub extern "C" fn b45_pending() -> i32 { e().pending() as i32 }
    #[no_mangle] pub extern "C" fn b45_pop(out: *mut Ev) -> i32 { match e().pop() { Some(v) => { unsafe { *out = v; } 1 } None => 0 } }
    /// overlay position packed y<<16|x, plus sequence number (changes whenever the host pointer moved)
    #[no_mangle] pub extern "C" fn b45_overlay(seq: *mut u32) -> u32 { let g = e(); if !seq.is_null() { unsafe { *seq = g.ovl_seq; } } let (x, y) = g.overlay(); (y as u32) << 16 | x as u32 }
    #[no_mangle] pub extern "C" fn b45_stats(out: *mut u32) { let g = e(); unsafe { *out = g.taps; *out.add(1) = g.drags; *out.add(2) = g.longs; *out.add(3) = g.overflow; } }
    #[cfg(target_os = "none")]
    #[panic_handler]
    fn panic(_: &core::panic::PanicInfo) -> ! { loop {} }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn set_abs_a1_a3() {
        // guest does not read: 3 positions coalesce (A3), the press stays behind them in order (A2), last position exact (A1)
        let mut e = Engine::new(DEFAULT_CFG);
        e.set_abs(0, 10, 10, 0); e.set_abs(1, 20, 10, 0); e.set_abs(2, 30, 10, 0); e.set_abs(3, 30, 10, 1); e.set_abs(4, 40, 12, 1); e.set_abs(5, 50, 14, 1);
        let v: Vec<(u16, u16, u16)> = core::iter::from_fn(|| e.pop()).map(|r| (r.flags, r.px, r.py)).collect();
        // B87: moves with the button held are no longer merged (drag path kept), moves with no button still merge (A3)
        assert_eq!(v, vec![(0x8001, 30, 10), (0x8003, 30, 10), (0x8001, 40, 12), (0x8001, 50, 14)]);
        assert!(e.idle());
    }
    #[test]
    fn rel_drag_keeps_every_point() {
        // B87: a held-button drag from rel() (USB mouse / console ptr) reaches the guest point by point, unpaced
        let mut e = Engine::new(DEFAULT_CFG);
        e.rel(0, 100, 100, 0); while e.pop().is_some() {}
        e.rel(1, 0, 0, 1); e.rel(2, 0, -50, 1); e.rel(3, 50, 0, 1); e.rel(4, 0, 50, 1); e.rel(5, 0, 0, 0);
        let v: Vec<(u16, u16)> = core::iter::from_fn(|| e.pop()).filter(|r| r.flags == 0x8001).map(|r| (r.px, r.py)).collect();
        let (x0, y0) = e.overlay();
        assert_eq!(v, vec![(x0 - 100, y0), (x0 - 100, y0 - 100), (x0, y0 - 100), (x0, y0)], "press point + every corner kept");
    }
    #[test]
    fn abs16_roundtrip() {
        for &n in &[640u16, 800, 1024, 1280] {
            for p in 0..n {
                let v = abs16(p, n) as u32;
                assert_eq!((v * n as u32) >> 16, p as u32, "floor n={} p={}", n, p);
                assert_eq!((v * n as u32 + 32768) >> 16, p as u32, "round n={} p={}", n, p);
                assert_eq!(v * n as u32 / 65535, p as u32, "floor65535 n={} p={}", n, p);
                assert_eq!((v * n as u32 + 32767) / 65535, p as u32, "round65535 n={} p={}", n, p);
            }
        }
    }
}

//! Intel 8254 PIT, three channels, written from the datasheet (not from tiny386's i8254.c).
//! Pure state machine: every method takes `now` (guest microseconds, u64) so the same code runs on the
//! host test harness (injected clock) and on the board.
//!
//! Fixed relative to tiny386's i8254.c (read, checked, replaced):
//!  * all 8 control-word mode codes handled (6/7 = 2/3), nothing can abort;
//!  * 64-bit time base (tiny386: u32 us, wraps every 71.6 min);
//!  * IRQ0 accounting keeps a backlog (`due - delivered`) instead of a 1-bit coalesce;
//!  * counter stops after a control word until the count is written; modes 1/5 need a gate trigger;
//!  * mode 3 odd counts follow the datasheet (second half reloads N-1); BCD counting supported;
//!  * read-back status byte (OUT, null count, rw, mode, bcd) per datasheet.

pub const PIT_HZ: u64 = 1_193_182;

#[inline(always)]
fn div(a: u64, b: u64) -> u64 { a.checked_div(b).unwrap_or(0) }
#[inline(always)]
fn rem(a: u64, b: u64) -> u64 { a.checked_rem(b).unwrap_or(0) }


#[derive(Clone, Copy)]
pub struct Chan {
    pub count: u32, // 1..=65536 counted value (0 loaded = 65536, BCD 0 = 10000)
    load_t: u64,    // guest us when counting (re)started
    pend_count: u32, // 0 = none; takes effect at end of current period (mode 2/3)
    latched: u16,
    count_latched: u8, // 0 none, else rw mode at latch time
    read_msb_next: bool,
    status: u8,
    status_latched: bool,
    rw: u8, // 1 lsb, 2 msb, 3 word
    write_hi_next: bool,
    write_lo: u8,
    pub mode: u8, // 0..=5
    bcd: bool,
    pub gate: bool,
    loaded: bool,
    triggered: bool, // modes 1/5: gate trigger seen
    null_count: bool,
    frozen: u16, // value shown while not counting
    delivered: u64, // IRQ edges already handed out since (re)load
}

impl Chan {
    const fn new(gate: bool) -> Chan {
        Chan {
            count: 65536, load_t: 0, pend_count: 0, latched: 0, count_latched: 0, read_msb_next: false,
            status: 0, status_latched: false, rw: 3, write_hi_next: false, write_lo: 0, mode: 3, bcd: false,
            gate, loaded: true, triggered: false, null_count: false, frozen: 0, delivered: 0,
        }
    }

    #[inline]
    fn clocks(dt_us: u64) -> u64 {
        // dt up to ~1.5e13 us (170 days) fits u64 after x1.19e6
        dt_us.saturating_mul(PIT_HZ) / 1_000_000
    }

    /// Fold a finished period into the new count (mode 2/3 write while running). Phase error <= 1 us.
    fn resolve_pending(&mut self, now: u64) {
        if self.pend_count != 0 && (self.mode == 2 || self.mode == 3) && self.loaded {
            let d = Self::clocks(now.saturating_sub(self.load_t));
            let c = self.count as u64;
            if d >= c {
                let adv_clk = (div(d, c)) * c;
                self.load_t = self.load_t.saturating_add(adv_clk.saturating_mul(1_000_000).saturating_add(PIT_HZ - 1) / PIT_HZ);
                self.count = self.pend_count;
                self.pend_count = 0;
                self.delivered = 0;
            }
        }
    }

    fn counter(&mut self, now: u64) -> u32 {
        if !self.loaded {
            return self.frozen as u32;
        }
        self.resolve_pending(now);
        let c = self.count as u64;
        let d = Self::clocks(now.saturating_sub(self.load_t));
        match self.mode {
            0 | 4 => (c.wrapping_sub(d) & 0xffff) as u32,
            1 | 5 => {
                if self.triggered { (c.wrapping_sub(d) & 0xffff) as u32 } else { (c & 0xffff) as u32 }
            }
            3 => {
                let x = rem(d, c);
                let half = (c + 1) / 2;
                let v = if x < half {
                    c - 2 * x
                } else if c % 2 == 0 {
                    c - 2 * (x - half) // even: second half restarts from N
                } else {
                    (c - 1) - 2 * (x - half) // odd: second half restarts from N-1
                };
                (v & 0xffff) as u32 // 65536 reads as 0 like the real chip
            }
            _ => ((c - (rem(d, c))) & 0xffff) as u32,
        }
    }

    fn to_bcd(v: u32) -> u16 {
        let v = v % 10000;
        (((v / 1000) << 12) | (((v / 100) % 10) << 8) | (((v / 10) % 10) << 4) | (v % 10)) as u16
    }

    fn shown(&mut self, now: u64) -> u16 {
        let b = self.counter(now);
        if self.bcd { Self::to_bcd(b) } else { b as u16 }
    }

    fn out(&mut self, now: u64) -> bool {
        if !self.loaded {
            return self.mode != 0;
        }
        self.resolve_pending(now);
        let c = self.count as u64;
        let d = Self::clocks(now.saturating_sub(self.load_t));
        match self.mode {
            0 => d >= c,
            1 => !(self.triggered && d < c),
            2 => c == 1 || rem(d, c) != c - 1,
            3 => (rem(d, c)) < (c + 1) / 2,
            4 => d != c,
            _ => !(self.triggered && d == c),
        }
    }

    fn load_count(&mut self, now: u64, raw: u32) {
        let mut v = raw & 0xffff;
        if self.bcd {
            v = (((v >> 12) & 15) % 10) * 1000 + (((v >> 8) & 15) % 10) * 100 + (((v >> 4) & 15) % 10) * 10 + (v & 15) % 10;
            if v == 0 { v = 10000; }
        } else if v == 0 {
            v = 65536;
        }
        self.null_count = false;
        if self.loaded && (self.mode == 2 || self.mode == 3) {
            self.resolve_pending(now);
            self.pend_count = v; // new count at end of the running period
            return;
        }
        self.count = v;
        self.pend_count = 0;
        self.load_t = now;
        self.loaded = true;
        self.delivered = 0;
        self.triggered = false; // modes 1/5 wait for a gate trigger (ch0/1 gate tied high: never)
    }

    fn ticks_total(&mut self, now: u64) -> u64 {
        if !self.loaded {
            return 0;
        }
        self.resolve_pending(now);
        let c = self.count as u64;
        let d = Self::clocks(now.saturating_sub(self.load_t));
        match self.mode {
            0 | 4 => (d >= c) as u64,
            1 | 5 => (self.triggered && d >= c) as u64,
            _ => div(d, c),
        }
    }
}

pub struct Pit {
    pub ch: [Chan; 3],
    pub lost: u64,        // ticks dropped because the backlog cap was exceeded
    pub taken: u64,       // ticks handed out
    pub late_max_us: u64, // worst (hand-out time - due time)
    pub late_hist: [u64; 8], // <64us <256us <1ms <2ms <4ms <8ms <16ms >=16ms
    pub max_backlog: u64, // cap in ticks (0 = default 4096)
}

impl Default for Pit {
    fn default() -> Self { Self::new() }
}

impl Pit {
    pub const fn new() -> Pit {
        Pit { ch: [Chan::new(true), Chan::new(true), Chan::new(false)], lost: 0, taken: 0, late_max_us: 0, late_hist: [0; 8], max_backlog: 0 }
    }

    pub fn write(&mut self, now: u64, addr: u32, val: u32) {
        let val = val as u8;
        if addr & 3 == 3 {
            let sel = val >> 6;
            if sel == 3 {
                for i in 0..3 {
                    if val & (2 << i) != 0 {
                        let c = &mut self.ch[i];
                        if val & 0x20 == 0 && c.count_latched == 0 {
                            c.latched = c.shown(now);
                            c.count_latched = c.rw;
                            c.read_msb_next = false;
                        }
                        if val & 0x10 == 0 && !c.status_latched {
                            let o = c.out(now) as u8;
                            c.status = (o << 7) | ((c.null_count as u8) << 6) | (c.rw << 4) | (c.mode << 1) | c.bcd as u8;
                            c.status_latched = true;
                        }
                    }
                }
            } else {
                let c = &mut self.ch[sel as usize % 3];
                let acc = (val >> 4) & 3;
                if acc == 0 {
                    if c.count_latched == 0 {
                        c.latched = c.shown(now);
                        c.count_latched = c.rw;
                        c.read_msb_next = false;
                    }
                } else {
                    c.frozen = c.counter(now) as u16;
                    c.rw = acc;
                    c.read_msb_next = false;
                    c.count_latched = 0;
                    c.write_hi_next = false;
                    c.mode = match (val >> 1) & 7 { 6 => 2, 7 => 3, m => m };
                    c.bcd = val & 1 != 0;
                    c.loaded = false;
                    c.pend_count = 0;
                    c.triggered = false;
                    c.null_count = true;
                    c.delivered = 0;
                }
            }
            return;
        }
        let c = &mut self.ch[(addr & 3) as usize % 3];
        match c.rw {
            1 => c.load_count(now, val as u32),
            2 => c.load_count(now, (val as u32) << 8),
            _ => {
                if !c.write_hi_next {
                    c.write_lo = val;
                    c.write_hi_next = true;
                } else {
                    c.write_hi_next = false;
                    let lo = c.write_lo as u32;
                    c.load_count(now, lo | ((val as u32) << 8));
                }
            }
        }
    }

    pub fn read(&mut self, now: u64, addr: u32) -> u32 {
        let c = &mut self.ch[(addr & 3) as usize % 3];
        if c.status_latched {
            c.status_latched = false;
            return c.status as u32;
        }
        if c.count_latched != 0 {
            return match c.count_latched {
                1 => { c.count_latched = 0; (c.latched & 0xff) as u32 }
                2 => { c.count_latched = 0; (c.latched >> 8) as u32 }
                _ => {
                    if !c.read_msb_next { c.read_msb_next = true; (c.latched & 0xff) as u32 }
                    else { c.read_msb_next = false; c.count_latched = 0; (c.latched >> 8) as u32 }
                }
            };
        }
        let v = c.shown(now);
        match c.rw {
            1 => (v & 0xff) as u32,
            2 => (v >> 8) as u32,
            _ => {
                if !c.read_msb_next { c.read_msb_next = true; (v & 0xff) as u32 }
                else { c.read_msb_next = false; (v >> 8) as u32 }
            }
        }
    }

    pub fn out(&mut self, now: u64, i: usize) -> bool { self.ch[i % 3].out(now) }

    pub fn set_gate(&mut self, now: u64, i: usize, g: bool) {
        let c = &mut self.ch[i % 3];
        if g && !c.gate {
            match c.mode {
                1 | 5 => { c.triggered = true; c.load_t = now; c.delivered = 0; }
                2 | 3 => {
                    c.load_t = now;
                    c.delivered = 0;
                    if c.pend_count != 0 { c.count = c.pend_count; c.pend_count = 0; }
                }
                _ => {}
            }
        }
        c.gate = g;
    }

    /// Channel-0 IRQ edges due and not yet handed out.
    pub fn due(&mut self, now: u64) -> u64 {
        let c = &mut self.ch[0];
        c.ticks_total(now).saturating_sub(c.delivered)
    }

    /// Hand out one edge; applies the backlog cap (lost ticks are counted, never silent).
    pub fn take_one(&mut self, now: u64) -> bool {
        let cap = if self.max_backlog != 0 { self.max_backlog } else { 4096 };
        let total = self.ch[0].ticks_total(now);
        let d = total.saturating_sub(self.ch[0].delivered);
        if d == 0 {
            return false;
        }
        if d > cap {
            let drop = d - cap;
            self.ch[0].delivered += drop;
            self.lost += drop;
        }
        let due_t = self.next_deadline(now);
        self.ch[0].delivered += 1;
        self.taken += 1;
        if due_t != u64::MAX {
            let late = now.saturating_sub(due_t);
            if late > self.late_max_us { self.late_max_us = late; }
            let b = match late { 0..=63 => 0, 64..=255 => 1, 256..=999 => 2, 1000..=1999 => 3, 2000..=3999 => 4, 4000..=7999 => 5, 8000..=15999 => 6, _ => 7 };
            self.late_hist[b & 7] += 1;
        }
        true
    }

    /// Guest time (us) at which the next channel-0 edge is due; u64::MAX when none.
    pub fn next_deadline(&mut self, now: u64) -> u64 {
        let c = &mut self.ch[0];
        if !c.loaded {
            return u64::MAX;
        }
        c.resolve_pending(now);
        let cnt = c.count as u64;
        let k = match c.mode {
            0 | 4 => { if c.delivered >= 1 { return u64::MAX; } 1 }
            1 | 5 => { if !c.triggered || c.delivered >= 1 { return u64::MAX; } 1 }
            _ => c.delivered + 1,
        };
        let clk = k.saturating_mul(cnt);
        c.load_t.saturating_add((clk.saturating_mul(1_000_000).saturating_add(PIT_HZ - 1)) / PIT_HZ)
    }

    pub fn initial_count(&self, i: usize) -> u32 { self.ch[i % 3].count }
    pub fn mode(&self, i: usize) -> u8 { self.ch[i % 3].mode }
    pub fn gate(&self, i: usize) -> bool { self.ch[i % 3].gate }
}

// ---- B61 snapshot: symmetric (de)serializer, little endian, fixed width, no pointers ----
pub struct Io { pub p: *mut u8, pub pos: usize, pub cap: usize, pub rd: bool, pub err: bool }
impl Io {
    fn raw(&mut self, b: &mut [u8]) {
        if self.err || self.pos + b.len() > self.cap { self.err = true; if self.rd { for x in b.iter_mut() { *x = 0 } } return; }
        unsafe {
            if self.rd { core::ptr::copy_nonoverlapping(self.p.add(self.pos), b.as_mut_ptr(), b.len()); }
            else { core::ptr::copy_nonoverlapping(b.as_ptr(), self.p.add(self.pos), b.len()); }
        }
        self.pos += b.len();
    }
    pub fn u64(&mut self, x: &mut u64) { let mut b = x.to_le_bytes(); self.raw(&mut b); if self.rd { *x = u64::from_le_bytes(b) } }
    pub fn u32(&mut self, x: &mut u32) { let mut b = x.to_le_bytes(); self.raw(&mut b); if self.rd { *x = u32::from_le_bytes(b) } }
    pub fn u16(&mut self, x: &mut u16) { let mut b = x.to_le_bytes(); self.raw(&mut b); if self.rd { *x = u16::from_le_bytes(b) } }
    pub fn u8(&mut self, x: &mut u8) { let mut b = [*x]; self.raw(&mut b); if self.rd { *x = b[0] } }
    pub fn bool(&mut self, x: &mut bool) { let mut v = *x as u8; self.u8(&mut v); if self.rd { *x = v != 0 } }
}
impl Chan {
    pub fn snap(&mut self, io: &mut Io) {
        io.u32(&mut self.count); io.u64(&mut self.load_t); io.u32(&mut self.pend_count); io.u16(&mut self.latched);
        io.u8(&mut self.count_latched); io.bool(&mut self.read_msb_next); io.u8(&mut self.status);
        io.bool(&mut self.status_latched); io.u8(&mut self.rw); io.bool(&mut self.write_hi_next);
        io.u8(&mut self.write_lo); io.u8(&mut self.mode); io.bool(&mut self.bcd); io.bool(&mut self.gate);
        io.bool(&mut self.loaded); io.bool(&mut self.triggered); io.bool(&mut self.null_count);
        io.u16(&mut self.frozen); io.u64(&mut self.delivered);
        if io.rd && (self.mode > 5 || self.count == 0 || self.count > 65536) { io.err = true; }
    }
}
impl Pit {
    pub fn snap(&mut self, io: &mut Io) {
        for c in self.ch.iter_mut() { c.snap(io); }
        io.u64(&mut self.lost); io.u64(&mut self.taken); io.u64(&mut self.late_max_us);
        for h in self.late_hist.iter_mut() { io.u64(h); }
        io.u64(&mut self.max_backlog);
    }
}

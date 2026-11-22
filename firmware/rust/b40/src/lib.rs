//! B40 merged (RED shell + BLUE channel model).
//! pit.rs = BLUE's datasheet 8254 (BCD, odd mode 3, status, write-while-running, tick-count backlog cap, lateness
//! histogram), no panic paths. Timer = RED's delivery layer: injected host clock + warp, IRQ0 raised only when the PIC
//! is free (IRR|ISR), catch-up paced >= period/4, poll fast path = one compare, exact next-deadline for HLT sleep.
#![cfg_attr(any(feature = "board", feature = "fw"), no_std)]

#[cfg(feature = "capi")]
pub mod capi;
#[cfg(feature = "board")]
pub mod board;
pub mod pit;
pub use pit::{Pit, PIT_HZ};

/// Warp accounting (ns), spin statistics. Single emulation thread only (documented contract).
pub struct Warp {
    pub warp_ns: u64,
    pub skipped_iters: u64,
    pub collapses: u64,
}

impl Warp {
    pub const fn new() -> Warp { Warp { warp_ns: 0, skipped_iters: 0, collapses: 0 } }

    #[inline]
    pub fn gt_us(&self, wall_us: u64) -> u64 { wall_us.saturating_add(self.warp_ns / 1000) }

    /// Collapse up to `want` iterations of a self-loop running at `v` iterations per us, never past the next
    /// PIT edge. Returns iterations skipped; guest time has been advanced by exactly that many iterations.
    pub fn collapse(&mut self, pit: &mut Pit, wall_us: u64, want: u32, v: u32) -> u32 {
        if want == 0 || v == 0 { return 0; }
        let now = self.gt_us(wall_us);
        let dl = pit.next_deadline(now);
        let max_k = if dl == u64::MAX { u64::MAX } else { dl.saturating_sub(now).saturating_mul(v as u64) };
        let k = (want as u64).min(max_k) as u32;
        if k == 0 { return 0; }
        // exact: when k == max_k the time charged is whole us, so gt lands on the deadline
        self.warp_ns = self.warp_ns.saturating_add(((k as u64) * 1000).checked_div(v as u64).unwrap_or(0));
        self.skipped_iters += k as u64;
        self.collapses += 1;
        k
    }
}


/// Delivery layer over `Pit`. `now` arguments are host microseconds (u64); guest time = now + warp_us.
pub struct Timer {
    pub pit: Pit,
    pub warp_us: u64,
    last_raise_us: u64,
    vnow: u64,
    dirty: bool,
    next_virt_us: u64, // guest us of the next unraised ch0 edge (u64::MAX none)
}

impl Default for Timer { fn default() -> Self { Self::new() } }

impl Timer {
    pub const fn new() -> Timer {
        Timer { pit: Pit::new(), warp_us: 0, last_raise_us: 0, vnow: 0, dirty: true, next_virt_us: 0 }
    }
    #[inline] fn v(&self, now: u64) -> u64 { now.wrapping_add(self.warp_us) }

    /// power-on state like the reference reset: all channels mode 3 count 65536 (18.2 Hz on ch0)
    pub fn reset(&mut self, now: u64) {
        let w = self.warp_us;
        *self = Timer::new();
        self.warp_us = w;
        let t = self.v(now);
        for ch in 0..3u32 {
            self.pit.write(t, 3, (ch << 6) | (3 << 4) | (3 << 1));
            self.pit.write(t, ch, 0);
            self.pit.write(t, ch, 0);
        }
    }
    pub fn write(&mut self, now: u64, port: u32, val: u32) { self.dirty = true; let t = self.v(now); self.pit.write(t, port, val) }
    pub fn read(&mut self, now: u64, port: u32) -> u32 { let t = self.v(now); self.pit.read(t, port) }
    pub fn out(&mut self, now: u64, c: usize) -> bool { let t = self.v(now); self.pit.out(t, c) }
    pub fn gate(&self, c: usize) -> bool { self.pit.gate(c) }
    pub fn set_gate(&mut self, now: u64, c: usize, g: bool) { self.dirty = true; let t = self.v(now); self.pit.set_gate(t, c, g) }
    pub fn initial_count(&self, c: usize) -> u32 { self.pit.initial_count(c) }
    pub fn mode(&self, c: usize) -> u8 { self.pit.mode(c) }
    /// B14: guest time jumps forward without host time passing
    pub fn warp(&mut self, us: u64) { self.warp_us = self.warp_us.wrapping_add(us); self.dirty = true; }
    pub fn lost(&self) -> u64 { self.pit.lost }

    /// ticks due and not yet handed out (host clock)
    pub fn owed(&mut self, now: u64) -> u64 { let t = self.v(now); self.vnow = t; self.pit.due(t) }
    /// hand out one tick (at the time of the last owed() call); test helper, the glue uses poll_raise
    pub fn take_tick(&mut self) { let t = self.vnow; self.pit.take_one(t); }

    /// Host time (us) of the next unraised ch0 edge (exact HLT sleep); None = none ever.
    pub fn next_deadline(&mut self, now: u64) -> Option<u64> {
        let t = self.v(now);
        let d = self.pit.next_deadline(t);
        if d == u64::MAX { None } else { Some(d.wrapping_sub(self.warp_us)) }
    }

    /// One call per poll. Raises the next IRQ0 if one is due, the PIC is free (`busy` = IRR|ISR of IRQ0) and, when
    /// catching up a backlog, at least period/4 of host time has passed since the last raise. Without pacing a backlog
    /// nests IRQ0 inside the still-running Win95 tick handler (EOI, STI) and overflows its ring-0 stack.
    pub fn poll_raise(&mut self, now: u64, busy: bool) -> bool {
        let t = self.v(now);
        if !self.dirty && t < self.next_virt_us { return false; } // nothing due: one compare
        let r = self.slow(now, t, busy);
        self.next_virt_us = self.pit.next_deadline(t);
        self.dirty = false;
        r
    }
    fn slow(&mut self, now: u64, t: u64, busy: bool) -> bool {
        let due = self.pit.due(t);
        if due == 0 || busy { return false; }
        if due > 1 {
            let period_us = self.pit.ch[0].count as u64 * 1_000_000 / PIT_HZ;
            if now.wrapping_sub(self.last_raise_us) < period_us / 4 { return false; }
        }
        self.last_raise_us = now;
        self.pit.take_one(t)
    }
}

impl Timer {
    /// B61: save/restore the whole 8254 + delivery layer. `dt` (restore only) = host clock now minus host clock at
    /// snapshot (u64 wrapping): guest time `now + warp` is kept continuous by warp -= dt, so every channel phase,
    /// the owed-tick backlog and the PIT counters read exactly as they did when the snapshot was taken.
    pub fn snap(&mut self, io: &mut pit::Io, dt: u64, norm_now: Option<u64>) {
        self.pit.snap(io);
        // digest mode (norm_now = host clock now): write guest clock offset and age of the last raise, both host-time independent
        let (mut w, mut lr) = match norm_now { Some(n) => (self.warp_us.wrapping_add(n), n.wrapping_sub(self.last_raise_us)), None => (self.warp_us, self.last_raise_us) };
        io.u64(&mut w); io.u64(&mut lr); io.u64(&mut self.vnow);
        io.u64(&mut self.next_virt_us);
        if io.rd { self.warp_us = w; self.last_raise_us = lr; }
        if io.rd {
            self.warp_us = self.warp_us.wrapping_sub(dt);
            self.last_raise_us = self.last_raise_us.wrapping_add(dt);
            self.dirty = true;
        }
    }
}

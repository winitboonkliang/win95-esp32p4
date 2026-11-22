//! P4 board side (no_std): vector replay, hostile sequences and cycle costs of the Rust PIT, called from the IDF harness.
use crate::Timer as Pit;

#[panic_handler]
fn panic(_: &core::panic::PanicInfo) -> ! { loop {} }

extern "C" { fn b40_cyc() -> u32; }

static mut PIT: Pit = Pit::new();
fn pit() -> &'static mut Pit { unsafe { &mut *core::ptr::addr_of_mut!(PIT) } }

#[repr(C)]
pub struct VecRes { pub n: u32, pub fails: u32, pub irq_n: u32, pub irq_fails: u32 }

fn prog(p: &mut Pit, now: u64, ch: u32, mode: u32, count: u32) {
    p.write(now, 0x43, (ch << 6) | (3 << 4) | (mode << 1));
    p.write(now, 0x40 + ch, count & 255);
    p.write(now, 0x40 + ch, count >> 8);
}
fn word(p: &mut Pit, now: u64) -> u32 {
    p.write(now, 0x43, 0);
    let lo = p.read(now, 0x40);
    lo | (p.read(now, 0x40) << 8)
}
fn nums(line: &[u8], out: &mut [u64; 4]) -> usize {
    let (mut n, mut i) = (0, 0);
    while i < line.len() && n < 4 {
        while i < line.len() && line[i] == b' ' { i += 1; }
        if i >= line.len() { break; }
        let mut v = 0u64; let mut any = false;
        while i < line.len() && line[i].is_ascii_digit() { v = v * 10 + (line[i] - b'0') as u64; i += 1; any = true; }
        if !any { return 0; }
        out[n] = v; n += 1;
    }
    n
}

/// Replays pit_vectors.txt (counter reads incl. 6/7 aliases; IRQ0 counts for modes 2,3,6,7) with an injected clock.
#[no_mangle]
pub unsafe extern "C" fn b40_board_vectors(txt: *const u8, len: usize, r: *mut VecRes) {
    let text = core::slice::from_raw_parts(txt, len);
    let r = &mut *r;
    *r = VecRes { n: 0, fails: 0, irq_n: 0, irq_fails: 0 };
    for line in text.split(|&c| c == b'\n') {
        let line = if line.last() == Some(&b'\r') { &line[..line.len() - 1] } else { line };
        if line.is_empty() || line[0] == b'#' { continue; }
        let mut f = [0u64; 4];
        if line.starts_with(b"IRQ0 ") {
            if nums(&line[5..], &mut f) < 3 { continue; }
            let (count, t, _want) = (f[0] as u32, f[1], f[2]);
            let want = (t * 1193182).checked_div(count as u64 * 1_000_000).unwrap_or(0);
            for mode in [2u32, 3, 6, 7] {
                let p = pit();
                *p = Pit::new();
                prog(p, 1000, 0, mode, count);
                let (mut k, mut now) = (0u64, 1000u64);
                while now <= 1000 + t { while p.owed(now) > 0 { p.take_tick(); k += 1; } now += 997; }
                while p.owed(1000 + t) > 0 { p.take_tick(); k += 1; }
                r.irq_n += 1;
                if !(k == want || k + 1 == want) { r.irq_fails += 1; }
            }
        } else if nums(line, &mut f) == 4 {
            let (mode, count, t, exp) = (f[0] as u32, f[1] as u32, f[2], f[3] as i64);
            let p = pit();
            *p = Pit::new();
            let t0 = 4_294_000_000u64; // crosses 2^32 us for the long vectors
            prog(p, t0, 0, mode, count);
            let got = word(p, t0 + t) as i64;
            let d = (got - exp).abs().min((got - exp + 65536).abs()).min((got - exp - 65536).abs());
            r.n += 1;
            if d > 1 { r.fails += 1; }
        }
    }
}

/// 256 control words x counts {0,1,2,3,65535} x 20 ms with reads/latches/gates; returns the number of runs completed.
#[no_mangle]
pub unsafe extern "C" fn b40_board_hostile() -> u32 {
    let counts = [0u32, 1, 2, 3, 65535];
    let mut runs = 0;
    for cw in 0..256u32 {
        for &cnt in &counts {
            let p = pit();
            *p = Pit::new();
            let mut t = 1u64 << 31;
            p.write(t, 0x43, cw);
            for ch in 0..3 { p.write(t, 0x40 + ch, cnt & 255); p.write(t, 0x40 + ch, cnt >> 8); }
            for c in 0..3 { p.set_gate(t, c, false); p.set_gate(t + 5, c, true); }
            let end = t + 20_000;
            while t < end {
                if p.owed(t) > 0 { p.take_tick(); }
                for port in 0..4 { p.read(t, 0x40 + port); }
                p.write(t, 0x43, 0xC2 | (cw & 0x3d));
                p.out(t, 0); p.out(t, 2); p.next_deadline(t);
                t += 997;
            }
            runs += 1;
        }
    }
    runs
}

#[inline(never)]
fn timed<F: FnMut()>(mut f: F, n: u32) -> u32 {
    let a = unsafe { b40_cyc() };
    for _ in 0..n { f(); }
    (unsafe { b40_cyc() }.wrapping_sub(a)).checked_div(n).unwrap_or(0)
}

/// out[0] = cycles per latch+2 reads, out[1] = per owed() with nothing due, out[2] = per owed()+take with a tick due
#[no_mangle]
pub unsafe extern "C" fn b40_board_cost(out: *mut u32) {
    let p = pit();
    *p = Pit::new();
    prog(p, 0, 0, 2, 16384);
    let mut t = 100u64;
    let o = core::slice::from_raw_parts_mut(out, 3);
    o[0] = timed(|| { t += 7; core::hint::black_box(word(pit(), t)); }, 2000);
    let mut t2 = 20u64;
    o[1] = timed(|| { t2 += 0; core::hint::black_box(pit().poll_raise(t2, false)); }, 2000);
    let mut t3 = 0u64;
    o[2] = timed(|| { t3 += 13_733; core::hint::black_box(pit().poll_raise(t3, false)); }, 2000);
}

// ---- model API for the C stall test (same shape as capi, explicit clock) ----
#[no_mangle] pub unsafe extern "C" fn b40n_reset(now: u64, count: u32) { let p = pit(); *p = Pit::new(); prog(p, now, 0, 2, count); }
#[no_mangle] pub unsafe extern "C" fn b40n_owed(now: u64) -> u32 { pit().owed(now) as u32 }
#[no_mangle] pub unsafe extern "C" fn b40n_take() { pit().take_tick() }
/// glue-shaped poll: returns 1 if a tick was raised
#[no_mangle] pub unsafe extern "C" fn b40n_poll(now: u64, busy: u32) -> u32 { pit().poll_raise(now, busy != 0) as u32 }
#[no_mangle] pub unsafe extern "C" fn b40n_next_deadline(now: u64) -> u64 { pit().next_deadline(now).unwrap_or(u64::MAX) }

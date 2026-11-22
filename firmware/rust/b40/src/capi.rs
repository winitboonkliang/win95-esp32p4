//! C ABI replacing base/i8254.c. One PIT per machine (static storage, no allocator).
use crate::Timer;
use core::ffi::{c_int, c_void};

extern "C" {
    fn get_uticks() -> u32;
    /// new in i8259.c: 1 if IRQ n is pending (IRR) or in service (ISR)
    fn i8259_irq_busy(pic: *mut c_void, irq: c_int) -> c_int;
}
type SetIrq = unsafe extern "C" fn(*mut c_void, c_int, c_int);

static mut PIT: Timer = Timer::new();
static mut PIC: *mut c_void = core::ptr::null_mut();
static mut SETIRQ: Option<SetIrq> = None;
static mut HI: u64 = 0;
static mut LAST: u32 = 0;
pub static mut STAT_RAISED: u64 = 0;
static mut STAT_SKIPPED: u32 = 0;

/// 32-bit get_uticks() -> monotonic u64 (valid while polled at least once per 71 min).
unsafe fn now() -> u64 {
    let t = get_uticks();
    if t < LAST { HI += 1 << 32; }
    LAST = t;
    HI + t as u64
}

#[no_mangle]
pub unsafe extern "C" fn i8254_init(irq: c_int, pic: *mut c_void, set_irq: SetIrq) -> *mut Timer {
    let _ = irq;
    PIC = pic;
    SETIRQ = Some(set_irq);
    let p = core::ptr::addr_of_mut!(PIT);
    (*p).reset(now());
    p
}
#[no_mangle]
pub unsafe extern "C" fn i8254_update_irq(p: *mut Timer) {
    let t = now();
    let busy = i8259_irq_busy(PIC, 0) != 0;
    if (*p).poll_raise(t, busy) {
        if TL_ARMED && TL_SKIP + 1 < TL_CAP {
            TL_SKIP += 1; STAT_SKIPPED += 1; // B41b tickless: the guest idle VxD credits this period itself on wake
            return;
        }
        TL_ARMED = false; // a delivered tick ends the sleep; the guest takes the count after HLT
        if let Some(f) = SETIRQ { f(PIC, 0, 1); f(PIC, 0, 0); }
        STAT_RAISED += 1;
    }
}
// B41b (RED) tickless idle, same contract as base i8254.c (port 0x5B4 in pc.c): arm = deliver the cap-th edge, take = edges skipped.
static mut TL_ARMED: bool = false;
static mut TL_SKIP: u32 = 0;
static mut TL_CAP: u32 = 8;
#[no_mangle]
pub unsafe extern "C" fn i8254_tl_arm(_p: *mut Timer, cap: u32) { TL_ARMED = true; TL_SKIP = 0; TL_CAP = if cap == 0 { 8 } else { cap }; }
#[no_mangle]
pub unsafe extern "C" fn i8254_tl_take(_p: *mut Timer) -> u32 { let k = TL_SKIP; TL_ARMED = false; TL_SKIP = 0; k }
#[no_mangle]
pub unsafe extern "C" fn b41b_tl_stats() -> u32 { STAT_SKIPPED }

#[no_mangle] pub unsafe extern "C" fn i8254_ioport_read(p: *mut Timer, a: u32) -> u32 { (*p).read(now(), a) }
#[no_mangle] pub unsafe extern "C" fn i8254_ioport_write(p: *mut Timer, a: u32, v: u32) { (*p).write(now(), a, v) }
#[no_mangle] pub unsafe extern "C" fn pit_get_out(p: *mut Timer, c: c_int) -> c_int { (*p).out(now(), (c as usize) % 3) as c_int }
#[no_mangle] pub unsafe extern "C" fn pit_get_gate(p: *mut Timer, c: c_int) -> c_int { (*p).gate((c as usize) % 3) as c_int }
#[no_mangle] pub unsafe extern "C" fn pit_set_gate(p: *mut Timer, c: c_int, v: c_int) { (*p).set_gate(now(), (c as usize) % 3, v != 0) }
#[no_mangle] pub unsafe extern "C" fn pit_get_initial_count(p: *mut Timer, c: c_int) -> c_int { (*p).initial_count((c as usize) % 3) as c_int }
#[no_mangle] pub unsafe extern "C" fn pit_get_mode(p: *mut Timer, c: c_int) -> c_int { (*p).mode((c as usize) % 3) as c_int }
/// B14: guest time jumps forward by `us` (collapsed delay loop)
#[no_mangle] pub unsafe extern "C" fn b40_warp_us(us: u64) { (*core::ptr::addr_of_mut!(PIT)).warp(us) }
#[no_mangle] pub unsafe extern "C" fn b40_warp_total_us() -> u64 { (*core::ptr::addr_of_mut!(PIT)).warp_us }
#[no_mangle] pub unsafe extern "C" fn b40_dropped() -> u64 { (*core::ptr::addr_of_mut!(PIT)).lost() }
#[no_mangle] pub unsafe extern "C" fn b40_raised() -> u64 { STAT_RAISED }
/// host us until the next tick is due (0 = due now); capped at 1 s; for exact HLT sleeping
#[no_mangle]
pub unsafe extern "C" fn b40_next_tick_in_us() -> u32 {
    let n = now();
    match (*core::ptr::addr_of_mut!(PIT)).next_deadline(n) {
        Some(d) => { if d <= n { 0 } else { (d - n).min(1_000_000) as u32 } }
        None => 1_000_000,
    }
}

/// B14: collapse up to `n` iterations of a `LOOP $` delay loop. Guest PIT time advances k/ipus us (ipus = iterations
/// per microsecond the guest believes, = host LOOP speed so executed and collapsed iterations agree); the collapse stops
/// at the next IRQ0 deadline so the tick still interrupts the loop with ECX = iterations consumed (Win95 VMM calibrates
/// by interrupting such a loop; collapsing it whole gives the IOS #DE). Returns iterations skipped (0 = run normally).
#[no_mangle]
pub unsafe extern "C" fn b40_loop_collapse(n: u32, ipus: u32) -> u32 {
    if ipus == 0 { return 0; }
    let p = &mut *core::ptr::addr_of_mut!(PIT);
    let mut lim = n as u64;
    if i8259_irq_busy(PIC, 0) == 0 {
        let t = now();
        if let Some(d) = p.next_deadline(t) {
            let cap = if d <= t { 0 } else { (d - t) * ipus as u64 };
            if cap < lim { lim = cap; }
        }
    }
    let k = lim as u32;
    if k < 64 { return 0; }
    p.warp(k as u64 / ipus as u64);
    k
}

/// B44 counters: lateness histogram of IRQ0 hand-out (<64us <256us <1ms <2ms <4ms <8ms <16ms >=16ms), taken, max late us
#[no_mangle] pub unsafe extern "C" fn b40_late_hist(i: u32) -> u64 { (*core::ptr::addr_of!(PIT)).pit.late_hist[(i & 7) as usize] }
#[no_mangle] pub unsafe extern "C" fn b40_taken() -> u64 { (*core::ptr::addr_of!(PIT)).pit.taken }
#[no_mangle] pub unsafe extern "C" fn b40_late_max_us() -> u64 { (*core::ptr::addr_of!(PIT)).pit.late_max_us }

#[cfg(all(feature = "fw", not(feature = "board")))]
#[panic_handler]
fn panic(_: &core::panic::PanicInfo) -> ! { loop {} }

/// reset to power-on state keeping the PIC wiring (F26 system reset)
#[no_mangle] pub unsafe extern "C" fn i8254_reset(p: *mut Timer) { (*p).reset(now()) }

/// B61: serialize (rd=0) / restore (rd=1) the PIT into buf[0..cap]. Returns bytes used, or -1 on error.
#[no_mangle]
pub unsafe extern "C" fn b40_snap(buf: *mut u8, cap: usize, rd: c_int, dt: u64) -> c_int {
    let mut io = crate::pit::Io { p: buf, pos: 0, cap, rd: rd == 1, err: false };
    (*core::ptr::addr_of_mut!(PIT)).snap(&mut io, dt, if rd == 2 { Some(dt) } else { None });
    if io.err { -1 } else { io.pos as c_int }
}
/// B61: the 64-bit host clock the PIT works on (so the resume code can compute dt in the same base)
#[no_mangle]
pub unsafe extern "C" fn b40_now() -> u64 { now() }

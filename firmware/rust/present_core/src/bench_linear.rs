#![allow(unused_unsafe)]
//! Board bench for the linear 1280x800 surface + damage rects path (lead redirect 14 Nov): PPA rotate of changed rects only,
//! 3 fbs with swap wait, no full-frame msync. Called from fw/main/main.c.
use crate::linear::*;
use crate::*;
use core::ffi::c_void;

extern "C" {
    fn esp_rom_printf(fmt: *const u8, ...) -> i32;
    fn esp_timer_get_time() -> i64;
    fn esp_cache_msync(addr: *mut c_void, size: usize, flags: i32) -> i32;
    fn esp_lcd_panel_draw_bitmap(p: *mut c_void, x0: i32, y0: i32, x1: i32, y1: i32, c: *const c_void) -> i32;
    fn heap_caps_aligned_calloc(align: usize, n: usize, size: usize, caps: u32) -> *mut c_void;
    fn vTaskDelay(t: u32);
    static b20_vsync: u32;
    static b20_probe_chunks: u32;
    fn b20_ppa_rect(src: *const u16, x0: i32, y0: i32, x1: i32, y1: i32, dst: *mut u16) -> i32;
    fn b20_old_full(console: *mut u16, t_ppa: *mut u32, t_draw: *mut u32) -> i32;
}
macro_rules! p {
    ($f:literal $(, $a:expr)*) => { unsafe { esp_rom_printf(concat!($f, "\0").as_ptr() $(, $a)*); } };
}
const CAPS: u32 = (1 << 10) | (1 << 2);
const SYNC: i32 = (1 << 1) | (1 << 2); // C2M, unaligned ok

fn now() -> u32 { unsafe { esp_timer_get_time() as u32 } }
fn vs() -> u32 { unsafe { core::ptr::read_volatile(&b20_vsync) } }
fn probe() -> u32 { unsafe { core::ptr::read_volatile(&b20_probe_chunks) } }

fn msync_rect(fb: *mut u16, r: &Rect) {
    let (x0, x1, y0, y1) = (r.x0 as usize, r.x1 as usize, r.y0 as usize, r.y1 as usize);
    unsafe {
        // one call per rect: esp_cache_msync costs ~15 us per CALL (measured, run1), far more than the lines it walks
        esp_cache_msync(fb.add(y0 * PW + x0) as *mut c_void, ((y1 - 1 - y0) * PW + (x1 - x0)) * 2, SYNC);
    }
}

struct St {
    lp: &'static mut LinearPresenter,
    s: &'static mut [u16],
    sp: *mut u16,
    fbs: [*mut u16; 3],
    nfb: usize,
    idx: usize,
    hv: [u32; 3],
    panel: *mut c_void,
    live: bool,
    ppa_n: u32,
    out: [Rect; MAXRECT],
}

/// wait (untimed) until the buffer to write is no longer scanned, present, sync, swap. (us, rects, px, presented)
fn step(c: &mut St) -> (u32, usize, u32, bool) {
    if c.live {
        let last = (c.idx + c.nfb - 1) % c.nfb;
        let r = if c.nfb == 2 { last } else { (c.idx + c.nfb - 2) % c.nfb };
        let need = c.hv[r];
        let t = now();
        while vs() == need && now().wrapping_sub(t) < 200_000 {}
    }
    let t0 = now();
    let fb = c.fbs[c.idx];
    let back = unsafe { core::slice::from_raw_parts_mut(fb, PW * PH) };
    let sp = c.sp;
    let mut jobs = 0u32;
    let (n, px) = c.lp.present(c.s, back, |r, _| { jobs += 1; unsafe { b20_ppa_rect(sp, r.x0 as i32, r.y0 as i32, r.x1 as i32, r.y1 as i32, fb); } }, &mut c.out);
    c.ppa_n += jobs;
    if n == 0 { return (now().wrapping_sub(t0), 0, 0, false); }
    // PPA blocks are synced by the PPA driver; CPU-written small rects + pointer are synced here, rect by rect (never the whole frame)
    for i in 0..n { if c.out[i].area() < PPA_MIN_PX { msync_rect(fb, &c.out[i]); } }
    if c.live { unsafe { esp_lcd_panel_draw_bitmap(c.panel, 0, 0, PW as i32, 1, fb as *const c_void); } }
    let dt = now().wrapping_sub(t0);
    c.hv[c.idx] = vs();
    c.idx = (c.idx + 1) % c.nfb;
    (dt, n, px, true)
}

fn scen(name: &[u8], iters: usize, c: &mut St, prep: &mut dyn FnMut(usize, &mut LinearPresenter)) {
    let (mut sum, mut mx, mut pres, mut idle, mut rects, mut pxt) = (0u64, 0u32, 0u32, 0u32, 0u64, 0u64);
    let pc0 = probe();
    let p0 = c.ppa_n;
    let t_start = now();
    for i in 0..iters {
        prep(i, c.lp);
        let (dt, n, px, did) = step(c);
        if !did { idle += 1; continue; }
        pres += 1; sum += dt as u64; mx = mx.max(dt); rects += n as u64; pxt += px as u64;
    }
    let wall = now().wrapping_sub(t_start).max(1);
    let avg = if pres > 0 { (sum / pres as u64) as u32 } else { 0 };
    let fps10 = (pres as u64 * 10_000_000 / wall as u64) as u32;
    let mb10 = (probe().wrapping_sub(pc0) as u64 * 65536 * 10 / wall as u64) as u32;
    unsafe {
        esp_rom_printf(b"B20L %s presents=%u idle=%u avg_us=%u max_us=%u rects/present=%u src_px/present=%u ppa_jobs=%u wall_ms=%u fps_x10=%u probe_copy_MBps_x10=%u\n\0".as_ptr(),
            name.as_ptr(), pres, idle, avg, mx, (rects / pres.max(1) as u64) as u32, (pxt / pres.max(1) as u64) as u32, c.ppa_n - p0, wall / 1000, fps10, mb10);
    }
}

#[no_mangle]
pub extern "C" fn b20_run_linear(panel: *mut c_void, fb0: *mut u16, fb1: *mut u16, fb2: *mut u16, nfb: i32, live: i32, pclk: i32) {
    p!("B20L start linear 1280x800 nfb=%d live=%d pclk_mhz=%d\n", nfb, live, pclk);
    unsafe {
        let sp = heap_caps_aligned_calloc(128, 1, SW * SH * 2, CAPS) as *mut u16;
        let s = core::slice::from_raw_parts_mut(sp, SW * SH);
        for y in 0..SH { for x in 0..SW { s[y * SW + x] = ((x * 5 + y * 3) as u16) ^ (((x / 40) * 0x0841) as u16); } }
        esp_cache_msync(sp as *mut c_void, SW * SH * 2, SYNC);
        let lpm = heap_caps_aligned_calloc(128, 1, core::mem::size_of::<LinearPresenter>() + 64, CAPS) as *mut LinearPresenter;
        core::ptr::write(lpm, LinearPresenter::new(nfb as u8));
        let mut c = St { lp: &mut *lpm, s, sp, fbs: [fb0, fb1, fb2], nfb: nfb as usize, idx: 0, hv: [0; 3], panel, live: live != 0, ppa_n: 0, out: [Rect::default(); MAXRECT] };
        let arrow = arrow_shape();
        c.lp.set_cursor(Some(&arrow), 10, 10, true);
        c.lp.damage_all();
        for _ in 0..4 { let _ = step(&mut c); c.lp.damage_all(); }
        let _ = step(&mut c);
        let shown = (c.idx + c.nfb - 1) % c.nfb;
        p!("B20L crc_full_ptr_10_10=%x\n", crc32_u16(core::slice::from_raw_parts(c.fbs[shown], PW * PH), 0));
        scen(b"idle_100\0", 100, &mut c, &mut |_, _| {});
        scen(b"pointer_sweep_400\0", 400, &mut c, &mut |i, lp| {
            let t = i as i32;
            lp.set_cursor(None, 40 + (t * 37) % 1200, 40 + (t * 23) % 700, true);
        });
        for &(name, w, h) in &[(&b"dirty_64x48\0"[..], 64usize, 48usize), (&b"dirty_200x150\0"[..], 200, 150), (&b"dirty_400x300\0"[..], 400, 300), (&b"dirty_800x600\0"[..], 800, 600), (&b"full_1280x800\0"[..], 1280, 800)] {
            scen(name, 30, &mut c, &mut |i, lp| {
                let x0 = ((i * 53) % (1280 - w + 1)) as u16;
                let y0 = ((i * 29) % (800 - h + 1)) as u16;
                lp.damage(&Rect { x0, y0, x1: x0 + w as u16, y1: y0 + h as u16 });
            });
        }
        if live != 0 {
            let (mut sp_, mut sd_) = (0u64, 0u64);
            let pc0 = probe();
            let t_start = now();
            for _ in 0..20 {
                let (mut tp, mut td) = (0u32, 0u32);
                let t0 = now();
                b20_old_full(sp, &mut tp, &mut td);
                sp_ += tp as u64; sd_ += td as u64;
                while now().wrapping_sub(t0) < 100_000 {}
            }
            let wall = now().wrapping_sub(t_start);
            p!("B20L legacy_old_full_ppa presents=20 ppa_us=%u draw_us=%u wall_ms=%u probe_copy_MBps_x10=%u\n", (sp_ / 20) as u32, (sd_ / 20) as u32, wall / 1000, (probe().wrapping_sub(pc0) as u64 * 65536 * 10 / wall as u64) as u32);
        }
        let pc0 = probe();
        let t0 = now();
        vTaskDelay(1000);
        let wall = now().wrapping_sub(t0);
        p!("B20L probe_floor_idle probe_copy_MBps_x10=%u\n", (probe().wrapping_sub(pc0) as u64 * 65536 * 10 / wall as u64) as u32);
        p!("B20L done\n");
    }
}

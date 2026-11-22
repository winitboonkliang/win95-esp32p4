#![allow(unused_unsafe)]
//! Board bench (ESP32-P4, no_std). Called from fw/main/main.c after the panel is up.
//! Prints `B20 ...` lines over the console. IDF functions are declared by hand; everything else is this crate.
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
    static b20_probe_chunks: u32; // 64 KiB PSRAM->PSRAM copy chunks done by the core-1 probe task
    // legacy path (old port reproduced in C, fw/main/old_path.c)
    fn b20_old_present(fb: *mut u16, snap: *const u8, t_conv_us: *mut u32, t_ppa_us: *mut u32, t_draw_us: *mut u32) -> i32;
}
macro_rules! p {
    ($f:literal $(, $a:expr)*) => { unsafe { esp_rom_printf(concat!($f, "\0").as_ptr() $(, $a)*); } };
}
const CAPS: u32 = (1 << 10) | (1 << 2); // MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
const MSYNC_C2M_UNALIGNED: i32 = (1 << 1) | (1 << 2);

fn now() -> u32 { unsafe { esp_timer_get_time() as u32 } }
fn vs() -> u32 { unsafe { core::ptr::read_volatile(&b20_vsync) } }
fn probe() -> u32 { unsafe { core::ptr::read_volatile(&b20_probe_chunks) } }

struct Snap { w: usize, h: usize, line: usize, pal: [u16; 16], data: *const u8 }
fn parse_snap(p: *const u8) -> Snap {
    unsafe {
        let r = |o: usize| u32::from_le_bytes([*p.add(o), *p.add(o + 1), *p.add(o + 2), *p.add(o + 3)]) as usize;
        let mut pal = [0u16; 16];
        for i in 0..16 { pal[i] = u16::from_le_bytes([*p.add(16 + 2 * i), *p.add(17 + 2 * i)]); }
        Snap { w: r(4), h: r(8), line: r(12), pal, data: p.add(64) }
    }
}

#[derive(Default, Clone, Copy)]
struct Acc { n: u32, sum: u64, max: u32, src: u64, panel: u64, rects: u64, presents: u32, idle: u32 }

struct Ctx<'a> {
    pr: &'a mut Presenter<'a>,
    panel: *mut c_void,
    fbs: [*mut u16; 3],
    nfb: usize,
    idx: usize,
    hist_vs: [u32; 3],
    ro: [Rect; MAXRECT],
    live: bool,
}

fn msync_rect(fb: *mut u16, r: &Rect) {
    if r.is_empty() { return; }
    let (x0, x1, y0, y1) = (r.x0 as usize, r.x1 as usize, r.y0 as usize, r.y1 as usize);
    unsafe {
        if (x1 - x0) * 2 * 4 < PW * 2 {
            // narrow: per row
            for y in y0..y1 { esp_cache_msync(fb.add(y * PW + x0) as *mut c_void, (x1 - x0) * 2, MSYNC_C2M_UNALIGNED); }
        } else {
            esp_cache_msync(fb.add(y0 * PW + x0) as *mut c_void, ((y1 - 1 - y0) * PW + (x1 - x0)) * 2, MSYNC_C2M_UNALIGNED);
        }
    }
}

/// one present + swap. returns (report, compute_us) ; waits (outside the timing) until the target buffer is free
fn do_present(c: &mut Ctx, snap: &Snap) -> (Report, u32) {
    let t0 = now();
    let back = unsafe { core::slice::from_raw_parts_mut(c.fbs[c.idx], PW * PH) };
    let (w, h) = (snap.w, snap.h);
    let pal = snap.pal;
    let (line, data) = (snap.line, snap.data);
    let vram = unsafe { core::slice::from_raw_parts(data, h * line) };
    let (rep, n) = c.pr.present(|cache, r| planar16_to_cache(vram, 0, line, &pal, cache, h, r), back, &mut c.ro);
    let _ = w;
    if rep.idle { return (rep, now().wrapping_sub(t0)); }
    for i in 0..n { msync_rect(c.fbs[c.idx], &c.ro[i]); }
    if c.live {
        unsafe { esp_lcd_panel_draw_bitmap(c.panel, 0, 0, PW as i32, 1, c.fbs[c.idx] as *const c_void); }
    }
    let dt = now().wrapping_sub(t0);
    c.hist_vs[c.idx] = vs();
    c.idx = (c.idx + 1) % c.nfb;
    (rep, dt)
}

/// block (outside the measured time) until the buffer we are about to write is no longer scanned
fn wait_free(c: &Ctx) {
    if !c.live { return; }
    // buffer c.idx was last handed over nfb presents ago; it is safe once one refresh completed after the hand-over of the NEWER
    // buffers: nfb==2 -> after the last hand-over, nfb==3 -> after the hand-over before the last one.
    let last = (c.idx + c.nfb - 1) % c.nfb;
    let ref_idx = if c.nfb == 2 { last } else { (c.idx + c.nfb - 2) % c.nfb };
    let need = c.hist_vs[ref_idx];
    let t = now();
    while vs() == need && now().wrapping_sub(t) < 200_000 {}
}

fn report(name: &[u8], a: &Acc, wall_us: u32, probe_chunks: u32) {
    let avg = if a.presents > 0 { (a.sum / a.presents as u64) as u32 } else { 0 };
    let fps10 = if wall_us > 0 { (a.presents as u64 * 10_000_000 / wall_us as u64) as u32 } else { 0 };
    let mbps10 = if wall_us > 0 { (probe_chunks as u64 * 65536 * 10 / wall_us as u64) as u32 } else { 0 }; // bytes/us = MB/s, x10
    unsafe {
        esp_rom_printf(b"B20 %s presents=%u idle=%u avg_us=%u max_us=%u src_B/present=%u panel_B/present=%u rects/present=%u wall_ms=%u fps_x10=%u probe_copy_MBps_x10=%u\n\0".as_ptr(),
            name.as_ptr(), a.presents, a.idle, avg, a.max,
            (a.src / a.presents.max(1) as u64) as u32, (a.panel / a.presents.max(1) as u64) as u32, (a.rects / a.presents.max(1) as u64) as u32,
            wall_us / 1000, fps10, mbps10);
    }
}

fn run_loop<F: FnMut(usize, &mut Ctx)>(name: &[u8], iters: usize, c: &mut Ctx, snaps: [&Snap; 2], which: &mut usize, mut prep: F) {
    let mut a = Acc::default();
    let pc0 = probe();
    let t_start = now();
    for i in 0..iters {
        prep(i, c);
        wait_free(c);
        let (rep, dt) = do_present(c, snaps[*which]);
        if rep.idle { a.idle += 1; continue; }
        a.presents += 1; a.sum += dt as u64; a.max = a.max.max(dt);
        a.src += rep.src_bytes as u64; a.panel += rep.panel_bytes as u64; a.rects += rep.panel_rects as u64;
    }
    let wall = now().wrapping_sub(t_start);
    report(name, &a, wall, probe().wrapping_sub(pc0));
}

fn selftest(pr: &Presenter) -> u32 {
    // asm kernel vs reference kernel on random rects / mode geometries
    let mut bad = 0u32;
    let mut seed = 12345u32;
    let mut rnd = || { seed = seed.wrapping_mul(1664525).wrapping_add(1013904223); seed >> 8 };
    let (w, h) = (pr.maps.w as usize, pr.maps.h as usize);
    unsafe {
        let a = heap_caps_aligned_calloc(128, 1, PW * PH * 2, CAPS) as *mut u16;
        let b = heap_caps_aligned_calloc(128, 1, PW * PH * 2, CAPS) as *mut u16;
        let sa = core::slice::from_raw_parts_mut(a, PW * PH);
        let sb = core::slice::from_raw_parts_mut(b, PW * PH);
        let cache = core::slice::from_raw_parts(pr.cache.as_ptr(), w * h);
        for _ in 0..300 {
            let x0 = rnd() as usize % PW; let y0 = rnd() as usize % PH;
            let x1 = (x0 + 1 + rnd() as usize % 300).min(PW); let y1 = (y0 + 1 + rnd() as usize % 200).min(PH);
            let r = Rect { x0: x0 as u16, y0: y0 as u16, x1: x1 as u16, y1: y1 as u16 };
            kern::expand_rect_ref(cache, h, &pr.maps.xmap, &pr.maps.yrev, &r, sa, PW);
            kern::expand_rect(cache, h, &pr.maps.xmap, &pr.maps.ykern, &r, sb, PW);
        }
        if sa != sb { bad += 1; }
        let full = Rect { x0: 0, y0: 0, x1: PW as u16, y1: PH as u16 };
        kern::expand_rect_ref(cache, h, &pr.maps.xmap, &pr.maps.yrev, &full, sa, PW);
        kern::expand_rect(cache, h, &pr.maps.xmap, &pr.maps.ykern, &full, sb, PW);
        if sa != sb { bad += 2; }
        // kernel timing, cache-resident source, PSRAM destination
        let t0 = now();
        for _ in 0..4 { kern::expand_rect_ref(cache, h, &pr.maps.xmap, &pr.maps.yrev, &full, sa, PW); }
        let t_ref = now().wrapping_sub(t0) / 4;
        let t0 = now();
        for _ in 0..4 { kern::expand_rect(cache, h, &pr.maps.xmap, &pr.maps.ykern, &full, sb, PW); }
        let t_asm = now().wrapping_sub(t0) / 4;
        p!("B20 selftest kernel_vs_ref_mismatch=%u full_expand_us ref=%u asm=%u\n", bad, t_ref, t_asm);
    }
    bad
}

#[no_mangle]
pub extern "C" fn b20_run(panel: *mut c_void, fb0: *mut u16, fb1: *mut u16, fb2: *mut u16, nfb: i32, snap_a: *const u8, snap_b: *const u8, live: i32, pclk: i32) {
    let a = parse_snap(snap_a);
    let b = parse_snap(snap_b);
    let (w, h) = (a.w, a.h);
    p!("B20 start w=%u h=%u nfb=%d live=%d pclk_mhz=%d\n", w as u32, h as u32, nfb, live, pclk);
    unsafe {
        let cache_ptr = heap_caps_aligned_calloc(128, 1, w * h * 2, CAPS) as *mut u16;
        let cache = core::slice::from_raw_parts_mut(cache_ptr, w * h);
        let mut sd = 0x1234_5678u32;
        for v in cache.iter_mut() { sd = sd.wrapping_mul(1664525).wrapping_add(1013904223); *v = (sd >> 12) as u16; } // random content for the kernel self-test
        let mem = heap_caps_aligned_calloc(128, 1, core::mem::size_of::<Presenter>() + 64, CAPS) as *mut Presenter;
        core::ptr::write(mem, Presenter::new(w, h, cache, nfb as u8));
        let pr: &'static mut Presenter<'static> = &mut *(mem as *mut Presenter<'static>);
        p!("B20 presenter_state_B=%u cache_B=%u\n", core::mem::size_of::<Presenter>() as u32, (w * h * 2) as u32);
        let bad = selftest(pr);
        let mut c = Ctx { pr, panel, fbs: [fb0, fb1, fb2], nfb: nfb as usize, idx: 0, hist_vs: [0; 3], ro: [Rect::default(); MAXRECT], live: live != 0 };
        let snaps = [&a, &b];
        let mut which = 0usize;
        let arrow = arrow_shape();

        // --- correctness fingerprint: full present of snapshot A, then pointer at (200,150): CRC of the panel buffer
        c.pr.mark_all();
        let (rep, dt) = do_present(&mut c, snaps[0]);
        let fb_a = core::slice::from_raw_parts(fb0, PW * PH);
        p!("B20 full_first us=%u panel_B=%u crc_full=%x\n", dt, rep.panel_bytes, crc32_u16(fb_a, 0));
        wait_free(&c);
        c.pr.set_cursor(Some(&arrow), 200, 150, true);
        wait_free(&c);
        let _ = do_present(&mut c, snaps[0]);
        wait_free(&c);
        c.pr.set_cursor(None, 200, 150, true);
        let _ = do_present(&mut c, snaps[0]);
        let shown = (c.idx + c.nfb - 1) % c.nfb;
        let sl = core::slice::from_raw_parts(c.fbs[shown], PW * PH);
        p!("B20 crc_cursor_200_150=%x\n", crc32_u16(sl, 0));
        c.pr.set_cursor(None, 200, 150, false);
        let _ = do_present(&mut c, snaps[0]);

        // --- idle: no change, no pointer move => no presents
        which = 0;
        run_loop(b"idle_100", 100, &mut c, snaps, &mut which, |_, _| {});

        // --- pointer sweep (Lissajous over the whole screen), one move per present
        c.pr.set_cursor(Some(&arrow), 10, 10, true);
        run_loop(b"pointer_sweep_400", 400, &mut c, snaps, &mut which, |i, c| {
            let t = i as i32;
            let x = 320 + ((t * 7) % 600) - 300 + if (t / 85) % 2 == 0 { 0 } else { 5 };
            let y = 240 + ((t * 5) % 440) - 220;
            c.pr.set_cursor(None, x, y, true);
        });

        // --- window-sized dirty rects (content changes: alternate snapshot A/B inside the rect)
        c.pr.set_cursor(None, 320, 240, false);
        for &(name, x0, y0, x1, y1) in &[(&b"dirty_64x48"[..], 100u16, 100u16, 164u16, 148u16), (&b"dirty_200x150"[..], 100, 100, 300, 250), (&b"dirty_400x300"[..], 100, 100, 500, 400), (&b"dirty_640x480_full"[..], 0, 0, 640, 480)] {
            let r = Rect { x0, y0, x1, y1 };
            run_loop(name, 24, &mut c, snaps, &mut which, |i, c| { c_set(c, &r); let _ = i; });
            which ^= 1;
        }
        // content toggle: this alternates the snapshot used by the conversion each iteration
        let mut tgl = 0usize;
        let r = Rect { x0: 100, y0: 100, x1: 500, y1: 400 };
        let mut aacc = Acc::default();
        let pc0 = probe();
        let t_start = now();
        for _ in 0..24 {
            c.pr.mark_rect(&r);
            wait_free(&c);
            let (rep, dt) = do_present(&mut c, snaps[tgl & 1]);
            tgl += 1;
            aacc.presents += 1; aacc.sum += dt as u64; aacc.max = aacc.max.max(dt);
            aacc.src += rep.src_bytes as u64; aacc.panel += rep.panel_bytes as u64; aacc.rects += rep.panel_rects as u64;
        }
        report(b"toggle_ab_400x300", &aacc, now().wrapping_sub(t_start), probe().wrapping_sub(pc0));

        // --- full frame every present (mark_all)
        run_loop(b"full_640x480", 24, &mut c, snaps, &mut which, |_, c| c.pr.mark_all());

        // --- LEGACY: old port pipeline (generic planar convert in C -> console fb -> PPA SRM rotate+scale -> draw_bitmap whole fb), 10 fps
        if live != 0 {
            let console = heap_caps_aligned_calloc(128, 1, 1280 * 800 * 2, CAPS) as *mut u16;
            let (mut tc, mut tp, mut td) = (0u32, 0u32, 0u32);
            let (mut sc, mut sp, mut sd, mut mx) = (0u64, 0u64, 0u64, 0u32);
            let pc0 = probe();
            let t_start = now();
            let n = 20u32;
            for _ in 0..n {
                let t0 = now();
                b20_old_present(console, snap_a, &mut tc, &mut tp, &mut td);
                let dt = now().wrapping_sub(t0);
                sc += tc as u64; sp += tp as u64; sd += td as u64; mx = mx.max(dt);
                // old throttle: 100 ms between presents
                while now().wrapping_sub(t0) < 100_000 {}
            }
            let wall = now().wrapping_sub(t_start);
            p!("B20 legacy_old_port presents=%u convert_us=%u ppa_us=%u draw_us=%u max_total_us=%u wall_ms=%u probe_copy_MBps_x10=%u\n", n, (sc / n as u64) as u32, (sp / n as u64) as u32, (sd / n as u64) as u32, mx, wall / 1000, (probe().wrapping_sub(pc0) as u64 * 65536 * 10 / wall as u64) as u32);
        }
        // --- probe floor: panel scanning only
        let pc0 = probe();
        let t0 = now();
        vTaskDelay(1000);
        let wall = now().wrapping_sub(t0);
        p!("B20 probe_floor_idle wall_ms=%u probe_copy_MBps_x10=%u\n", wall / 1000, (probe().wrapping_sub(pc0) as u64 * 65536 * 10 / wall as u64) as u32);
        p!("B20 done selftest_bad=%u\n", bad);
    }
}

fn c_set(c: &mut Ctx, r: &Rect) { c.pr.mark_rect(r); }

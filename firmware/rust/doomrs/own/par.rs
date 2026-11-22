//! B83 dual-core 3D view: R_RenderPlayerView records every column/span draw (walls, sky, planes, sprites, fuzz, translated) instead of
//! drawing it, then both cores replay the list, core 0 the columns x < split, core 1 the rest (`b83_par_run`, host glue).
//! Bit-exact: every pixel is written by the same commands in the same order as before (each half keeps the list order; a span cut at
//! the split continues with pos + step*(split-x1), the same wrapping sum the C loop reaches); fuzz reads only its own column and gets
//! its fuzzpos start from record time. The split comes from a per-column work histogram, so both halves get about the same pixels.
use crate::src::dg::r_draw::{columnofs, dc_colormap, dc_iscale, dc_source, dc_texturemid, dc_translation, dc_x, dc_yh, dc_yl,
    ds_colormap, ds_source, ds_x1, ds_x2, ds_xfrac, ds_xstep, ds_y, ds_yfrac, ds_ystep, fuzzoffset, fuzzpos, viewheight, ylookup};
use crate::src::dg::r_data::colormaps;
use crate::src::dg::r_main::centery;
use crate::src::hooks::i_system::I_Error;

pub const SW: i32 = 640;
pub const SH: i32 = 400;
const FUZZTABLE: i32 = 50;

#[derive(Clone, Copy)]
#[repr(C)]
struct Cmd {
    kind: u8,      // 0 column, 1 translated column, 2 fuzz column, 3 span
    xy: u16,       // column: x; span: y
    a: i16,        // column: yl; span: x1
    b: i16,        // column: yh; span: x2
    frac: i32,     // column: first frac; span: packed position; fuzz: fuzzpos at the first pixel
    step: i32,
    src: *const u8,
    cmap: *const u8,
    tr: *const u8, // translation table (kind 1)
}

const NMAX: usize = 8192;   // board B demo1 peak 4301 (s2); a full list flushes early (still bit-exact)
static mut BUF: [Cmd; NMAX] = unsafe { core::mem::zeroed() };
static mut N: usize = 0;
static mut WORK: [i32; SW as usize + 1] = [0; SW as usize + 1];   // per-column pixel count (columns) + span coverage (difference)
static mut SPANS: [i32; SW as usize + 1] = [0; SW as usize + 1];
/// recording on (R_RenderPlayerView); the kernels push commands instead of drawing
pub static mut REC: bool = false;
/// console `doom par 0|1` (host glue): 0 = the one-core path (generated order, no recording)
#[no_mangle]
pub static mut b83_par: i32 = 1;
/// largest list of the last frames and flushes forced by a full list (diagnostics: `doom` console line)
#[no_mangle]
pub static mut b83_par_peak: i32 = 0;
#[no_mangle]
pub static mut b83_par_split: i32 = 0;

extern "C" {
    /// host glue: run f(1) on core 1 while this core runs f(0); returns when both are done
    fn b83_par_run(f: unsafe extern "C" fn(i32));
}

#[inline(always)]
unsafe fn push(c: Cmd) {
    if N == NMAX {
        flush();
    }
    *BUF.as_mut_ptr().add(N) = c;
    N += 1;
}

pub unsafe fn rec_column(kind: u8) {
    let (yl, yh, x) = (dc_yl, dc_yh, dc_x);
    let frac = dc_texturemid.wrapping_add((yl - centery).wrapping_mul(dc_iscale));
    *WORK.as_mut_ptr().add(x as usize) += yh - yl + 1;
    push(Cmd { kind, xy: x as u16, a: yl as i16, b: yh as i16, frac, step: dc_iscale, src: dc_source, cmap: dc_colormap, tr: dc_translation });
}

pub unsafe fn rec_fuzz() {
    // the C kernel's own clamps (they also change dc_yl/dc_yh for the caller) and fuzzpos walk happen here, in list order
    let (yl, yh, x) = (dc_yl, dc_yh, dc_x);
    let n = yh - yl + 1;
    *WORK.as_mut_ptr().add(x as usize) += n;
    push(Cmd { kind: 2, xy: x as u16, a: yl as i16, b: yh as i16, frac: fuzzpos, step: 0, src: core::ptr::null(), cmap: colormaps, tr: core::ptr::null() });
    fuzzpos = (fuzzpos + n) % FUZZTABLE;
}

pub unsafe fn rec_span() {
    let (x1, x2) = (ds_x1, ds_x2);
    *SPANS.as_mut_ptr().add(x1 as usize) += 1;
    *SPANS.as_mut_ptr().add(x2 as usize + 1) -= 1;
    let pos = ((ds_xfrac << 10) as u32 & 0xffff0000) | ((ds_yfrac >> 6) & 0xffff) as u32;
    let step = ((ds_xstep << 10) as u32 & 0xffff0000) | ((ds_ystep >> 6) & 0xffff) as u32;
    push(Cmd { kind: 3, xy: ds_y as u16, a: x1 as i16, b: x2 as i16, frac: pos as i32, step: step as i32, src: ds_source, cmap: ds_colormap, tr: core::ptr::null() });
}

static mut SPLIT: i32 = SW / 2;

/// replay the recorded list for one half (0: x < SPLIT, 1: x >= SPLIT)
unsafe extern "C" fn replay(half: i32) {
    let (lo, hi) = if half == 0 { (0, SPLIT - 1) } else { (SPLIT, SW - 1) };
    let n = N;
    let buf = BUF.as_ptr();
    for i in 0..n {
        let c = &*buf.add(i);
        if c.kind == 3 {
            let (mut x1, mut x2) = (c.a as i32, c.b as i32);
            let mut pos = c.frac as u32;
            if x2 < lo || x1 > hi {
                continue;
            }
            if x1 < lo {
                pos = pos.wrapping_add((c.step as u32).wrapping_mul((lo - x1) as u32));
                x1 = lo;
            }
            if x2 > hi {
                x2 = hi;
            }
            span(c.xy as i32, x1, x2, pos, c.step as u32, c.src, c.cmap);
        } else {
            let x = c.xy as i32;
            if x < lo || x > hi {
                continue;
            }
            match c.kind {
                0 => column(x, c.a as i32, c.b as i32, c.frac, c.step, c.src, c.cmap),
                1 => translated(x, c.a as i32, c.b as i32, c.frac, c.step, c.src, c.cmap, c.tr),
                _ => fuzz(x, c.a as i32, c.b as i32, c.frac, c.cmap),
            }
        }
    }
}

/// draw everything recorded so far (both cores), then start a new list
pub unsafe fn flush() {
    if N == 0 {
        return;
    }
    // split where the left half holds half of the pixel work (columns: exact; spans: coverage per column)
    let (mut tot, mut cov) = (0i64, 0i32);
    for x in 0..SW as usize {
        cov += *SPANS.as_ptr().add(x);
        tot += (*WORK.as_ptr().add(x) + cov) as i64;
    }
    let (mut acc, mut sx, mut cov2) = (0i64, SW / 2, 0i32);
    for x in 0..SW as usize {
        cov2 += *SPANS.as_ptr().add(x);
        acc += (*WORK.as_ptr().add(x) + cov2) as i64;
        if acc * 2 >= tot {
            sx = x as i32 + 1;
            break;
        }
    }
    SPLIT = sx.clamp(1, SW - 1);
    b83_par_split = SPLIT;
    if N as i32 > b83_par_peak {
        b83_par_peak = N as i32;
    }
    b83_par_run(replay);
    N = 0;
    for x in 0..=SW as usize {
        *WORK.as_mut_ptr().add(x) = 0;
        *SPANS.as_mut_ptr().add(x) = 0;
    }
}

// ---- the kernels' pixel loops (shared by the direct path in own::r_draw and the replay) ----

#[inline(always)]
unsafe fn dest_of(x: i32, y: i32) -> *mut u8 {
    (*ylookup.as_ptr().add(y as usize)).offset(*columnofs.as_ptr().add(x as usize) as isize)
}

#[inline(always)]
pub unsafe fn column(x: i32, yl: i32, yh: i32, mut frac: i32, step: i32, src: *const u8, cmap: *const u8) {
    let mut dest = dest_of(x, yl);
    let mut n = (yh - yl) as u32 + 1;
    // 4 pixels per step, loads grouped (texels, then colormap, then stores): the in-order core overlaps the load latencies
    while n >= 4 {
        let (f1, f2, f3) = (frac.wrapping_add(step), frac.wrapping_add(step.wrapping_mul(2)), frac.wrapping_add(step.wrapping_mul(3)));
        let t0 = *src.add(((frac >> 16) & 127) as usize);
        let t1 = *src.add(((f1 >> 16) & 127) as usize);
        let t2 = *src.add(((f2 >> 16) & 127) as usize);
        let t3 = *src.add(((f3 >> 16) & 127) as usize);
        let (c0, c1, c2, c3) = (*cmap.add(t0 as usize), *cmap.add(t1 as usize), *cmap.add(t2 as usize), *cmap.add(t3 as usize));
        *dest = c0;
        *dest.add(SW as usize) = c1;
        *dest.add(2 * SW as usize) = c2;
        *dest.add(3 * SW as usize) = c3;
        dest = dest.add(4 * SW as usize);
        frac = frac.wrapping_add(step.wrapping_mul(4));
        n -= 4;
    }
    while n > 0 {
        *dest = *cmap.add(*src.add(((frac >> 16) & 127) as usize) as usize);
        dest = dest.add(SW as usize);
        frac = frac.wrapping_add(step);
        n -= 1;
    }
}

#[inline(always)]
pub unsafe fn translated(x: i32, yl: i32, yh: i32, mut frac: i32, step: i32, src: *const u8, cmap: *const u8, tr: *const u8) {
    let mut dest = dest_of(x, yl);
    for _ in 0..=(yh - yl) {
        *dest = *cmap.add(*tr.add(*src.offset((frac >> 16) as isize) as usize) as usize);
        dest = dest.add(SW as usize);
        frac = frac.wrapping_add(step);
    }
}

#[inline(always)]
pub unsafe fn fuzz(x: i32, yl: i32, yh: i32, mut fp: i32, cm: *const u8) {
    let mut dest = dest_of(x, yl);
    for _ in 0..=(yh - yl) {
        *dest = *cm.offset((6 * 256 + *dest.offset(*fuzzoffset.as_ptr().add(fp as usize) as isize) as i32) as isize);
        fp += 1;
        if fp == FUZZTABLE {
            fp = 0;
        }
        dest = dest.add(SW as usize);
    }
}

#[inline(always)]
pub unsafe fn span(y: i32, x1: i32, x2: i32, mut pos: u32, step: u32, src: *const u8, cmap: *const u8) {
    let mut dest = dest_of(x1, y);
    let mut n = (x2 - x1) as u32 + 1;
    let sp = |p: u32| (((p >> 4) & 0xfc0) | (p >> 26)) as usize;
    while n >= 4 {
        let (p1, p2, p3) = (pos.wrapping_add(step), pos.wrapping_add(step.wrapping_mul(2)), pos.wrapping_add(step.wrapping_mul(3)));
        let (t0, t1, t2, t3) = (*src.add(sp(pos)), *src.add(sp(p1)), *src.add(sp(p2)), *src.add(sp(p3)));
        let (c0, c1, c2, c3) = (*cmap.add(t0 as usize), *cmap.add(t1 as usize), *cmap.add(t2 as usize), *cmap.add(t3 as usize));
        *dest = c0;
        *dest.add(1) = c1;
        *dest.add(2) = c2;
        *dest.add(3) = c3;
        dest = dest.add(4);
        pos = pos.wrapping_add(step.wrapping_mul(4));
        n -= 4;
    }
    while n > 0 {
        *dest = *cmap.add(*src.add(sp(pos)) as usize);
        dest = dest.add(1);
        pos = pos.wrapping_add(step);
        n -= 1;
    }
}

// ---- the C entry points (colfunc/fuzzcolfunc/transcolfunc/spanfunc targets) ----

#[no_mangle]
pub unsafe extern "C" fn R_DrawColumn() {
    let (yl, yh, x) = (dc_yl, dc_yh, dc_x);
    if yh - yl < 0 {
        return;
    }
    if x as u32 >= SW as u32 || yl < 0 || yh >= SH {
        I_Error(b"R_DrawColumn: %i to %i at %i\0".as_ptr() as *mut _, yl, yh, x);
    }
    if REC {
        return rec_column(0);
    }
    column(x, yl, yh, dc_texturemid.wrapping_add((yl - centery).wrapping_mul(dc_iscale)), dc_iscale, dc_source, dc_colormap);
}

#[no_mangle]
pub unsafe extern "C" fn R_DrawTranslatedColumn() {
    let (yl, yh, x) = (dc_yl, dc_yh, dc_x);
    if yh - yl < 0 {
        return;
    }
    if x as u32 >= SW as u32 || yl < 0 || yh >= SH {
        I_Error(b"R_DrawColumn: %i to %i at %i\0".as_ptr() as *mut _, yl, yh, x);
    }
    if REC {
        return rec_column(1);
    }
    translated(x, yl, yh, dc_texturemid.wrapping_add((yl - centery).wrapping_mul(dc_iscale)), dc_iscale, dc_source, dc_colormap, dc_translation);
}

#[no_mangle]
pub unsafe extern "C" fn R_DrawFuzzColumn() {
    if dc_yl == 0 {
        dc_yl = 1;
    }
    if dc_yh == viewheight - 1 {
        dc_yh = viewheight - 2;
    }
    let (yl, yh, x) = (dc_yl, dc_yh, dc_x);
    if yh - yl < 0 {
        return;
    }
    if x as u32 >= SW as u32 || yl < 0 || yh >= SH {
        I_Error(b"R_DrawFuzzColumn: %i to %i at %i\0".as_ptr() as *mut _, yl, yh, x);
    }
    if REC {
        return rec_fuzz();
    }
    fuzz(x, yl, yh, fuzzpos, colormaps);
    fuzzpos = (fuzzpos + yh - yl + 1) % FUZZTABLE;
}

#[no_mangle]
pub unsafe extern "C" fn R_DrawSpan() {
    let (x1, x2, y) = (ds_x1, ds_x2, ds_y);
    if x2 < x1 || x1 < 0 || x2 >= SW || y as u32 > SH as u32 {
        I_Error(b"R_DrawSpan: %i to %i at %i\0".as_ptr() as *mut _, x1, x2, y);
    }
    if REC {
        return rec_span();
    }
    let pos = ((ds_xfrac << 10) as u32 & 0xffff0000) | ((ds_yfrac >> 6) & 0xffff) as u32;
    let step = ((ds_xstep << 10) as u32 & 0xffff0000) | ((ds_ystep >> 6) & 0xffff) as u32;
    span(y, x1, x2, pos, step, ds_source, ds_colormap);
}

extern "C" {
    fn R_RenderPlayerView_c2r(player: *mut core::ffi::c_void);
    fn esp_timer_get_time() -> i64;
}

/// the generated view with recording around it; the recorded list is drawn by both cores at the end. dg_render_us (host stats line)
/// then covers recording + both halves
#[no_mangle]
pub unsafe extern "C" fn R_RenderPlayerView(player: *mut core::ffi::c_void) {
    let t0 = esp_timer_get_time();
    REC = b83_par != 0;
    R_RenderPlayerView_c2r(player);
    REC = false;
    flush();
    crate::src::dg::r_main::dg_render_us = (esp_timer_get_time() - t0) as u32;
}

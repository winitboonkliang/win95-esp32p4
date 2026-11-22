//! C ABI for integration into the emulator (lead): caller owns all memory, no allocation here.
use crate::*;

#[no_mangle]
pub extern "C" fn b20_state_size() -> usize { core::mem::size_of::<Presenter>() }

/// mem: b20_state_size() bytes, 8-aligned. cache: w*h u16 (column-major RGB565 cache), nfb 2 or 3. Returns mem.
#[no_mangle]
pub unsafe extern "C" fn b20_init(mem: *mut u8, w: i32, h: i32, nfb: i32, cache: *mut u16) -> *mut Presenter<'static> {
    let c = core::slice::from_raw_parts_mut(cache, (w * h) as usize);
    let p = mem as *mut Presenter<'static>;
    core::ptr::write(p, Presenter::new(w as usize, h as usize, c, nfb as u8));
    p
}
/// guest wrote source pixels in [x0,x1) x [y0,y1) (call from the VGA/LFB write path or from a changed-band scan)
#[no_mangle]
pub unsafe extern "C" fn b20_mark_rect(p: *mut Presenter<'static>, x0: i32, y0: i32, x1: i32, y1: i32) {
    (*p).mark_rect(&Rect { x0: x0.max(0) as u16, y0: y0.max(0) as u16, x1: x1.max(0) as u16, y1: y1.max(0) as u16 });
}
/// mode change, palette change, start address change, first frame
#[no_mangle]
pub unsafe extern "C" fn b20_mark_all(p: *mut Presenter<'static>) { (*p).mark_all(); }

/// SetCursor: and/xor = h rows of u32 (bit i = pixel i), w <= 32 here (driver size up to 64 via the Rust API)
#[no_mangle]
pub unsafe extern "C" fn b20_cursor_shape(p: *mut Presenter<'static>, w: i32, h: i32, hx: i32, hy: i32, and: *const u32, xor: *const u32, x: i32, y: i32, visible: i32) {
    let mut s = CursorShape { w: w as u8, h: h as u8, hx: hx as i16, hy: hy as i16, and: [u64::MAX; CMAX], xor: [0; CMAX] };
    for j in 0..(h as usize).min(32) { s.and[j] = *and.add(j) as u64 | (u64::MAX << 32); s.xor[j] = *xor.add(j) as u64; }
    (*p).set_cursor(Some(&s), x, y, visible != 0);
}
/// MoveCursor
#[no_mangle]
pub unsafe extern "C" fn b20_cursor_move(p: *mut Presenter<'static>, x: i32, y: i32, visible: i32) { (*p).set_cursor(None, x, y, visible != 0); }

/// One present from planar16 VRAM (4 B per address, byte p = plane p). back = the panel buffer to write (800x1280 u16).
/// rects_out: MAXRECT x {x0,y0,x1,y1} u16 panel rects to cache-sync. Returns number of rects, 0 = idle (do not swap).
/// src_bytes/panel_bytes: out counters (may be NULL).
#[no_mangle]
pub unsafe extern "C" fn b20_present_planar16(p: *mut Presenter<'static>, vram: *const u8, vram_len: usize, base: usize, line: usize, pal16: *const u16,
                                              back: *mut u16, rects_out: *mut u16, src_bytes: *mut u32, panel_bytes: *mut u32) -> i32 {
    let pr = &mut *p;
    let h = pr.maps.h as usize;
    let vram = core::slice::from_raw_parts(vram, vram_len);
    let mut pal = [0u16; 16];
    for i in 0..16 { pal[i] = *pal16.add(i); }
    let back = core::slice::from_raw_parts_mut(back, PW * PH);
    let mut ro = [Rect::default(); MAXRECT];
    let (rep, n) = pr.present(|c, r| planar16_to_cache(vram, base, line, &pal, c, h, r), back, &mut ro);
    for i in 0..n { let o = rects_out.add(4 * i); *o = ro[i].x0; *o.add(1) = ro[i].y0; *o.add(2) = ro[i].x1; *o.add(3) = ro[i].y1; }
    if !src_bytes.is_null() { *src_bytes = rep.src_bytes; }
    if !panel_bytes.is_null() { *panel_bytes = rep.panel_bytes; }
    n as i32
}

// ---------------------------------------------------------------- LinearPresenter C ABI (B29a): caller owns memory, no allocation
use crate::linear::LinearPresenter;
#[no_mangle]
pub extern "C" fn b29l_state_size() -> usize { core::mem::size_of::<LinearPresenter>() }
/// mem: b29l_state_size() bytes, 8-aligned.  nfb = number of panel buffers (2 or 3).
#[no_mangle]
pub unsafe extern "C" fn b29l_init(mem: *mut u8, nfb: i32) -> *mut LinearPresenter {
    let p = mem as *mut LinearPresenter;
    core::ptr::write(p, LinearPresenter::new(nfb as u8));
    p
}
/// pixels of the surface changed in [x0,x1) x [y0,y1) (half open, source coordinates 1280x800)
#[no_mangle]
pub unsafe extern "C" fn b29l_damage(p: *mut LinearPresenter, x0: i32, y0: i32, x1: i32, y1: i32) {
    (*p).damage(&Rect { x0: x0.max(0) as u16, y0: y0.max(0) as u16, x1: x1.max(0) as u16, y1: y1.max(0) as u16 });
}
#[no_mangle]
pub unsafe extern "C" fn b29l_damage_all(p: *mut LinearPresenter) { (*p).damage_all(); }
/// guest mode change (surface width/height in pixels: 1280x800, 1024x640 or 640x400); resets all history and repaints everything
#[no_mangle]
pub unsafe extern "C" fn b29l_set_mode(p: *mut LinearPresenter, w: i32, h: i32) { (*p).set_mode(w as usize, h as usize); }
/// scale to the panel in quarters: 4 = 1:1, 5 = 1.25x, 8 = 2x (the PPA job scales by this/4)
#[no_mangle]
pub unsafe extern "C" fn b29l_scale4(p: *mut LinearPresenter) -> i32 { (*p).scale4() as i32 }
/// new shape: and/xor = h rows of u32, BIT i = PIXEL i (the C side reverses the card's MSB-first bytes)
#[no_mangle]
pub unsafe extern "C" fn b29l_cursor_shape(p: *mut LinearPresenter, w: i32, h: i32, hx: i32, hy: i32, and: *const u32, xor: *const u32, x: i32, y: i32, visible: i32) {
    let mut s = CursorShape { w: w as u8, h: h as u8, hx: hx as i16, hy: hy as i16, and: [u64::MAX; CMAX], xor: [0; CMAX] };
    for j in 0..(h as usize).min(32) { s.and[j] = *and.add(j) as u64 | (u64::MAX << 32); s.xor[j] = *xor.add(j) as u64; }
    (*p).set_cursor(Some(&s), x, y, visible != 0);
}
#[no_mangle]
pub unsafe extern "C" fn b29l_cursor_move(p: *mut LinearPresenter, x: i32, y: i32, visible: i32) { (*p).set_cursor(None, x, y, visible != 0); }

/// pointer fast path (see LinearPresenter::cursor_fast): both panel buffers get the final pixels of the old and new pointer rect at once.  rects_out = 2 x {x0,y0,x1,y1}
/// PANEL rects to cache-sync in BOTH buffers.  Returns the rect count, 0 = use the normal present.
#[no_mangle]
pub unsafe extern "C" fn b29l_cursor_fast(p: *mut LinearPresenter, surf: *const u16, buf0: *mut u16, buf1: *mut u16, rects_out: *mut u16) -> i32 {
    let (sw, sh) = (*p).dims();
    let s = core::slice::from_raw_parts(surf, sw * sh);
    let b0 = core::slice::from_raw_parts_mut(buf0, PW * PH);
    let b1 = core::slice::from_raw_parts_mut(buf1, PW * PH);
    let mut ro = [Rect::default(); 2];
    let n = (*p).cursor_fast(s, b0, b1, &mut ro);
    for i in 0..n { let o = rects_out.add(4 * i); *o = ro[i].x0; *o.add(1) = ro[i].y0; *o.add(2) = ro[i].x1; *o.add(3) = ro[i].y1; }
    n as i32
}
/// PPA job callback: rotate the SOURCE block [x0,x1)x[y0,y1) (already 4-aligned) of the surface into the panel buffer `back`.
pub type PpaCb = unsafe extern "C" fn(x0: i32, y0: i32, x1: i32, y1: i32, back: *mut u16);
/// One present.  surf = 1280x800 u16 (stride 1280).  back = panel buffer 800x1280 u16.  rects_out = MAXRECT x {x0,y0,x1,y1} u16 PANEL rects to
/// cache-sync.  Returns the number of rects, 0 = nothing changed (do not swap buffers).
#[no_mangle]
pub unsafe extern "C" fn b29l_present(p: *mut LinearPresenter, surf: *const u16, back: *mut u16, ppa: PpaCb, rects_out: *mut u16, src_px: *mut u32) -> i32 {
    let (sw, sh) = (*p).dims();
    let s = core::slice::from_raw_parts(surf, sw * sh);
    let b = core::slice::from_raw_parts_mut(back, PW * PH);
    let mut ro = [Rect::default(); MAXRECT];
    let bp = back;
    let (n, px) = (*p).present(s, b, |r, _bk| ppa(r.x0 as i32, r.y0 as i32, r.x1 as i32, r.y1 as i32, bp), &mut ro);
    for i in 0..n { let o = rects_out.add(4 * i); *o = ro[i].x0; *o.add(1) = ro[i].y0; *o.add(2) = ro[i].x1; *o.add(3) = ro[i].y1; }
    if !src_px.is_null() { *src_px = px; }
    n as i32
}

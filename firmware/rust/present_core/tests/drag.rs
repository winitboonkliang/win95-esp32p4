// B29a: XOR drag outline with the pointer overlay on/off, 2 panel buffers, presents interleaved with the draw/erase steps.
// Expected panel = rotate(surface with the pointer composited); checked after EVERY present against the buffer that was just written.
use present_core::linear::*;
use present_core::*;

fn xor_rect_edges(s: &mut [u16], x0: usize, y0: usize, x1: usize, y1: usize, t: usize) -> Vec<Rect> {
    // 4 edges, t px thick, inclusive-exclusive; XOR 0xffff
    let mut r = vec![];
    let e = [(x0, y0, x1, y0 + t), (x0, y1 - t, x1, y1), (x0, y0, x0 + t, y1), (x1 - t, y0, x1, y1)];
    for &(a, b, c, d) in &e {
        let (a, b, c, d) = (a.min(SW), b.min(SH), c.min(SW), d.min(SH));
        if a >= c || b >= d { continue; }
        for y in b..d { for x in a..c { s[y * SW + x] ^= 0xffff; } }
        r.push(Rect { x0: a as u16, y0: b as u16, x1: c as u16, y1: d as u16 });
    }
    r
}

fn expected(s: &[u16], cur: Option<(i32, i32)>) -> Vec<u16> {
    let mut e = s.to_vec();
    if let Some((cx, cy)) = cur {
        let a = arrow_shape();
        for j in 0..32i32 { for i in 0..32i32 {
            let (x, y) = (cx + i, cy + j);
            if x >= 0 && y >= 0 && x < SW as i32 && y < SH as i32 {
                let k = y as usize * SW + x as usize;
                e[k] = cursor_px(s[k], (a.and[j as usize] >> i) & 1 != 0, (a.xor[j as usize] >> i) & 1 != 0);
            }
        } }
    }
    e
}

fn run(overlay: bool) {
    let mut s = vec![0u16; SW * SH];
    let mut lp = LinearPresenter::new(2);
    let mut bufs = vec![vec![0u16; PW * PH]; 2];
    let mut which = 0usize;
    let mut out = [Rect::default(); MAXRECT];
    let arrow = arrow_shape();
    lp.set_cursor(Some(&arrow), 0, 0, false);
    lp.damage_all();
    let mut check = |lp: &mut LinearPresenter, s: &Vec<u16>, which: &mut usize, bufs: &mut Vec<Vec<u16>>, cur: Option<(i32, i32)>, tag: &str| {
        let (nt, _) = lp.present(s, &mut bufs[*which], |r, b| rot_cpu(s, r, b), &mut out);
        if nt == 0 { return; }
        let e = expected(s, cur);
        for py in 0..PH { for px in 0..PW { assert_eq!(bufs[*which][py * PW + px], e[(799 - px) * SW + py], "overlay {} {} px {} py {}", overlay, tag, px, py); } }
        *which ^= 1;
    };
    check(&mut lp, &s, &mut which, &mut bufs, None, "init");
    check(&mut lp, &s, &mut which, &mut bufs, None, "init2");
    let (mut ox, mut oy, mut ow, mut oh) = (300usize, 60usize, 900usize, 560usize);
    let mut old: Option<(usize, usize, usize, usize)> = None;
    let (mut cx, mut cy) = (ox as i32 + 5, oy as i32 + 5);
    for step in 0..60 {
        // erase old outline (XOR), draw new outline (XOR); the pointer sits on / near the outline edges
        let mut dmg = vec![];
        if let Some((a, b, c, d)) = old { dmg.extend(xor_rect_edges(&mut s, a, b, c, d, 2)); }
        ox = (ox as i32 - 7 + (step % 3) as i32).max(0) as usize; oy = oy + 9;
        if oy + oh > SH + 200 { oy = 0; }
        let (a, b, c, d) = (ox, oy, (ox + ow).min(SW + 100), oy + oh);
        // interleaved presents: after the erase, before the redraw (what a free-running presenter can do)
        for r in &dmg { lp.damage(r); }
        if overlay { lp.set_cursor(None, cx, cy, true); }
        check(&mut lp, &s, &mut which, &mut bufs, if overlay { Some((cx, cy)) } else { None }, "after_erase");
        let mut dmg2 = xor_rect_edges(&mut s, a, b, c, d, 2);
        old = Some((a, b, c, d));
        cx = (ox as i32 + 20 + (step * 11 % 300) as i32).min(SW as i32 - 1); cy = oy as i32 + 3 + (step % 5) as i32;
        for r in dmg2.drain(..) { lp.damage(&r); }
        if overlay { lp.set_cursor(None, cx, cy, true); }
        check(&mut lp, &s, &mut which, &mut bufs, if overlay { Some((cx, cy)) } else { None }, "after_draw");
        // pointer-only present
        if overlay { cx += 3; lp.set_cursor(None, cx, cy, true); check(&mut lp, &s, &mut which, &mut bufs, Some((cx, cy)), "cursor_only"); }
        ow = ow; oh = oh;
    }
}

#[test] fn xor_outline_drag_overlay_off() { run(false); }
#[test] fn xor_outline_drag_overlay_on() { run(true); }

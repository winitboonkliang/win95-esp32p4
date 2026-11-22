// B29a: the three display modes (1280x800 1:1, 1024x640 1.25x, 640x400 2x) against an independent model:
// panel(px, py) = S'(X = py, Y = 799 - px), S'(X, Y) = S(floor(X*4/s4), floor(Y*4/s4)) with the pointer composited in scaled space.
use present_core::linear::*;
use present_core::*;

struct Lcg(u64);
impl Lcg { fn next(&mut self) -> u32 { self.0 = self.0.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407); (self.0 >> 33) as u32 } }

fn expected(s: &[u16], sw: usize, s4: usize, cur: Option<(i32, i32)>) -> Vec<u16> {
    let a = arrow_shape();
    let mut e = vec![0u16; PW * PH];
    for py in 0..PH {
        for px in 0..PW {
            let (x, y) = (py, PW - 1 - px);
            let mut v = s[(y * 4 / s4) * sw + x * 4 / s4];
            if let Some((cx, cy)) = cur {
                let (ox, oy) = ((cx - a.hx as i32) * s4 as i32 / 4, (cy - a.hy as i32) * s4 as i32 / 4);
                let (i, j) = ((x as i32 - ox) * 4 / s4 as i32, (y as i32 - oy) * 4 / s4 as i32);
                if x as i32 >= ox && y as i32 >= oy && i >= 0 && j >= 0 && i < a.w as i32 && j < a.h as i32 {
                    v = cursor_px(v, (a.and[j as usize] >> i) & 1 != 0, (a.xor[j as usize] >> i) & 1 != 0);
                }
            }
            e[py * PW + px] = v;
        }
    }
    e
}

fn run(sw: usize, sh: usize) {
    let mut r = Lcg(5 + sw as u64);
    let mut s: Vec<u16> = (0..sw * sh).map(|_| r.next() as u16).collect();
    let mut lp = LinearPresenter::new(2);
    lp.set_mode(sw, sh);
    let s4 = lp.scale4();
    assert_eq!(lp.dims(), (sw, sh));
    let mut bufs = vec![vec![0u16; PW * PH]; 2];
    let mut which = 0usize;
    let mut out = [Rect::default(); MAXRECT];
    let arrow = arrow_shape();
    lp.set_cursor(Some(&arrow), 0, 0, false);
    let (mut cx, mut cy, mut vis) = (0i32, 0i32, false);
    let mut ppa_calls = 0;
    for it in 0..80 {
        let n = if it == 0 { 0 } else { (r.next() % 3) as usize };
        for _ in 0..n {
            let x0 = r.next() as usize % sw;
            let y0 = r.next() as usize % sh;
            let x1 = (x0 + 1 + r.next() as usize % 120).min(sw);
            let y1 = (y0 + 1 + r.next() as usize % 80).min(sh);
            for y in y0..y1 { for x in x0..x1 { s[y * sw + x] = r.next() as u16; } }
            lp.damage(&Rect { x0: x0 as u16, y0: y0 as u16, x1: x1 as u16, y1: y1 as u16 });
        }
        if it % 3 == 0 {
            cx = (r.next() % (sw as u32 + 80)) as i32 - 40;
            cy = (r.next() % (sh as u32 + 80)) as i32 - 40;
            vis = it % 10 != 0;
            lp.set_cursor(None, cx, cy, vis);
        }
        let (nt, _) = lp.present(&s, &mut bufs[which], |rr, b| {
            ppa_calls += 1;
            if s4 == 4 { assert!(rr.y0 % 4 == 0 && rr.y1 % 4 == 0); } else { assert!(rr.x0 % 16 == 0 && rr.y0 % 16 == 0 && (rr.x1 % 16 == 0 || rr.x1 as usize == sw) && (rr.y1 % 16 == 0 || rr.y1 as usize == sh)); }
            rot_cpu_scaled(&s, sw, s4, rr, b);
        }, &mut out);
        if nt == 0 { continue; }
        for i in 0..nt { let pr = out[i]; assert!(pr.x0 % 4 == 0 && pr.x1 % 4 == 0, "panel rect x must be 4-aligned for the PPA: {:?}", (pr.x0, pr.x1)); }
        let e = expected(&s, sw, s4, if vis { Some((cx, cy)) } else { None });
        for py in 0..PH { for px in 0..PW { assert_eq!(bufs[which][py * PW + px], e[py * PW + px], "mode {}x{} it {} px {} py {}", sw, sh, it, px, py); } }
        which ^= 1;
    }
    assert!(ppa_calls > 0);
}

#[test] fn mode_1280x800() { run(1280, 800); }
#[test] fn mode_1024x640() { run(1024, 640); }
#[test] fn mode_640x400() { run(640, 400); }

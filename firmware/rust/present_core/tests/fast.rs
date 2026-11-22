// B29a: pointer fast path. Interleaves normal presents (random damage) with cursor-only fast moves in all three modes; after every operation the buffer(s) written
// must equal rotate(scale(surface) + pointer). A fast move writes BOTH buffers, a normal present writes one (the other lags by buffer age, as before).
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
    let mut r = Lcg(11 + sw as u64);
    let mut s: Vec<u16> = (0..sw * sh).map(|_| r.next() as u16).collect();
    let mut lp = LinearPresenter::new(2);
    lp.set_mode(sw, sh);
    let s4 = lp.scale4();
    let arrow = arrow_shape();
    let mut bufs = [vec![0u16; PW * PH], vec![0u16; PW * PH]];
    let mut which = 0usize;
    let mut out = [Rect::default(); MAXRECT];
    let mut out2 = [Rect::default(); 2];
    lp.set_cursor(Some(&arrow), 0, 0, false);
    // two normal presents fill both buffers (age builds up)
    for _ in 0..2 {
        let (nt, _) = lp.present(&s, &mut bufs[which], |rr, b| rot_cpu_scaled(&s, sw, s4, rr, b), &mut out);
        assert!(nt > 0 || which == 1);
        which ^= 1;
        lp.damage_all();
    }
    let (mut cx, mut cy, mut vis) = (100i32, 100i32, true);
    let mut fast = 0;
    for it in 0..120 {
        if it % 4 == 3 {
            // normal present with damage (+ maybe a pointer change handled by the normal path)
            let x0 = r.next() as usize % sw; let y0 = r.next() as usize % sh;
            let (x1, y1) = ((x0 + 1 + r.next() as usize % 100).min(sw), (y0 + 1 + r.next() as usize % 60).min(sh));
            for y in y0..y1 { for x in x0..x1 { s[y * sw + x] = r.next() as u16; } }
            lp.damage(&Rect { x0: x0 as u16, y0: y0 as u16, x1: x1 as u16, y1: y1 as u16 });
            let (nt, _) = lp.present(&s, &mut bufs[which], |rr, b| rot_cpu_scaled(&s, sw, s4, rr, b), &mut out);
            if nt > 0 {
                let e = expected(&s, sw, s4, if vis { Some((cx, cy)) } else { None });
                assert!(bufs[which] == e, "normal present mode {}x{} it {}", sw, sh, it);
                which ^= 1;
            }
        } else {
            cx = (cx + (r.next() % 61) as i32 - 30).clamp(-40, sw as i32 + 40);
            cy = (cy + (r.next() % 61) as i32 - 30).clamp(-40, sh as i32 + 40);
            if it % 17 == 0 { vis = !vis; }
            lp.set_cursor(None, cx, cy, vis);
            let snap = [bufs[0].clone(), bufs[1].clone()];
            let [b0, b1] = &mut bufs;
            let n = lp.cursor_fast(&s, b0, b1, &mut out2);
            if n > 0 {
                fast += 1;
                let e = expected(&s, sw, s4, if vis { Some((cx, cy)) } else { None });
                for b in 0..2 {
                    for py in 0..PH { for px in 0..PW {
                        let inside = (0..n).any(|k| (out2[k].x0 as usize..out2[k].x1 as usize).contains(&px) && (out2[k].y0 as usize..out2[k].y1 as usize).contains(&py));
                        let q = py * PW + px;
                        if inside { assert_eq!(bufs[b][q], e[q], "fast path mode {}x{} it {} buffer {} px {} py {}", sw, sh, it, b, px, py); }
                        else { assert_eq!(bufs[b][q], snap[b][q], "fast path touched a pixel outside its rects (buffer {} px {} py {})", b, px, py); }
                    } }
                }
            }
        }
    }
    assert!(fast > 20, "fast path used {} times", fast);
}

#[test] fn fast_1280x800() { run(1280, 800); }
#[test] fn fast_1024x640() { run(1024, 640); }
#[test] fn fast_640x400() { run(640, 400); }

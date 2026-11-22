// Host test harness: injected clock, oracle = study/SYS/data/pit_vectors.txt (independent of any emulator code).
use b40::{Pit, Warp};

const VEC: &str = include_str!("pit_vectors.txt");

fn ctl(ch: u32, rw: u32, mode: u32) -> u32 { (ch << 6) | (rw << 4) | (mode << 1) }

fn load(p: &mut Pit, t: u64, ch: u32, mode: u32, count: u32) {
    p.write(t, 3, ctl(ch, 3, mode));
    p.write(t, ch, count & 0xff);
    p.write(t, ch, (count >> 8) & 0xff);
}

fn latch_read(p: &mut Pit, t: u64, ch: u32) -> u32 {
    p.write(t, 3, ctl(ch, 0, 0));
    let lo = p.read(t, ch);
    let hi = p.read(t, ch);
    lo | (hi << 8)
}

fn near(a: u32, b: u32, count: u32) -> bool {
    let c = if count == 65536 { 65536 } else { count };
    let d = (a as i64 - b as i64).rem_euclid(c.max(1) as i64);
    d <= 1 || d >= c as i64 - 1
}

fn vectors() -> Vec<String> {
    VEC.lines().map(|s| s.to_string()).collect()
}

#[test]
fn counter_vectors_all_modes_and_aliases() {
    let mut n = 0;
    for base in [1_000u64, 4_294_967_296 - 1_000, 5_000_000_000_000] {
        for l in vectors() {
            if l.starts_with('#') || l.starts_with("IRQ0") || l.trim().is_empty() { continue; }
            let f: Vec<u64> = l.split_whitespace().map(|x| x.parse().unwrap()).collect();
            let (mode, count, t, exp) = (f[0] as u32, f[1] as u32, f[2], f[3] as u32);
            let alias = match mode { 2 => 6, 3 => 7, m => m };
            for m in [mode, alias] {
                let mut p = Pit::new();
                load(&mut p, base, 0, m, count % 65536);
                let cnt = if count == 65536 { 65536 } else { count };
                let v1 = latch_read(&mut p, base + t, 0);
                assert!(near(v1, exp, cnt), "latch mode {m} count {count} t {t} base {base}: got {v1} exp {exp}");
                let lo = p.read(base + t, 0);
                let hi = p.read(base + t, 0);
                let v2 = lo | (hi << 8);
                assert!(near(v2, exp, cnt), "direct mode {m} count {count} t {t}: got {v2} exp {exp}");
                n += 1;
            }
        }
    }
    assert!(n > 500, "ran {n}");
}

#[test]
fn irq0_stream_vectors() {
    for l in vectors() {
        if !l.starts_with("IRQ0") { continue; }
        let f: Vec<u64> = l.split_whitespace().skip(1).map(|x| x.parse().unwrap()).collect();
        let (count, win, exp) = (f[0] as u32, f[1], f[2]);
        for mode in [2u32, 3, 6, 7] {
            let mut p = Pit::new();
            let t0 = 7_000_000_000u64; // > 2^32 us
            load(&mut p, t0, 0, mode, count % 65536);
            let mut got = 0u64;
            let mut t = t0;
            while t <= t0 + win {
                while p.take_one(t) { got += 1; }
                t += 997; // 1 kHz-ish poll, irregular
            }
            while p.take_one(t0 + win) { got += 1; }
            assert!(got + 1 >= exp && got <= exp + 1, "mode {mode} count {count} win {win}: got {got} exp {exp}");
        }
    }
}

#[test]
fn stall_is_recovered_not_lost() {
    let mut p = Pit::new();
    load(&mut p, 0, 0, 2, 16384);
    let mut got = 0u64;
    let mut t = 0u64;
    while t < 2_000_000 {
        while p.take_one(t) { got += 1; }
        // a 300 ms host stall at 0.5 s and a 2 s one near the end
        t += if t == 500_000 - (500_000 % 1000) { 300_000 } else { 1000 };
    }
    while p.take_one(t) { got += 1; }
    let exp = t * 1_193_182 / 1_000_000 / 16384;
    assert_eq!(p.lost, 0);
    assert!(got + 1 >= exp && got <= exp, "got {got} exp {exp}");
}

#[test]
fn backlog_cap_counts_lost() {
    let mut p = Pit::new();
    p.max_backlog = 100;
    load(&mut p, 0, 0, 2, 1193); // 1 kHz
    let mut got = 0u64;
    while p.take_one(10_000_000) { got += 1; } // 10 s stall = 10000 ticks due
    assert!(p.lost >= 9_800 && got <= 101, "lost {} got {}", p.lost, got);
}

#[test]
fn counter_and_deadline_after_24h() {
    let mut p = Pit::new();
    let t0 = 86_400_000_000u64;
    load(&mut p, t0, 0, 2, 16384);
    let dl = p.next_deadline(t0);
    assert!(dl > t0 && dl - t0 <= 13_733, "dl {dl}");
    assert!(!p.take_one(dl - 1));
    assert!(p.take_one(dl));
    // read exactly 24 h after load
    let v = latch_read(&mut p, t0 + 86_400_000_000, 0);
    assert!(v >= 1 && v <= 16384);
}

#[test]
fn mode0_single_tick_mode1_5_need_trigger() {
    let mut p = Pit::new();
    load(&mut p, 0, 0, 0, 100);
    assert!(!p.take_one(50));
    assert!(p.take_one(200));
    assert!(!p.take_one(1_000_000));
    for m in [1u32, 5] {
        let mut q = Pit::new();
        load(&mut q, 0, 0, m, 100);
        assert!(!q.take_one(1_000_000), "mode {m} must not fire without trigger");
        assert_eq!(latch_read(&mut q, 5, 0), 100);
    }
    // ch2 mode 1 triggered by gate rising edge
    let mut q = Pit::new();
    load(&mut q, 0, 2, 1, 1000);
    q.set_gate(10, 2, true);
    assert!(q.out(10, 2) == false);
    assert!(q.out(10 + 2000, 2));
}

#[test]
fn mode3_odd_count_and_status_readback() {
    let mut p = Pit::new();
    load(&mut p, 0, 0, 3, 5);
    // 5 clocks per period: x=0..2 high: 5,3,1 then second half 4,2
    let us = |clk: u64| (clk * 1_000_000 + 1_193_181) / 1_193_182;
    let vals: Vec<u32> = (0..5).map(|c| latch_read(&mut p, us(c), 0)).collect();
    assert_eq!(vals, vec![5, 3, 1, 4, 2]);
    // read-back status ch0: bit7 out, mode bits
    p.write(0, 3, 0xc2 | 0x20); // read-back, status only, channel 0
    let st = p.read(0, 0);
    assert_eq!((st >> 1) & 7, 3);
    assert_eq!((st >> 4) & 3, 3);
}

#[test]
fn write_while_running_takes_effect_at_period_end() {
    let mut p = Pit::new();
    load(&mut p, 0, 0, 2, 1000);
    p.write(100, 0, 0xe8); p.write(100, 0, 0x03); // new count 1000 -> 1000? use 1000 then 2000
    p.write(100, 0, 0xd0); p.write(100, 0, 0x07); // 2000
    // still period of 1000 until its end
    let v = latch_read(&mut p, 500, 0);
    assert!(v <= 1000 && v > 0);
    let v2 = latch_read(&mut p, 1_000 + 1_000, 0); // after end of first period (838 us): in a 2000 period
    assert!(v2 > 1000 && v2 <= 2000, "v2 {v2}");
}

#[test]
fn hostile_all_control_words_all_counts_no_abort() {
    let mut polls = 0u64;
    for cw in 0..=255u32 {
        for &cnt in &[0u32, 1, 2, 3, 65535] {
            let mut p = Pit::new();
            let mut t = 1_000u64;
            p.write(t, 3, cw);
            // write count to the channel addressed (or all, for read-back words)
            for ch in 0..3u32 {
                p.write(t, ch, cnt & 0xff);
                p.write(t, ch, cnt >> 8);
                p.write(t, ch, 0xff);
            }
            p.set_gate(t, 2, true);
            p.set_gate(t, 2, false);
            while t < 21_000 {
                if p.take_one(t) { polls += 1; }
                for a in 0..4 { let _ = p.read(t, a); }
                let _ = p.out(t, 0); let _ = p.out(t, 2);
                let _ = p.next_deadline(t);
                p.write(t, 3, cw ^ (t as u32 & 0xff));
                p.write(t, (t as u32) & 3, (t >> 3) as u32);
                t += 997;
            }
        }
    }
    assert!(polls > 0);
}

#[test]
fn warp_collapse_stops_at_deadline_and_charges_time() {
    let mut p = Pit::new();
    load(&mut p, 0, 0, 2, 16384); // 13 733 us period
    let mut w = Warp::new();
    let dl = p.next_deadline(0);
    assert_eq!(dl, 13_732);
    let k = w.collapse(&mut p, 1_000, 100_000_000, 15);
    // may skip at most (13733-1000)*15 iterations, guest time lands on the deadline
    assert_eq!(k as u64, (dl - 1_000) * 15);
    assert_eq!(w.gt_us(1_000), dl);
    assert!(p.take_one(w.gt_us(1_000)));
    // small request fully honoured
    let mut w2 = Warp::new();
    assert_eq!(w2.collapse(&mut Pit::new(), 0, 150, 15), 150);
    assert_eq!(w2.gt_us(0), 10);
}

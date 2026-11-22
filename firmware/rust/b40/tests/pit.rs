use b40::Timer as Pit;
use b40::PIT_HZ;
#[allow(dead_code)]
fn us_to_clk(us: u64) -> u64 { us * PIT_HZ / 1_000_000 }

const VEC: &str = include_str!("pit_vectors.txt");

fn prog(p: &mut Pit, now: u64, ch: u32, mode: u32, count: u32) {
    p.write(now, 0x43, (ch << 6) | (3 << 4) | (mode << 1));
    p.write(now, 0x40 + ch, count & 255);
    p.write(now, 0x40 + ch, count >> 8);
}
fn read_word(p: &mut Pit, now: u64) -> u32 {
    p.write(now, 0x43, 0x00);
    let lo = p.read(now, 0x40);
    lo | (p.read(now, 0x40) << 8)
}

#[test]
fn counter_vectors() {
    let mut n = 0;
    for l in VEC.lines() {
        if l.starts_with('#') || l.starts_with("IRQ0") || l.is_empty() { continue; }
        let f: Vec<u64> = l.split_whitespace().map(|x| x.parse().unwrap()).collect();
        let (mode, count, t, exp) = (f[0] as u32, f[1] as u32, f[2], f[3] as i64);
        // start the load at a time that makes now+t cross 2^32 us for the long ones
        let t0 = 4_294_000_000u64;
        let mut p = Pit::new();
        prog(&mut p, t0, 0, mode, count);
        let got = read_word(&mut p, t0 + t) as i64;
        let diff = (got - exp).abs().min((got - exp + 65536).abs()).min((got - exp - 65536).abs());
        assert!(diff <= 1, "mode {mode} count {count} t {t}: got {got} want {exp}");
        // aliases 6/7 must behave as 2/3
        if mode == 2 || mode == 3 {
            let mut q = Pit::new();
            prog(&mut q, t0, 0, mode + 4, count);
            assert_eq!(read_word(&mut q, t0 + t) as i64, got);
        }
        n += 1;
    }
    assert!(n >= 100, "{n}");
}

/// drain ticks the way the C glue does: one per poll while owed
fn run_ticks(p: &mut Pit, t0: u64, t1: u64, step: u64) -> u64 {
    let mut k = 0;
    let mut t = t0;
    while t <= t1 {
        while p.owed(t) > 0 { p.take_tick(); k += 1; }
        t += step;
    }
    while p.owed(t1) > 0 { p.take_tick(); k += 1; }
    k
}

#[test]
fn irq0_vectors() {
    for l in VEC.lines().filter(|l| l.starts_with("IRQ0")) {
        let f: Vec<u64> = l.split_whitespace().skip(1).map(|x| x.parse().unwrap()).collect();
        let (count, t) = (f[0] as u32, f[1]);
        for mode in [2u32, 3, 6, 7] {
            let mut p = Pit::new();
            prog(&mut p, 1000, 0, mode, count);
            let got = run_ticks(&mut p, 1000, 1000 + t, 997) as i64; // 997 us poll, coarse
            let want = (t as u128 * 1193182 / (count as u128 * 1_000_000)) as i64;
            assert!(got == want || got == want - 1, "mode {mode} count {count} T {t}: got {got} want {want}");
        }
    }
}

#[test]
fn wrap_and_24h() {
    // 24 h at count 16384: ticks == floor(86400e6*F/(16384e6)), no phase shift at 2^32 us (the old model reloaded at 2^31).
    let day = 86_400_000_000u64;
    let mut p = Pit::new();
    prog(&mut p, 0, 0, 2, 16384);
    let got = run_ticks(&mut p, 0, day, 1_000_003);
    let want = (day as u128 * 1193182 / (16384u128 * 1_000_000)) as u64;
    assert!(got == want || got + 1 == want, "{got} {want}");
    // counter read stays vector-exact late in the day
    let t = day - 12345;
    let d = us_to_clk(t);
    let exp = (16384 - (d % 16384)) as u32;
    let c = read_word(&mut p, t);
    assert_eq!(c, exp);
}

#[test]
fn stall_is_recovered_not_lost() {
    // 1 s host stall at idle rate: all ticks come later; cumulative count equals the ideal.
    let mut p = Pit::new();
    prog(&mut p, 0, 0, 2, 16384);
    let a = run_ticks(&mut p, 0, 5_000_000, 1400);
    let mut t = 5_000_000;
    t += 1_000_000; // stall
    let mut late_burst = 0;
    while p.owed(t) > 0 { p.take_tick(); late_burst += 1; }
    let b = run_ticks(&mut p, t, 20_000_000, 1400);
    let total = a + late_burst + b;
    let want = (20_000_000u128 * 1193182 / (16384u128 * 1_000_000)) as u64;
    assert!(total == want || total + 1 == want, "{total} vs {want}");
    assert_eq!(p.lost(), 0);
}

#[test]
fn hostile_all_control_words() {
    // 256 control words x counts {0,1,2,3,65535} x 20 ms of run, plus reads/latches/gates: no panic, no overflow.
    let counts = [0u32, 1, 2, 3, 65535];
    for cw in 0..256u32 {
        for &cnt in &counts {
            let mut p = Pit::new();
            let mut t = 1u64 << 31;
            p.write(t, 0x43, cw);
            p.write(t, 0x40, cnt & 255);
            p.write(t, 0x41, cnt & 255);
            p.write(t, 0x42, cnt & 255);
            p.write(t, 0x40, cnt >> 8);
            p.write(t, 0x41, cnt >> 8);
            p.write(t, 0x42, cnt >> 8);
            for c in 0..3 { p.set_gate(t, c, false); p.set_gate(t + 5, c, true); }
            let end = t + 20_000;
            while t < end {
                p.owed(t); if p.owed(t) > 0 { p.take_tick(); }
                for port in 0..4 { p.read(t, 0x40 + port); }
                p.write(t, 0x43, 0xC2 | ((cw & 0x3f) << 0) as u32);
                p.out(t, 0); p.out(t, 2); p.next_deadline(t);
                t += 997;
            }
        }
    }
}

#[test]
fn warp_advances_counter_and_ticks() {
    let mut p = Pit::new();
    prog(&mut p, 0, 0, 2, 16384);
    p.warp(1_000_000);
    let got = run_ticks(&mut p, 0, 1, 1);
    assert_eq!(got, 72); // 1 s of virtual time owed at once, delivered as a burst
}

#[test]
fn next_deadline_matches_edges() {
    let mut p = Pit::new();
    prog(&mut p, 5000, 0, 2, 16384);
    let mut prev = 5000;
    for _ in 0..1000 {
        let d = p.next_deadline(prev).unwrap();
        assert_eq!(p.owed(d - 1), 0, "early");
        assert_eq!(p.owed(d), 1, "late at {d}");
        p.take_tick();
        prev = d;
    }
}

/// guest model: step 1365 us (2048 instr at 1.5 MIPS), ISR takes 2 steps, PIC blocks while IRQ0 pending/in service.
/// Returns (delivered, ideal, min raise gap while backlog existed, lost)
fn guest_run(stalls: &[(u64, u64)], total_us: u64) -> (u64, u64, u64, u64) {
    let mut p = Pit::new();
    prog(&mut p, 0, 0, 2, 16384);
    let (mut now, mut irr, mut isr, mut st, mut deliv) = (0u64, false, false, 0, 0u64);
    let (mut min_gap, mut last) = (u64::MAX, 0u64);
    let mut si = 0;
    while now < total_us {
        if p.poll_raise(now, irr || isr) { irr = true; }
        if isr { st += 1; if st >= 2 { isr = false; st = 0; } }
        if irr && !isr {
            irr = false; isr = true; st = 0; deliv += 1;
            if last > 0 && p.owed(now) > 0 { min_gap = min_gap.min(now - last); }
            last = now;
        }
        now += 1365;
        if si < stalls.len() && now >= stalls[si].0 { now += stalls[si].1; si += 1; }
    }
    (deliv, (now as u128 * 1193182 / (16384u128 * 1_000_000)) as u64, min_gap, p.lost())
}

#[test]
fn paced_catchup_after_stall_no_loss_no_storm() {
    let st: Vec<(u64, u64)> = (1..300).map(|i| (i * 2_000_000, 30_000)).collect();
    let (d, want, gap, lost) = guest_run(&st, 600_000_000);
    assert!(d + 3 >= want && d <= want + 1, "{d} vs {want}");
    assert_eq!(lost, 0);
    assert!(gap >= 2 * 1365, "tick storm: gap {gap}");
}

/// judge R2_B40 stall run as a regression: 3 s stall at t = 8 s (boot), then 5 s / 4 s / 0.8 s stalls at idle.
/// RED v1 lost 98 ticks to its 4 s cap (-0.77 %); BLUE hung without pacing. Merged model: nothing lost, no storm.
#[test]
fn judge_boot_and_idle_stalls_regression() {
    let st = [(8_000_000u64, 3_000_000u64), (60_000_000, 5_000_000), (110_000_000, 4_000_000), (150_000_000, 800_000)];
    let (d, want, gap, lost) = guest_run(&st, 200_000_000);
    assert_eq!(lost, 0, "backlog cap must not drop ticks on a 5 s stall");
    assert!(d + 3 >= want && d <= want + 1, "clock error: {d} vs {want}");
    assert!(gap >= 2 * 1365, "tick storm after stall: gap {gap}");
}

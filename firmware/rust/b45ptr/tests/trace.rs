// Scripted-trace test (deliverable 2): every traces/*.trace is replayed through the engine in virtual time and the
// Windows events are compared bit-exact with traces/*.expect (hand-written or built by the python generator, never by
// this engine).  Trace lines:  T t_ms down rx ry   (GSL3680 report, raw chip coords)
//                              R t_ms dx dy buttons (USB relative report)
//                              A t_ms x y buttons   (set_pointer_abs: exact pixel, never merged)
//                              C key value         (config override: rotate180 0|1)
// Timing model (same on the board replay): ticks every 4 ms boundary strictly between two input times, the guest
// drains the queue after every call.  Output line: t_us flags(hex) px py x16 y16 ; .expect lists t_us flags px py.
use b45ptr::*;
use std::fs;
use std::path::Path;

const TICK_US: u32 = 4000;

fn run(src: &str) -> (Vec<Ev>, Engine) {
    let mut cfg = DEFAULT_CFG;
    for l in src.lines() {
        let p: Vec<&str> = l.split_whitespace().collect();
        if p.len() == 3 && p[0] == "C" && p[1] == "rotate180" { cfg.rotate180 = p[2].parse().unwrap(); }
    }
    let mut e = Engine::new(cfg);
    let mut out = Vec::new();
    let mut prev: u32 = 0;
    let mut first = true;
    let drain = |e: &mut Engine, out: &mut Vec<Ev>| { while let Some(v) = e.pop() { out.push(v); } };
    let ticks_to = |e: &mut Engine, out: &mut Vec<Ev>, prev: u32, t: u32| {
        let mut b = (prev / TICK_US + 1) * TICK_US;
        while b < t { e.tick(b); drain(e, out); b += TICK_US; }
    };
    for l in src.lines() {
        let p: Vec<&str> = l.split_whitespace().collect();
        if p.is_empty() || p[0].starts_with('#') || p[0] == "C" { continue; }
        let t: u32 = p[1].parse::<u32>().unwrap() * 1000;
        if !first { ticks_to(&mut e, &mut out, prev, t); }
        first = false;
        match p[0] {
            "T" => e.touch(t, p[2] == "1", p[3].parse().unwrap(), p[4].parse().unwrap()),
            "A" => e.set_abs(t, p[2].parse().unwrap(), p[3].parse().unwrap(), p[4].parse().unwrap()),
            "R" => e.rel(t, p[2].parse().unwrap(), p[3].parse().unwrap(), p[4].parse().unwrap()),
            x => panic!("bad line {}", x),
        }
        drain(&mut e, &mut out);
        prev = t;
    }
    ticks_to(&mut e, &mut out, prev, prev + 1_000_000);
    (out, e)
}

fn fnv(evs: &[Ev]) -> u64 {
    let mut h: u64 = 0xcbf29ce484222325;
    for v in evs {
        for b in v.t_us.to_le_bytes().iter().chain(v.flags.to_le_bytes().iter()).chain(v.x16.to_le_bytes().iter()).chain(v.y16.to_le_bytes().iter()) {
            h ^= *b as u64; h = h.wrapping_mul(0x100000001b3);
        }
    }
    h
}

#[test]
fn traces() {
    let dir = Path::new(env!("CARGO_MANIFEST_DIR")).join("traces");
    let outd = dir.join("out");
    fs::create_dir_all(&outd).unwrap();
    let mut names: Vec<_> = fs::read_dir(&dir).unwrap().filter_map(|d| d.ok()).map(|d| d.path())
        .filter(|p| p.extension().map_or(false, |x| x == "trace")).collect();
    names.sort();
    assert!(!names.is_empty());
    let mut fails = 0;
    let mut summary = String::new();
    for p in &names {
        let name = p.file_stem().unwrap().to_string_lossy().to_string();
        let (evs, e) = run(&fs::read_to_string(p).unwrap());
        let mut txt = String::new();
        for v in &evs { txt += &format!("{} {:04x} {} {} {} {}\n", v.t_us, v.flags, v.px, v.py, v.x16, v.y16); }
        fs::write(outd.join(format!("{}.out", name)), &txt).unwrap();
        let got: Vec<String> = evs.iter().map(|v| format!("{} {:04x} {} {}", v.t_us, v.flags, v.px, v.py)).collect();
        let exp_path = p.with_extension("expect");
        let verdict = match fs::read_to_string(&exp_path) {
            Ok(x) => {
                let exp: Vec<String> = x.lines().map(|l| l.trim()).filter(|l| !l.is_empty() && !l.starts_with('#')).map(|l| l.split_whitespace().collect::<Vec<_>>().join(" ")).collect();
                if exp == got { "PASS".to_string() } else {
                    fails += 1;
                    let i = exp.iter().zip(got.iter()).position(|(a, b)| a != b).unwrap_or(exp.len().min(got.len()));
                    format!("FAIL first diff at event {}: expect {:?} got {:?} (n exp {} got {})", i, exp.get(i), got.get(i), exp.len(), got.len())
                }
            }
            Err(_) => { fails += 1; "FAIL no .expect".to_string() }
        };
        let downs = evs.iter().filter(|v| v.flags & SF_LDOWN != 0).count();
        let rights = evs.iter().filter(|v| v.flags & SF_RDOWN != 0).count();
        summary += &format!("{:10} {} events={} ldown={} rdown={} taps={} drags={} longs={} overflow={} fnv={:016x}\n",
            name, verdict, evs.len(), downs, rights, e.taps, e.drags, e.longs, e.overflow, fnv(&evs));
    }
    print!("{}", summary);
    fs::write(outd.join("SUMMARY.txt"), &summary).unwrap();
    assert_eq!(fails, 0, "{}", summary);
}

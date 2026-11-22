//! INT 09h (scancode -> BDA ring buffer) and INT 16h (read/peek/flags).  US layout, set-1 scancodes
//! (the i8042 translates).  State lives in the BDA exactly where DOS programs expect it.
use crate::*;

const B_FLAGS1: u32 = 0x17; // b7 ins b6 caps b5 num b4 scroll b3 alt b2 ctrl b1 lshift b0 rshift
const B_FLAGS2: u32 = 0x18; // held: b6 caps b5 num b4 scroll, b1 lalt, b0 lctrl
const B_HEAD: u32 = 0x1A;
const B_TAIL: u32 = 0x1C;
const B_KB3: u32 = 0x96; // b3 ralt b2 rctrl b1 E0 prefix seen b0 E1 sequence running, b7..4 = E1 byte count
const B_RESET: u32 = 0x72;
const ALTNUM: u32 = 0xB8; // Alt+keypad accumulator (intra-application area 0040:00B8)

// ctrl / alt scancodes of the navigation keypad 0x47..=0x53
const NAV_CTRL: [u8; 13] = [0x77, 0x8D, 0x84, 0x8E, 0x73, 0x8F, 0x74, 0x90, 0x75, 0x91, 0x76, 0x92, 0x93];
const NAV_ALT: [u8; 13] = [0x97, 0x98, 0x99, 0x4A, 0x9B, 0x4C, 0x9D, 0x4E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3];

//            normal, shifted  for scancodes 0x00..=0x39
const MAP: [(u8, u8); 0x3A] = {
    let mut m = [(0u8, 0u8); 0x3A];
    m[0x01] = (0x1B, 0x1B);
    let n = *b"1234567890-=";
    let s = *b"!@#$%^&*()_+";
    let mut i = 0;
    while i < 12 {
        m[2 + i] = (n[i], s[i]);
        i += 1;
    }
    m[0x0E] = (8, 8);
    m[0x0F] = (9, 0);
    let q = *b"qwertyuiop[]";
    let qs = *b"QWERTYUIOP{}";
    i = 0;
    while i < 12 {
        m[0x10 + i] = (q[i], qs[i]);
        i += 1;
    }
    m[0x1C] = (13, 13);
    let a = *b"asdfghjkl;'`";
    let a_s = *b"ASDFGHJKL:\"~";
    i = 0;
    while i < 12 {
        m[0x1E + i] = (a[i], a_s[i]);
        i += 1;
    }
    m[0x2B] = (b'\\', b'|');
    let z = *b"zxcvbnm,./";
    let zs = *b"ZXCVBNM<>?";
    i = 0;
    while i < 10 {
        m[0x2C + i] = (z[i], zs[i]);
        i += 1;
    }
    m[0x37] = (b'*', b'*');
    m[0x39] = (b' ', b' ');
    m
};

/// scancode -> BIOS key word (scan<<8 | ascii), None = no key.  Layout logic merged with the RED team's table work.
fn translate(cx: &Cx, code: u8, ext: bool, f1: u8) -> Option<u16> {
    let shift = f1 & 3 != 0;
    let ctrl = f1 & 4 != 0;
    let alt = f1 & 8 != 0;
    let caps = f1 & 0x40 != 0;
    let num = f1 & 0x20 != 0;
    let _ = cx;
    let sc = code as u16;
    if (0x3B..=0x44).contains(&code) {
        let base = if alt { 0x68 } else if ctrl { 0x5E } else if shift { 0x54 } else { 0x3B };
        return Some((base + (code - 0x3B) as u16) << 8);
    }
    if code == 0x57 || code == 0x58 {
        let base = if alt { 0x8B } else if ctrl { 0x89 } else if shift { 0x87 } else { 0x85 };
        return Some((base + (code - 0x57) as u16) << 8);
    }
    if (0x47..=0x53).contains(&code) && code != 0x4A && code != 0x4E {
        let i = (code - 0x47) as usize;
        if ext || !(num ^ shift) || ctrl || alt {
            if alt && !ext { return None; }
            let asc = if ext && !ctrl && !alt { 0xE0 } else { 0 };
            return Some(if ctrl { (NAV_CTRL[i] as u16) << 8 } else if alt { (NAV_ALT[i] as u16) << 8 } else { sc << 8 | asc });
        }
        const DIG: [u8; 13] = *b"789-456+1230.";
        return Some(sc << 8 | DIG[i] as u16);
    }
    if code == 0x4A || code == 0x4E { return Some(sc << 8 | if code == 0x4A { b'-' } else { b'+' } as u16); }
    if code == 0x37 { return Some(if ext { 0 } else { sc << 8 | b'*' as u16 }); }
    if code == 0x35 && ext { return Some(0xE02F); }
    if code == 0x1C && ext { return Some(if ctrl { 0xE00A } else { 0xE00D }); }
    if code as usize >= MAP.len() { return None; }
    let (n, s) = MAP[code as usize];
    match code {
        0x01 => return Some(if alt { 0x0100 } else { 0x011B }),
        0x0E => return Some(if alt { 0x0E00 } else if ctrl { 0x0E7F } else { 0x0E08 }),
        0x0F => return Some(if alt { 0xA500 } else if ctrl { 0x9400 } else if shift { 0x0F00 } else { 0x0F09 }),
        0x1C => return Some(if alt { 0x1C00 } else if ctrl { 0x1C0A } else { 0x1C0D }),
        0x39 => return Some(0x3920),
        _ => {}
    }
    if n == 0 { return None; }
    let letter = n.is_ascii_lowercase();
    if alt {
        if (0x02..=0x0D).contains(&code) { return Some((code as u16 + 0x76) << 8); }
        return Some(sc << 8);
    }
    if ctrl {
        if letter { return Some(sc << 8 | (n & 0x1F) as u16); }
        return match code {
            0x1A => Some(sc << 8 | 0x1B), 0x2B => Some(sc << 8 | 0x1C), 0x1B => Some(sc << 8 | 0x1D),
            0x07 => Some(sc << 8 | 0x1E), 0x0C => Some(sc << 8 | 0x1F), 0x03 => Some(0x0300),
            _ => None,
        };
    }
    let up = if letter { shift ^ caps } else { shift };
    Some(sc << 8 | if up { s } else { n } as u16)
}

fn push(cx: &mut Cx, w: u16) -> bool {
    let tail = cx.bda16(B_TAIL) as u32;
    let mut next = tail + 2;
    if next >= 0x3E {
        next = 0x1E;
    }
    if next == cx.bda16(B_HEAD) as u32 {
        return false; // full: drop (a real BIOS beeps)
    }
    cx.set_bda16(tail, w);
    cx.set_bda16(B_TAIL, next as u16);
    true
}

pub fn int09(cx: &mut Cx) {
    let e = env();
    let sc = (e.pio_in)(0x60, 1) as u8;
    (e.pio_out)(0x20, 1, 0x20);
    let mut kb3 = cx.bda8(B_KB3);
    if sc == 0xE0 {
        cx.set_bda8(B_KB3, kb3 | 2);
        return;
    }
    if sc == 0xE1 {
        cx.set_bda8(B_KB3, (kb3 & 0x0F) | 1); // Pause: swallow the next 5 bytes
        return;
    }
    if kb3 & 1 != 0 {
        let cnt = (kb3 >> 4) + 1;
        if cnt >= 5 {
            cx.set_bda8(B_KB3, kb3 & 0x0C);
        } else {
            cx.set_bda8(B_KB3, (kb3 & 0x0F) | (cnt << 4));
        }
        return;
    }
    let ext = kb3 & 2 != 0;
    kb3 &= !2;
    cx.set_bda8(B_KB3, kb3);
    let make = sc & 0x80 == 0;
    let code = sc & 0x7F;
    let mut f1 = cx.bda8(B_FLAGS1);
    let mut f2 = cx.bda8(B_FLAGS2);
    if ext && (code == 0x2A || code == 0x36) {
        return; // fake shifts around PrintScreen / arrows
    }
    match code {
        0x2A | 0x36 => {
            let bit = if code == 0x2A { 2 } else { 1 };
            if make { f1 |= bit } else { f1 &= !bit }
            cx.set_bda8(B_FLAGS1, f1);
            return;
        }
        0x1D => {
            if ext {
                if make { kb3 |= 4 } else { kb3 &= !4 }
            } else if make {
                f2 |= 1
            } else {
                f2 &= !1
            }
            if (f2 & 1 != 0) || (kb3 & 4 != 0) { f1 |= 4 } else { f1 &= !4 }
            cx.set_bda8(B_FLAGS1, f1);
            cx.set_bda8(B_FLAGS2, f2);
            cx.set_bda8(B_KB3, kb3);
            return;
        }
        0x38 => {
            if ext {
                if make { kb3 |= 8 } else { kb3 &= !8 }
            } else if make {
                f2 |= 2
            } else {
                f2 &= !2
            }
            if (f2 & 2 != 0) || (kb3 & 8 != 0) { f1 |= 8 } else { f1 &= !8 }
            cx.set_bda8(B_FLAGS1, f1);
            cx.set_bda8(B_FLAGS2, f2);
            cx.set_bda8(B_KB3, kb3);
            if !make && f1 & 8 == 0 {
                let n = cx.bda8(ALTNUM);
                if n != 0 {
                    cx.set_bda8(ALTNUM, 0);
                    push(cx, n as u16);
                }
            }
            return;
        }
        0x3A | 0x45 | 0x46 => {
            let bit = match code {
                0x3A => 0x40,
                0x45 => 0x20,
                _ => 0x10,
            };
            if make {
                if f2 & bit == 0 {
                    f1 ^= bit;
                    f2 |= bit;
                }
            } else {
                f2 &= !bit;
            }
            cx.set_bda8(B_FLAGS1, f1);
            cx.set_bda8(B_FLAGS2, f2);
            return;
        }
        _ => {}
    }
    if !make {
        return;
    }
    let shift = f1 & 3 != 0;
    let ctrl = f1 & 4 != 0;
    let alt = f1 & 8 != 0;
    if code == 0x53 && ctrl && alt {
        cx.set_bda16(B_RESET, 0x1234); // Ctrl+Alt+Del: warm reset
        (e.reset)();
        return;
    }
    // Alt + keypad digits compose a character (released with Alt)
    if alt && !ext && (0x47..=0x53).contains(&code) {
        const D: [i8; 13] = [7, 8, 9, -1, 4, 5, 6, -1, 1, 2, 3, 0, -1];
        let d = D[(code - 0x47) as usize];
        if d >= 0 {
            let n = cx.bda8(ALTNUM);
            cx.set_bda8(ALTNUM, n.wrapping_mul(10).wrapping_add(d as u8));
        }
        return;
    }
    if let Some(k) = translate(cx, code, ext, f1) {
        push(cx, k);
    }
}

fn ready(cx: &Cx) -> Option<u16> {
    let head = cx.bda16(B_HEAD) as u32;
    if head == cx.bda16(B_TAIL) as u32 { None } else { Some(cx.bda16(head)) }
}

pub fn int16(cx: &mut Cx) {
    let ah = cx.ah();
    match ah {
        0x00 | 0x10 => match ready(cx) {
            None => cx.live_flags(CF, 0), // stub: hlt + retry
            Some(mut w) => {
                if ah == 0 && (w & 0xFF) == 0xE0 {
                    w &= 0xFF00;
                }
                let head = cx.bda16(B_HEAD) as u32 + 2;
                cx.set_bda16(B_HEAD, if head >= 0x3E { 0x1E } else { head as u16 });
                cx.set16(EAX, w);
                cx.live_flags(0, CF);
            }
        },
        0x01 | 0x11 => {
            cx.live_flags(0, CF);
            match ready(cx) {
                None => cx.ret_flags(ZF, 0),
                Some(mut w) => {
                    if ah == 1 && (w & 0xFF) == 0xE0 {
                        w &= 0xFF00;
                    }
                    cx.set16(EAX, w);
                    cx.ret_flags(0, ZF);
                }
            }
        }
        0x02 => {
            let f = cx.bda8(B_FLAGS1);
            cx.set_al(f);
            cx.live_flags(0, CF);
        }
        0x12 => {
            let f1 = cx.bda8(B_FLAGS1) as u16;
            let f2 = (cx.bda8(B_FLAGS2) & 0x73) as u16 | ((cx.bda8(B_KB3) & 0x0C) as u16);
            cx.set16(EAX, (f2 << 8) | f1);
            cx.live_flags(0, CF);
        }
        0x05 => {
            let w = cx.cx();
            let ok = push(cx, w);
            cx.set_al(if ok { 0 } else { 1 });
            cx.live_flags(0, CF);
        }
        _ => cx.live_flags(0, CF),
    }
}

//! INT 15h (memory size, A20, block move, config table) and INT 1Ah (tick, RTC, PCI BIOS).
use crate::*;

fn cmos_read(r: u32) -> u8 {
    let e = env();
    (e.pio_out)(0x70, 1, r);
    (e.pio_in)(0x71, 1) as u8
}
fn cmos_write(r: u32, v: u8) {
    let e = env();
    (e.pio_out)(0x70, 1, r);
    (e.pio_out)(0x71, 1, v as u32);
}

/// total RAM in bytes, from the CMOS memory size bytes the emulated board fills in
fn ram_bytes() -> u64 {
    let lo = cmos_read(0x30) as u64 | (cmos_read(0x31) as u64) << 8; // KB above 1 MB (up to 64 MB)
    let hi = cmos_read(0x34) as u64 | (cmos_read(0x35) as u64) << 8; // 64 KB blocks above 16 MB
    if hi != 0 { (hi << 16) + (16 << 20) } else { (lo << 10) + (1 << 20) }
}

const SYSCONF: u32 = 0xFE6F5; // F000:E6F5, built by bios.S

pub fn int15(cx: &mut Cx) {
    let ah = cx.ah();
    let ram = ram_bytes();
    match ah {
        0x88 => {
            // KB above 1 MB, capped at 63 MB.  (A DOS box with HIMEM loaded sees AX=0 because HIMEM hooks this call;
            // HIMEM itself must get the real figure or it decides there is no extended memory.)
            let kb = ((ram - (1 << 20)) >> 10).min(63 * 1024);
            cx.set16(EAX, kb as u16);
            cx.ok();
        }
        0xE8 => match cx.al() {
            0x01 => {
                let below = (((ram.min(16 << 20)) - (1 << 20)) >> 10) as u16; // KB 1M..16M
                let above = (ram.saturating_sub(16 << 20) >> 16) as u16; // 64 KB blocks above 16M
                cx.set16(EAX, below);
                cx.set16(ECX, below);
                cx.set16(EBX, above);
                cx.set16(EDX, above);
                cx.ok();
            }
            0x20 => {
                if cx.r(EDX) != 0x534D_4150 || cx.r(ECX) < 20 {
                    cx.fail(0x86);
                    return;
                }
                let idx = cx.r(EBX);
                let ent: [(u64, u64, u32); 5] = [
                    (0, 0x9FC00, 1),
                    (0x9FC00, 0x400, 2),
                    (0xF0000, 0x10000, 2),
                    (0x10_0000, ram - 0x10_0000, 1),
                    (0xFFFC_0000, 0x4_0000, 2),
                ];
                if idx as usize >= 5 {
                    cx.fail(0x86);
                    return;
                }
                let (b, l, t) = ent[idx as usize];
                let a = cx.seg(ES).wrapping_add(cx.di() as u32);
                cx.wr32(a, b as u32);
                cx.wr32(a + 4, (b >> 32) as u32);
                cx.wr32(a + 8, l as u32);
                cx.wr32(a + 12, (l >> 32) as u32);
                cx.wr32(a + 16, t);
                cx.set32(EAX, 0x534D_4150);
                cx.set32(ECX, 20);
                cx.set32(EBX, if idx == 4 { 0 } else { idx + 1 });
                cx.ok();
            }
            _ => cx.fail(0x86),
        },
        0xC0 => {
            cx.set_ah(0);
            let es = 0xF000u32;
            // ES:BX = F000:E6F5 ; segment register edit is not supported by the host API, so BX/ES are
            // returned through the stacked-frame-free route below
            let _ = (es, SYSCONF);
            cx.fail(0x86); // replaced by guest side stub if needed (see REPORT: C0 unsupported for now)
        }
        0x24 => match cx.al() {
            0x00 => { (env().pio_out)(0x92, 1, (env().pio_in)(0x92, 1) & !2); cx.set_ah(0); cx.ok(); }
            0x01 => { (env().pio_out)(0x92, 1, (env().pio_in)(0x92, 1) | 2); cx.set_ah(0); cx.ok(); }
            0x02 => { cx.set_ah(0); cx.set_al(((env().pio_in)(0x92, 1) >> 1 & 1) as u8); cx.ok(); }
            0x03 => { cx.set_ah(0); cx.set16(EBX, 3); cx.ok(); }
            _ => cx.fail(0x86),
        },
        0x87 => {
            // extended memory block move: ES:SI -> GDT, source descriptor at +0x10, destination at +0x18, CX words
            let g = cx.seg(ES).wrapping_add(cx.si() as u32);
            let base = |o: u32| -> u32 { cx.rd16(g + o + 2) as u32 | (cx.rd8(g + o + 4) as u32) << 16 | (cx.rd8(g + o + 7) as u32) << 24 };
            let (s, d) = (base(0x10), base(0x18));
            let n = cx.cx() as u32 * 2;
            for i in 0..n {
                let v = match cx.phys(s + i, false) { Some(p) => unsafe { *env().mem.add(p) }, None => 0 };
                if let Some(p) = cx.phys(d + i, true) { unsafe { *env().mem.add(p) = v } }
            }
            cx.set_ah(0);
            cx.ret_flags(ZF, CF);
        }
        0xC2 => mouse(cx),
        0x53 => apm(cx),
        0x86 | 0x83 => { cx.set_ah(0); cx.ok(); } // timed waits: complete at once (see REPORT)
        _ => cx.fail(0x86),
    }
}

fn bcd_ok(v: u8) -> u8 { v }

fn pci_addr(bdf: u16, reg: u32) -> u32 {
    0x8000_0000 | (bdf as u32) << 8 | (reg & 0xFC)
}
fn pci_rd(bdf: u16, reg: u32, size: u32) -> u32 {
    let e = env();
    (e.pio_out)(0xCF8, 4, pci_addr(bdf, reg));
    (e.pio_in)(0xCFC + (reg & 3), size)
}
fn pci_wr(bdf: u16, reg: u32, size: u32, v: u32) {
    let e = env();
    (e.pio_out)(0xCF8, 4, pci_addr(bdf, reg));
    (e.pio_out)(0xCFC + (reg & 3), size, v);
}

pub fn int1a(cx: &mut Cx) {
    let ah = cx.ah();
    match ah {
        0x00 => {
            let lo = cx.bda16(0x6C) as u32;
            let hi = cx.bda16(0x6E) as u32;
            let mid = cx.bda8(0x70);
            cx.set16(ECX, hi as u16);
            cx.set16(EDX, lo as u16);
            cx.set_al(mid);
            cx.set_bda8(0x70, 0);
            cx.ok();
        }
        0x01 => {
            cx.set_bda16(0x6C, cx.dx());
            cx.set_bda16(0x6E, cx.cx());
            cx.set_bda8(0x70, 0);
            cx.set_ah(0);
            cx.ok();
        }
        0x02 => {
            cx.set_ch(bcd_ok(cmos_read(4)));
            cx.set_cl(bcd_ok(cmos_read(2)));
            cx.set_dh(bcd_ok(cmos_read(0)));
            cx.set_dl(0);
            cx.ok();
        }
        0x04 => {
            cx.set_ch(cmos_read(0x32));
            cx.set_cl(cmos_read(9));
            cx.set_dh(cmos_read(8));
            cx.set_dl(cmos_read(7));
            cx.ok();
        }
        0x03 => {
            cmos_write(4, (cx.cx() >> 8) as u8);
            cmos_write(2, cx.cx() as u8);
            cmos_write(0, (cx.dx() >> 8) as u8);
            cx.ok();
        }
        0x05 => {
            cmos_write(0x32, (cx.cx() >> 8) as u8);
            cmos_write(9, cx.cx() as u8);
            cmos_write(8, (cx.dx() >> 8) as u8);
            cmos_write(7, cx.dx() as u8);
            cx.ok();
        }
        0xB1 => pcibios(cx),
        _ => cx.ret_flags(CF, 0),
    }
}

pub fn pcibios(cx: &mut Cx) {
    let f = cx.al();
    match f {
        0x01 => {
            cx.set_ah(0);
            cx.set_al(1); // configuration mechanism 1
            cx.set16(EBX, 0x0210);
            cx.set_cl(0); // last bus
            cx.set32(EDX, 0x2049_4350); // "PCI "
            cx.ok();
        }
        0x02 | 0x03 => {
            let mut idx = cx.si();
            for bus_dev in 0..256u16 {
                // bus 0 only (flat scan of 32 devices x 8 functions)
                if bus_dev >= 32 { break; }
                for func in 0..8u16 {
                    let bdf = (bus_dev << 3) | func;
                    let id = pci_rd(bdf, 0, 4);
                    if id == 0xFFFF_FFFF || id & 0xFFFF == 0xFFFF { if func == 0 { break } else { continue } }
                    let hit = if f == 2 {
                        (id & 0xFFFF) as u16 == cx.dx() && (id >> 16) as u16 == cx.cx()
                    } else {
                        (pci_rd(bdf, 8, 4) >> 8) == cx.r(ECX) & 0x00FF_FFFF
                    };
                    if hit {
                        if idx == 0 {
                            cx.set_ah(0);
                            cx.set16(EBX, bdf);
                            cx.ok();
                            return;
                        }
                        idx -= 1;
                    }
                }
            }
            cx.fail(0x86);
        }
        0x08 | 0x09 | 0x0A => {
            let size = 1u32 << (f - 8);
            let v = pci_rd(cx.bx(), cx.di() as u32, size);
            match size { 1 => cx.set_cl(v as u8), 2 => cx.set16(ECX, v as u16), _ => cx.set32(ECX, v) }
            cx.set_ah(0);
            cx.ok();
        }
        0x0B | 0x0C | 0x0D => {
            let size = 1u32 << (f - 0x0B);
            pci_wr(cx.bx(), cx.di() as u32, size, cx.r(ECX));
            cx.set_ah(0);
            cx.ok();
        }
        _ => cx.fail(0x81),
    }
}

/// INT 15h AH=C2: PS/2 pointing device.  Packets are collected by the guest INT 74h stub (bios.S).
fn mouse(cx: &mut Cx) {
    let e = env();
    let aux = |b: u32| { (e.pio_out)(0x64, 1, 0xD4); (e.pio_out)(0x60, 1, b); };
    let drain = || { for _ in 0..8 { if (e.pio_in)(0x64, 1) & 1 == 0 { break; } (e.pio_in)(0x60, 1); } };
    match cx.al() {
        0x00 => {
            if (cx.bx() >> 8) == 1 { // BH = 1 enable
                (e.pio_out)(0x64, 1, 0xA8);
                (e.pio_out)(0x64, 1, 0x60); (e.pio_out)(0x60, 1, 0x47); // KBD+AUX irq on, translation
                aux(0xF4); drain();
            } else {
                aux(0xF5); drain();
                (e.pio_out)(0x64, 1, 0xA7);
                (e.pio_out)(0x64, 1, 0x60); (e.pio_out)(0x60, 1, 0x61);
            }
            cx.set_ah(0); cx.ok();
        }
        0x01 => {
            (e.pio_out)(0x64, 1, 0xA8);
            aux(0xFF); drain();
            cx.set_bl(0xAA); cx.set32(EBX, cx.r(EBX) & 0xFFFF_00FF);
            cx.set_ah(0); cx.ok();
        }
        0x02 | 0x03 | 0x05 => { cx.set_ah(0); cx.ok(); }
        0x04 => { cx.set32(EBX, cx.r(EBX) & 0xFFFF_00FF); cx.set_ah(0); cx.ok(); }
        0x06 => { cx.set_bl(0); cx.set_cl(2); cx.set_dl(100); cx.set_ah(0); cx.ok(); }
        0x07 => {
            let seg = cx.st[16 + ES] as u16;
            cx.set_bda16(0xF8, cx.bx());
            cx.set_bda16(0xFA, seg);
            cx.set_bda16(0xF2, 0);
            cx.set_ah(0); cx.ok();
        }
        _ => cx.fail(0x86),
    }
}

/// INT 15h AH=53: APM 1.2, 32-bit protected-mode interface only.  The idle call (HLT) lives in the guest
/// stub `apm32_entry` (bios.S); Windows 95 uses it to stop burning the CPU on the idle loop.
fn apm(cx: &mut Cx) {
    match cx.al() {
        0x00 => { cx.set16(EAX, 0x0102); cx.set16(EBX, 0x504D); cx.set16(ECX, 0x0002); cx.ok(); }
        0x07 => apm_power(cx),
        0x01 | 0x04 | 0x05 | 0x06 | 0x08 | 0x09 | 0x0D | 0x0F => { cx.set_ah(0); cx.ok(); }
        0x03 => {
            cx.set16(EAX, 0xF000);
            cx.set32(EBX, crate::syms::APM32_ENTRY as u32);
            cx.set16(ECX, 0xF000);
            cx.set16(EDX, 0xF000);
            cx.set32(ESI, 0xFFFF_FFFF);
            cx.set16(EDI, 0xFFFF);
            cx.ok();
        }
        0x0A => { cx.set32(EBX, 0x01FF); cx.set16(ECX, 0xFFFF); cx.set16(EDX, 0xFFFF); cx.set16(ESI, 0); cx.set_ah(0); cx.ok(); }
        0x0B => cx.fail(0x80),
        0x0E => { cx.set16(EAX, 0x0102); cx.ok(); }
        _ => cx.fail(0x09),
    }
}

/// APM set power state (INT 15h/5307 and the 32-bit entry): BX=0001 (all devices), CX=0003 = off -> host event 1
/// ("safe to power off": the host flushes the disk and stops / hibernates), CX=0002 suspend -> event 2.
pub fn apm_power(cx: &mut Cx) {
    if cx.bx() == 0x0001 {
        match cx.cx() {
            3 => (env().event)(1),
            2 | 1 => (env().event)(2),
            _ => {}
        }
    }
    cx.set_ah(0);
    cx.ok();
}

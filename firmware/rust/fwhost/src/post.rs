//! Instant "POST": the whole machine is known, so nothing is probed.  Builds the state a PC BIOS leaves behind
//! (IVT, BDA, EBDA, A20, PIC/PIT/RTC/KBC) and loads the boot sector to 0000:7C00.  The guest reset stub is
//! `out 0xE0,al` + option-ROM init (placeholder) + jump to 7C00: about 12 guest instructions in total.
use crate::syms::*;
use crate::*;

fn vec(cx: &Cx, v: u32, off: u16) {
    cx.wr16(v * 4, off);
    cx.wr16(v * 4 + 2, 0xF000);
}

pub fn post(cx: &mut Cx) {
    let e = env();
    let o = |p: u32, v: u32| (e.pio_out)(p, 1, v);
    o(0x92, 2); // A20 on, no reset
    let mut a = 0;
    while a < 0x500 {
        cx.wr32(a, 0);
        a += 4;
    }
    for v in 0..256 {
        vec(cx, v, DUMMY_IRET);
    }
    for v in 0x0A..=0x0F { vec(cx, v, IRQ_MASTER_EOI); }
    for v in 0x71..=0x77 { vec(cx, v, IRQ_SLAVE_EOI); }
    vec(cx, 0x08, INT08);
    vec(cx, 0x09, INT09);
    vec(cx, 0x10, INT10);
    vec(cx, 0x11, INT11);
    vec(cx, 0x1F, FONT8 + 1024);
    vec(cx, 0x43, FONT8);
    vec(cx, 0x12, INT12);
    vec(cx, 0x13, INT13);
    vec(cx, 0x14, INT14);
    vec(cx, 0x15, INT15);
    vec(cx, 0x16, INT16);
    vec(cx, 0x17, INT17);
    vec(cx, 0x18, INT18);
    vec(cx, 0x19, INT19);
    vec(cx, 0x1A, INT1A);
    vec(cx, 0x70, INT70);
    vec(cx, 0x74, INT74);
    vec(cx, 0x75, INT75);
    vec(cx, 0x41, HD0_TABLE);
    vec(cx, 0x1E, DISKETTE_TABLE);
    // 8259 pair: master 08h, slave 70h, only timer/keyboard/cascade/FDC unmasked
    o(0x20, 0x11); o(0xA0, 0x11);
    o(0x21, 0x08); o(0xA1, 0x70);
    o(0x21, 0x04); o(0xA1, 0x02);
    o(0x21, 0x01); o(0xA1, 0x01);
    o(0x21, 0xF8); o(0xA1, 0x8E); // masks as the reference leaves them: IRQ14 (IDE) stays enabled
    // 8254: ch0 mode 2 count 0 (18.2 Hz), ch1 refresh
    o(0x43, 0x34); o(0x40, 0); o(0x40, 0);
    o(0x43, 0x54); o(0x41, 0x12);
    // RTC: reg A rate 0x26, reg B 24 h / BCD / no interrupts, clear C and D, shutdown byte 0
    o(0x70, 0x0A); o(0x71, 0x26);
    o(0x70, 0x0B); o(0x71, 0x02);
    o(0x70, 0x0C); (e.pio_in)(0x71, 1);
    o(0x70, 0x0D); (e.pio_in)(0x71, 1);
    o(0x70, 0x0F); o(0x71, 0);
    // keyboard controller as the reference BIOS leaves it: self-test done (status bit 2 set - HIMEM looks at it),
    // keyboard on with scancode translation, mouse port disabled (command byte 0x61)
    let drain = || { for _ in 0..16 { if (e.pio_in)(0x64, 1) & 1 == 0 { break; } (e.pio_in)(0x60, 1); } };
    o(0x64, 0xAD); o(0x64, 0xA7);
    o(0x64, 0xAA); drain();
    o(0x64, 0xAB); drain();
    o(0x64, 0x60); o(0x60, 0x30);
    o(0x60, 0xF4); drain();
    o(0x64, 0x60); o(0x60, 0x61);
    // BDA tick count = time of day from the RTC (a real BIOS does this at POST; Windows takes its start time from it)
    let bcd = |r: u32| { (e.pio_out)(0x70, 1, r); let v = (e.pio_in)(0x71, 1) as u32; (v >> 4) * 10 + (v & 15) };
    let secs = bcd(4) * 3600 + bcd(2) * 60 + bcd(0);
    cx.wr32(0x46C, ((secs as u64 * 1_193_182) >> 16) as u32);
    // BDA / EBDA
    cx.set_bda16(0x13, 639); // 1 KB EBDA at 9FC00 is reserved, as on the reference (DOS then shows 45K used)
    cx.set_bda16(0x0E, 0x9FC0);
    cx.set_bda16(0x10, 0x0024); // as the reference BIOS reports it (80x25 colour + pointing device)
    cx.set_bda16(0x1A, 0x1E);
    cx.set_bda16(0x1C, 0x1E);
    cx.set_bda16(0x80, 0x1E);
    cx.set_bda16(0x82, 0x3E);
    cx.set_bda8(0x96, 0x10);
    cx.set_bda8(0x76, 0xC0); // as the reference BIOS leaves it
    cx.set_bda8(0x41, 0x01);
    (e.pio_out)(0x70, 1, 0x12);
    let hd = ((e.pio_in)(0x71, 1) & 0xF0 != 0) as u8;
    cx.set_bda8(0x75, hd);
    cx.set_bda16(0x63, 0x3D4);
    // PCI as the reference BIOS leaves it: decode enabled on every function (command 0x0103), VGA BAR0 = FEA00000,
    // IDE primary+secondary decode enable (PIIX config 0x40 = 0x80008000; without it Win95 drops to MS-DOS compatibility mode)
    // any other device gets its 32-bit memory BARs sized and placed below the VGA BAR (top-down, size-aligned),
    // as a PC BIOS would; without this a paravirtual card's BAR stays 0 and its driver maps guest RAM (RED B29a find)
    let mut mem_top: u32 = 0xFEA0_0000;
    for dev in 0..32u32 {
        for func in 0..8u32 {
            let a = 0x8000_0000 | dev << 11 | func << 8;
            (e.pio_out)(0xCF8, 4, a);
            let id = (e.pio_in)(0xCFC, 4);
            if id == 0xFFFF_FFFF { continue; }
            (e.pio_out)(0xCF8, 4, a | 0x04);
            (e.pio_out)(0xCFC, 4, 0x0103);
            if id == 0x1111_1234 {
                (e.pio_out)(0xCF8, 4, a | 0x10);
                (e.pio_out)(0xCFC, 4, 0xFEA0_0000);
                cx.set_bda16(0xF0, (dev << 3 | func) as u16);
            } else if id == 0x7000_8086 {
                (e.pio_out)(0xCF8, 4, a | 0x60);
                (e.pio_out)(0xCFC, 4, 0x0B0B_0A0A); // PIRQ A..D routing
            } else if id == 0x7010_8086 {
                (e.pio_out)(0xCF8, 4, a | 0x40);
                (e.pio_out)(0xCFC, 4, 0x8000_8000);
            } else if id & 0xFFFF != 0x8086 {
                let mut bar = 0u32;
                while bar < 6 {
                    let r = a | (0x10 + bar * 4);
                    (e.pio_out)(0xCF8, 4, r);
                    let orig = (e.pio_in)(0xCFC, 4);
                    bar += 1;
                    if orig & 1 != 0 { continue; } // I/O BAR: leave as the device reset it
                    let is64 = (orig >> 1) & 3 == 2;
                    (e.pio_out)(0xCFC, 4, 0xFFFF_FFFF);
                    let mask = (e.pio_in)(0xCFC, 4) & 0xFFFF_FFF0;
                    if mask == 0 { (e.pio_out)(0xCFC, 4, 0); continue; }
                    let size = (!mask).wrapping_add(1);
                    mem_top = (mem_top - size) & mask;
                    (e.pio_out)(0xCFC, 4, mem_top);
                    if is64 { (e.pio_out)(0xCF8, 4, r + 4); (e.pio_out)(0xCFC, 4, 0); bar += 1; } // upper half: below 4 GB
                }
            }
        }
    }
    // INT 41h fixed-disk parameter table, filled from the drive (logical cylinders capped at 1024 as the reference does)
    let mut chs = [0u32; 3];
    let mut nb = 0u64;
    if (e.disk_info)(chs.as_mut_ptr(), &mut nb) == 0 {
        let t = 0x9FC00 + 0x3D; // table lives in the EBDA as on a real PC (INT 41h -> 9FC0:003D)
        cx.wr8(0x9FC00, 1); // EBDA size in KB
        cx.wr16(0x41 * 4, 0x3D);
        cx.wr16(0x41 * 4 + 2, 0x9FC0);
        let lc = chs[0].min(1024) as u16;
        let b = [lc as u8, (lc >> 8) as u8, chs[1] as u8, 0xA0, 0x3F, 0xFF, 0xFF, 0, 0xC8,
                 chs[0] as u8, (chs[0] >> 8) as u8, chs[1] as u8, chs[0] as u8, (chs[0] >> 8) as u8, chs[2] as u8];
        let mut sum = 0u8;
        for (i, v) in b.iter().enumerate() { cx.wr8(t + i as u32, *v); sum = sum.wrapping_add(*v); }
        cx.wr8(t + 15, 0u8.wrapping_sub(sum));
    }
    // boot sector -> 0000:7C00, DL = 80h
    cx.set32(EDX, 0x80);
    if hd != 0 {
        unsafe { (e.disk_read)(0, 1, e.mem.add(0x7C00)); }
    }
}

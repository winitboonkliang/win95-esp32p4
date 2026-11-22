//! INT 13h hard-disk service, drive 80h only.  One host call per request; sectors go straight from the block
//! layer into guest RAM (no PIO loop, no IDE register traffic).  Write-through: every write has reached the
//! block device when the call returns, so a power cut cannot lose what the guest saw as written.
use crate::*;

const BPS: usize = 512;
static mut BOUNCE: [u8; BPS] = [0; BPS];

struct Geo { pc: u32, ph: u32, ps: u32, nb: u64, lc: u32, lh: u32, ls: u32 }

fn geometry() -> Option<Geo> {
    let mut chs = [0u32; 3];
    let mut nb = 0u64;
    if (env().disk_info)(chs.as_mut_ptr(), &mut nb) != 0 { return None; }
    // CHS 1024/16/63 style logical geometry: cylinders capped at 1024, heads/sectors as the drive reports.
    let lc = chs[0].min(1023); // what the reference BIOS reports for a 1040-cylinder disk: max cylinder index 1022
    Some(Geo { pc: chs[0], ph: chs[1], ps: chs[2], nb, lc, lh: chs[1], ls: chs[2] })
}

fn status(cx: &mut Cx, ah: u8) {
    cx.set_bda8(0x74, ah);
    cx.set_ah(ah);
    if ah == 0 { cx.ok() } else { cx.ret_flags(CF, 0) }
}

/// contiguous physical span of [lin, lin+len) or None
fn span(cx: &Cx, lin: u32, len: u32, wr: bool) -> Option<usize> {
    let p0 = cx.phys(lin, wr)?;
    let mut off = 0x1000 - (lin & 0xFFF);
    while off < len {
        let p = cx.phys(lin.wrapping_add(off), wr)?;
        if p != p0 + off as usize { return None; }
        off += 0x1000;
    }
    if p0 + len as usize > env().mem_size as usize { return None; }
    Some(p0)
}

fn xfer(cx: &Cx, lba: u64, n: u32, lin: u32, write: bool) -> bool {
    let e = env();
    if let Some(p) = span(cx, lin, n * BPS as u32, !write) {
        let ptr = unsafe { e.mem.add(p) };
        return if write { (e.disk_write)(lba, n, ptr) == 0 } else { (e.disk_read)(lba, n, ptr) == 0 };
    }
    // buffer is not physically contiguous: sector by sector through a bounce buffer
    let b = unsafe { core::ptr::addr_of_mut!(BOUNCE) as *mut u8 };
    for i in 0..n {
        let base = lin.wrapping_add(i * BPS as u32);
        if write {
            for k in 0..BPS as u32 { unsafe { *b.add(k as usize) = cx.rd8(base.wrapping_add(k)); } }
            if (e.disk_write)(lba + i as u64, 1, b) != 0 { return false; }
        } else {
            if (e.disk_read)(lba + i as u64, 1, b) != 0 { return false; }
            for k in 0..BPS as u32 { cx.wr8(base.wrapping_add(k), unsafe { *b.add(k as usize) }); }
        }
    }
    true
}

/// V86 single-sector read: ask the guest stub for a shadow PIO read so ESDI_506 sees IDE port traffic (see bios.S)
fn shadow(cx: &mut Cx, lba: u64, n: u32, write: bool) {
    if !write && n == 1 && cx.st[8] & 0x2_0000 != 0 && lba < (1 << 28) {
        cx.wr32(0x4EC, lba as u32);
        cx.set_bda8(0xE8, 1);
    }
}

pub fn int13(cx: &mut Cx) {
    let ah = cx.ah();
    let dl = cx.dl();
    if dl != 0x80 {
        // no floppies, one hard disk
        match ah { 0x15 => { cx.set_ah(0); cx.ret_flags(CF, 0); } _ => status(cx, 0x01) }
        return;
    }
    let g = match geometry() { Some(g) => g, None => { status(cx, 0x01); return; } };
    match ah {
        0x00 | 0x09 | 0x0C | 0x0D | 0x10 | 0x11 | 0x14 | 0x05 | 0x06 | 0x07 => status(cx, 0),
        0x01 => { let s = cx.bda8(0x74); cx.set_ah(s); if s == 0 { cx.ok() } else { cx.ret_flags(CF, 0) } }
        0x02 | 0x03 | 0x04 => {
            let n = cx.al() as u32;
            let cl = cx.cx() as u32;
            let cyl = (cx.cx() >> 8) as u32 | ((cl & 0xC0) << 2);
            let sec = cl & 0x3F;
            let head = cx.dh() as u32;
            if n == 0 || n > 128 { status(cx, 0x01); return; }
            if cyl >= g.lc || head >= g.lh || sec == 0 || sec > g.ls { status(cx, 0x04); return; }
            let lba = ((cyl * g.lh + head) * g.ls + sec - 1) as u64;
            if lba + n as u64 > g.nb { status(cx, 0x04); return; }
            if ah != 0x04 {
                let lin = cx.seg(ES).wrapping_add(cx.bx() as u32);
                if !xfer(cx, lba, n, lin, ah == 0x03) { status(cx, 0x20); return; }
                shadow(cx, lba, n, ah == 0x03);
            }
            status(cx, 0);
            cx.set_al(n as u8);
        }
        0x08 => {
            let c = g.lc - 1;
            cx.set_ah(0);
            cx.set_al(0);
            cx.set_bl(0);
            cx.set_ch(c as u8);
            cx.set_cl((((c >> 2) & 0xC0) as u8) | g.ls as u8);
            cx.set_dh((g.lh - 1) as u8);
            cx.set_dl(cx.bda8(0x75));
            cx.set_bda8(0x74, 0);
            cx.ok();
        }
        0x15 => { // disk type: fixed disk, sector count in CX:DX
            let n = g.lc * g.lh * g.ls;
            cx.set_ah(3);
            cx.set16(ECX, (n >> 16) as u16);
            cx.set16(EDX, n as u16);
            cx.ok();
        }
        0x41 => { // EDD installation check
            if cx.bx() == 0x55AA { cx.set_ah(0x30); cx.set16(EBX, 0xAA55); cx.set16(ECX, 0x0007); cx.ok(); } else { status(cx, 0x01); }
        }
        0x42 | 0x43 | 0x44 | 0x47 => {
            let dap = cx.seg(DS).wrapping_add(cx.si() as u32);
            if cx.rd8(dap) < 0x10 { status(cx, 0x01); return; }
            let n = cx.rd16(dap.wrapping_add(2)) as u32;
            let off = cx.rd16(dap.wrapping_add(4)) as u32;
            let seg = cx.rd16(dap.wrapping_add(6)) as u32;
            let lba = cx.rd32(dap.wrapping_add(8)) as u64 | (cx.rd32(dap.wrapping_add(12)) as u64) << 32;
            if n > 127 || lba + n as u64 > g.nb { cx.wr16(dap.wrapping_add(2), 0); status(cx, if n > 127 { 0x01 } else { 0x04 }); return; }
            if (ah == 0x42 || ah == 0x43) && n > 0 {
                let lin = (seg << 4).wrapping_add(off);
                if !xfer(cx, lba, n, lin, ah == 0x43) { cx.wr16(dap.wrapping_add(2), 0); status(cx, 0x20); return; }
                shadow(cx, lba, n, ah == 0x43);
            }
            status(cx, 0);
        }
        0x48 => {
            let buf = cx.seg(DS).wrapping_add(cx.si() as u32);
            let insz = cx.rd16(buf);
            if insz < 0x1A { status(cx, 0x01); return; }
            let outsz: u16 = if insz >= 0x1E { 0x1E } else { 0x1A };
            cx.wr16(buf, outsz);
            cx.wr16(buf.wrapping_add(2), 2);
            cx.wr32(buf.wrapping_add(4), g.pc);
            cx.wr32(buf.wrapping_add(8), g.ph);
            cx.wr32(buf.wrapping_add(12), g.ps);
            cx.wr32(buf.wrapping_add(16), g.nb as u32);
            cx.wr32(buf.wrapping_add(20), (g.nb >> 32) as u32);
            cx.wr16(buf.wrapping_add(24), BPS as u16);
            if outsz == 0x1E { cx.wr32(buf.wrapping_add(26), 0xFFFF_FFFF); }
            status(cx, 0);
        }
        0x45 | 0x46 | 0x49 => status(cx, 0),
        _ => status(cx, 0x01),
    }
}

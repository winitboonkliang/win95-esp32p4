//! B60 BLUE host firmware services.  no_std, no allocator.  The C glue (pc.c) hands us an `Env` of callbacks;
//! `fw_call(port,val)` is invoked when the guest executes `out imm8,al` on 0xE0-0xEF.
#![no_std]
use core::ptr::null_mut;
use core::sync::atomic::{AtomicPtr, Ordering};

mod int13;
mod kbd;
mod misc;
mod post;
mod vga;
mod syms;

#[repr(C)]
pub struct Env {
    pub mem: *mut u8,
    pub mem_size: u32,
    pub get_state: extern "C" fn(*mut u32),
    pub set_gpr: extern "C" fn(i32, u32),
    pub set_flags: extern "C" fn(u32, u32),
    pub lin2phys: extern "C" fn(u32, i32) -> i64,
    pub disk_info: extern "C" fn(*mut u32, *mut u64) -> i32,
    pub disk_read: extern "C" fn(u64, u32, *mut u8) -> i32,
    pub disk_write: extern "C" fn(u64, u32, *const u8) -> i32,
    pub pio_in: extern "C" fn(u32, u32) -> u32,
    pub pio_out: extern "C" fn(u32, u32, u32),
    pub reset: extern "C" fn(),
    pub vga_rd: extern "C" fn(u32) -> u8,
    pub vga_wr: extern "C" fn(u32, u8),
    pub event: extern "C" fn(u32),
    pub vbe_w: extern "C" fn(i32, u16),
    pub vbe_r: extern "C" fn(i32) -> u16,
    pub vga_lfb: u32,
    pub vga_ram: u32,
}

static ENV: AtomicPtr<Env> = AtomicPtr::new(null_mut());

#[inline(always)]
pub(crate) fn env() -> &'static Env {
    unsafe { &*ENV.load(Ordering::Relaxed) }
}

#[no_mangle]
pub extern "C" fn fw_init(e: *const Env) {
    ENV.store(e as *mut Env, Ordering::Relaxed);
}

pub const EAX: usize = 0;
pub const ECX: usize = 1;
pub const EDX: usize = 2;
pub const EBX: usize = 3;
pub const ESP: usize = 4;
pub const EBP: usize = 5;
pub const ESI: usize = 6;
pub const EDI: usize = 7;
pub const ES: usize = 0;
pub const SS: usize = 2;
pub const DS: usize = 3;

pub const CF: u32 = 1;
pub const ZF: u32 = 0x40;

/// One service call: snapshot of the guest CPU, register edits are committed by `done()`.
pub struct Cx {
    pub st: [u32; 32],
    dirty: u8,
    flag_set: u32,
    flag_clr: u32,
    /// results go to the live flags (far-call entry) instead of the INT frame
    pub live: bool,
}

impl Cx {
    fn new() -> Cx {
        let mut st = [0u32; 32];
        (env().get_state)(st.as_mut_ptr());
        Cx { st, dirty: 0, flag_set: 0, flag_clr: 0, live: false }
    }
    #[inline] pub fn r(&self, i: usize) -> u32 { self.st[i] }
    #[inline] pub fn ax(&self) -> u16 { self.st[EAX] as u16 }
    #[inline] pub fn ah(&self) -> u8 { (self.st[EAX] >> 8) as u8 }
    #[inline] pub fn al(&self) -> u8 { self.st[EAX] as u8 }
    #[inline] pub fn bx(&self) -> u16 { self.st[EBX] as u16 }
    #[inline] pub fn cx(&self) -> u16 { self.st[ECX] as u16 }
    #[inline] pub fn dx(&self) -> u16 { self.st[EDX] as u16 }
    #[inline] pub fn dl(&self) -> u8 { self.st[EDX] as u8 }
    #[inline] pub fn dh(&self) -> u8 { (self.st[EDX] >> 8) as u8 }
    #[inline] pub fn si(&self) -> u16 { self.st[ESI] as u16 }
    #[inline] pub fn di(&self) -> u16 { self.st[EDI] as u16 }
    pub fn set32(&mut self, i: usize, v: u32) { self.st[i] = v; self.dirty |= 1 << i; }
    pub fn set16(&mut self, i: usize, v: u16) { self.set32(i, (self.st[i] & 0xFFFF_0000) | v as u32); }
    pub fn set_ah(&mut self, v: u8) { self.set32(EAX, (self.st[EAX] & 0xFFFF_00FF) | (v as u32) << 8); }
    pub fn set_al(&mut self, v: u8) { self.set32(EAX, (self.st[EAX] & 0xFFFF_FF00) | v as u32); }
    pub fn set_bl(&mut self, v: u8) { self.set32(EBX, (self.st[EBX] & 0xFFFF_FF00) | v as u32); }
    pub fn set_dl(&mut self, v: u8) { self.set32(EDX, (self.st[EDX] & 0xFFFF_FF00) | v as u32); }
    pub fn set_dh(&mut self, v: u8) { self.set32(EDX, (self.st[EDX] & 0xFFFF_00FF) | (v as u32) << 8); }
    pub fn set_ch(&mut self, v: u8) { self.set32(ECX, (self.st[ECX] & 0xFFFF_00FF) | (v as u32) << 8); }
    pub fn set_cl(&mut self, v: u8) { self.set32(ECX, (self.st[ECX] & 0xFFFF_FF00) | v as u32); }
    /// segment base of ES/CS/SS/DS/FS/GS
    #[inline] pub fn seg(&self, s: usize) -> u32 { self.st[9 + s] }

    // ---- guest memory by linear address (paging aware)
    #[inline]
    fn paged(&self) -> bool { self.st[24] & 0x8000_0000 != 0 }
    pub fn phys(&self, lin: u32, wr: bool) -> Option<usize> {
        let p = if self.paged() { let v = (env().lin2phys)(lin, wr as i32); if v < 0 { return None } v as usize } else { lin as usize };
        if p < env().mem_size as usize { Some(p) } else { None }
    }
    pub fn rd8(&self, lin: u32) -> u8 { match self.phys(lin, false) { Some(p) => unsafe { *env().mem.add(p) }, None => 0xFF } }
    pub fn wr8(&self, lin: u32, v: u8) { if let Some(p) = self.phys(lin, true) { unsafe { *env().mem.add(p) = v } } }
    pub fn rd16(&self, lin: u32) -> u16 { self.rd8(lin) as u16 | (self.rd8(lin.wrapping_add(1)) as u16) << 8 }
    pub fn wr16(&self, lin: u32, v: u16) { self.wr8(lin, v as u8); self.wr8(lin.wrapping_add(1), (v >> 8) as u8) }
    pub fn rd32(&self, lin: u32) -> u32 { self.rd16(lin) as u32 | (self.rd16(lin.wrapping_add(2)) as u32) << 16 }
    pub fn wr32(&self, lin: u32, v: u32) { self.wr16(lin, v as u16); self.wr16(lin.wrapping_add(2), (v >> 16) as u16) }
    /// BDA byte / word at 0040:off
    pub fn bda8(&self, off: u32) -> u8 { self.rd8(0x400 + off) }
    pub fn bda16(&self, off: u32) -> u16 { self.rd16(0x400 + off) }
    pub fn set_bda8(&self, off: u32, v: u8) { self.wr8(0x400 + off, v) }
    pub fn set_bda16(&self, off: u32, v: u16) { self.wr16(0x400 + off, v) }

    // ---- results
    /// Edit the FLAGS image the INT pushed (SS:SP+4): `set`/`clr` are masks (CF, ZF ...).
    pub fn ret_flags(&mut self, set: u32, clr: u32) {
        if self.live { self.flag_set |= set; self.flag_clr |= clr; self.flag_set &= !clr; self.flag_clr &= !set; return; }
        let sp = self.st[ESP] & self.st[15];
        let a = self.seg(SS).wrapping_add(sp).wrapping_add(4);
        let f = self.rd16(a) as u32;
        self.wr16(a, ((f & !clr) | set) as u16);
    }
    /// CF only (stacked image)
    pub fn ok(&mut self) { self.ret_flags(0, CF) }
    pub fn fail(&mut self, ah: u8) { self.set_ah(ah); self.ret_flags(CF, 0) }
    /// live flags (used by stubs that test them right after the `out`)
    pub fn live_flags(&mut self, set: u32, clr: u32) { self.flag_set = set; self.flag_clr = clr; }
    fn done(self) {
        let e = env();
        for i in 0..8 { if self.dirty & (1 << i) != 0 { (e.set_gpr)(i as i32, self.st[i]); } }
        if self.flag_set | self.flag_clr != 0 { (e.set_flags)(self.flag_set, self.flag_clr); }
    }
}

/// Guest executed `out imm8,al` on 0xE0+port.
#[no_mangle]
pub extern "C" fn fw_call(port: u32, _val: u32) {
    let mut cx = Cx::new();
    match port & 0xF {
        0x0 => post::post(&mut cx),
        0x1 => int13::int13(&mut cx),
        0x2 => misc::int15(&mut cx),
        0x3 => kbd::int16(&mut cx),
        0x4 => misc::int1a(&mut cx),
        0x5 => kbd::int09(&mut cx),
        0x7 => vga::int10(&mut cx),
        0x8 => { cx.live = true; misc::apm_power(&mut cx) }
        0x6 => { cx.live = true; misc::pcibios(&mut cx) }
        _ => {}
    }
    cx.done();
}

#[cfg(feature = "standalone")]
#[panic_handler]
fn panic(_: &core::panic::PanicInfo) -> ! { loop {} }

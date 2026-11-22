//! B76 native Notepad - host side of the B76APP.EXE contract (CONTRACT.md).
//! The guest stub traps once per window message (OUT 0xF1E0); we answer with a call script written into the guest's shared
//! area. Text editing, layout and drawing run here; keys can arrive from the host directly (0 guest instructions).
#![cfg_attr(not(test), no_std)]
#![allow(static_mut_refs, clippy::missing_safety_doc)]

pub mod ed;
mod imports;
use ed::Ed;
use imports::*;

extern "C" {
    fn b76g_read(lin: u32, dst: *mut u8, n: u32) -> i32; // 0 = ok, -1 = a page is not present
    fn b76g_write(lin: u32, src: *const u8, n: u32) -> i32;
    fn b76g_fb(w: *mut i32, h: *mut i32) -> *mut u16; // the screen surface, RGB565, stride = w
    fn b76g_dirty(y: i32, x0: i32, x1: i32);
    fn b76g_time() -> u32; // local time, unix seconds
    fn b76g_doorbell(); // ask the guest to run our stub once (an F12 key event)
    fn b76g_log(s: *const u8, n: u32);
    // B82 Doom (esp/main/b82doom.c): the engine runs on the host; this crate only owns its Win95 window
    fn b82_open(wad: i32) -> i32; // restarts into the Doom boot (never returns); <0 = WAD missing / no Doom app. bits 0-7 wad 0 shareware, 1 Ultimate, 2 Doom II; bit 8 engine (B83: 0 C, 1 Rust)
    fn b82_focus(on: i32);
    fn b82_close();
    fn b82_take_quit() -> i32; // the game asked to leave (Quit, engine error, end of a timedemo)
    fn b82_dib(bmi: u32, dib: u32); // last frame as a 1280x800 565 bottom-up DIB + its BITMAPINFO (covered/inactive window)
}

#[cfg(not(test))]
#[panic_handler]
fn panic(_: &core::panic::PanicInfo) -> ! { loop {} }

// ---- shared area layout (offsets from SHARED) ----
const AREA_SCRIPT: u32 = 16;
const AREA_SLOTS: u32 = 0x7c0; // 16 dword result slots
const AREA_SCRATCH: u32 = 0x800; // 2 KB strings / structs
const FILEBUF: u32 = 0x20000; // 256 KB, touched by the stub at start
const BMI: u32 = 0x5f000;
const DIB: u32 = 0x60000;
const SH: u32 = SHARED;

// window messages
const WM_CREATE: u32 = 1; const WM_DESTROY: u32 = 2; const WM_MOVE: u32 = 3; const WM_SIZE: u32 = 5;
const WM_SETFOCUS: u32 = 7; const WM_KILLFOCUS: u32 = 8; const WM_PAINT: u32 = 0xf; const WM_CLOSE: u32 = 0x10;
const WM_ERASEBKGND: u32 = 0x14; const WM_KEYDOWN: u32 = 0x100; const WM_KEYUP: u32 = 0x101; const WM_CHAR: u32 = 0x102;
const WM_COMMAND: u32 = 0x111; const WM_INITMENUPOPUP: u32 = 0x117; const WM_LBUTTONDOWN: u32 = 0x201;
const WM_ENTERMENULOOP: u32 = 0x211; const WM_EXITMENULOOP: u32 = 0x212; const WM_ENTERSIZEMOVE: u32 = 0x231;
const WM_EXITSIZEMOVE: u32 = 0x232;
pub const VK_SHIFT: u32 = 0x10; pub const VK_CONTROL: u32 = 0x11; const VK_F12: u32 = 0x7b;

// menu ids
const ID_NEW: u32 = 100; const ID_OPEN: u32 = 101; const ID_SAVE: u32 = 102; const ID_SAVEAS: u32 = 103; const ID_EXIT: u32 = 109;
const ID_UNDO: u32 = 200; const ID_CUT: u32 = 201; const ID_COPY: u32 = 202; const ID_PASTE: u32 = 203; const ID_DEL: u32 = 204;
const ID_SELALL: u32 = 205; const ID_TIME: u32 = 206; const ID_WRAP: u32 = 207; const ID_FIND: u32 = 300; const ID_FINDNEXT: u32 = 301;
const ID_ABOUT: u32 = 400;

#[derive(Clone, Copy, PartialEq)]
enum Then { Nothing, New, Open, Exit }
#[derive(Clone, Copy, PartialEq)]
enum Pend { None, Atoms, DdeExec(u32, u32), StartRead, OpenDlg, OpenRead, SaveDlg(Then), SaveDone(Then), Confirm(Then), ClipCopy(u32), ClipPaste, DibTouch }
#[derive(Clone, Copy, PartialEq)]
enum Req { None, Copy, Paste, Cut }

#[derive(Clone, Copy)]
enum A { V(u32), I(u32) }

/// One reply: script words + scratch bytes for one area.
struct Sc { area: u32, w: [u32; 492], n: usize, s: [u8; 2048], sn: usize }
impl Sc {
    fn new(area: u32) -> Sc { Sc { area, w: [0; 492], n: 0, s: [0; 2048], sn: 0 } }
    fn slot(&self, k: u32) -> u32 { self.area + AREA_SLOTS + 4 * k }
    fn put(&mut self, v: u32) { if self.n < self.w.len() { self.w[self.n] = v; self.n += 1; } }
    fn call(&mut self, f: u32, args: &[A], store: u32) {
        let mut ind = 0u32; self.put(f); self.put(args.len() as u32); self.put(store);
        let at = self.n; self.put(0);
        for (k, a) in args.iter().enumerate() { match *a { A::V(v) => self.put(v), A::I(o) => { ind |= 1 << k; self.put(o) } } }
        self.w[at] = ind;
    }
    /// bytes into scratch, returns their guest linear address
    fn bytes(&mut self, b: &[u8]) -> u32 {
        let a = self.sn; let n = b.len().min(self.s.len() - a); self.s[a..a + n].copy_from_slice(&b[..n]);
        self.sn = (a + n + 3) & !3; SH + self.area + AREA_SCRATCH + a as u32
    }
    fn zstr(&mut self, b: &[u8]) -> u32 {
        let a = self.sn; let n = b.len().min(self.s.len() - a - 1); self.s[a..a + n].copy_from_slice(&b[..n]); self.s[a + n] = 0;
        self.sn = (a + n + 4) & !3; SH + self.area + AREA_SCRATCH + a as u32
    }
    fn zeros(&mut self, n: usize) -> u32 { let a = self.sn; self.sn = (a + n + 3) & !3; SH + self.area + AREA_SCRATCH + a as u32 }
    fn end(&mut self, v: u32) { self.put(0xffff_ffff); self.put(1); self.put(0); self.put(0); self.put(v); }
    fn end_last(&mut self) { self.put(0xffff_ffff); self.put(0); self.put(0); self.put(0); }
    fn ask(&mut self) { self.put(0xffff_fffd); self.put(0); self.put(0); self.put(0); }
    fn touch(&mut self, p: u32, len: u32) { self.put(0xffff_fffe); self.put(2); self.put(0); self.put(0); self.put(p); self.put(len); }
    fn flush(&self) {
        unsafe {
            b76g_write(SH + self.area + AREA_SCRATCH, self.s.as_ptr(), self.sn as u32);
            b76g_write(SH + self.area + AREA_SCRIPT, self.w.as_ptr() as *const u8, 4 * self.n as u32);
        }
    }
}

fn rd32(lin: u32) -> u32 { let mut b = [0u8; 4]; unsafe { b76g_read(lin, b.as_mut_ptr(), 4); } u32::from_le_bytes(b) }
fn log(s: &[u8]) { unsafe { b76g_log(s.as_ptr(), s.len() as u32) } }

const CW: i32 = 8; const CH: i32 = 16; const SBW: i32 = 16; const FINDH: i32 = 20;
const MAXW: usize = 1280; const MAXH: usize = 800;

pub const NW: usize = 3; // Notepad windows open at once (one resident stub process owns them all)
/// one Notepad window
pub struct App {
    pub ed: Ed,
    idx: usize, used: bool,
    hwnd: u32,
    cx: i32, cy: i32, cw: i32, chh: i32,
    focused: bool, inmenu: bool, sizing: bool, minimized: bool,
    path: [u8; 260], plen: usize,
    req: Req,
    shift: bool, ctrl: bool,
    findmode: bool, find: [u8; 64], flen: usize,
    painted: bool,
    hs: i32,
    bh: [u64; 64],
}
/// state shared by all windows (and the DDE server window of the resident stub)
pub struct Glob {
    wins: [App; NW],
    band: [u16; MAXW * 16],
    tmp: [u8; ed::CAP],
    font: [u8; 4096],
    wndproc: u32, hinst: u32,
    pend: [Pend; 16], pend_win: [usize; 16],
    dib_ok: u32,
    dde_hwnd: u32, atom_app: u32, atom_topic: u32, creating: usize, class_ok: bool,
    doom_hwnd: u32, doom_focus: bool,
    open_q: [u8; 260], open_qn: usize, open_qv: bool,
    pub traps: u32, pub hostkeys: u32, pub direct_draws: u32, pub dib_paints: u32, pub doorbells: u32, pub opens: u32,
}
static mut G: *mut Glob = core::ptr::null_mut();
#[inline(always)] fn g() -> &'static mut Glob { unsafe { &mut *G } }
fn set_pend(s: &Sc, p: Pend, w: usize) { let l = (s.area >> 12) as usize & 15; g().pend[l] = p; g().pend_win[l] = w; }

/// bytes the host must give us (PSRAM on the board): one zeroed block, see b76_init
#[no_mangle] pub extern "C" fn b76_mem_size() -> u32 { core::mem::size_of::<Glob>() as u32 }
/// mem = b76_mem_size() zeroed bytes, 8-aligned
#[no_mangle] pub unsafe extern "C" fn b76_init(mem: *mut u8) {
    G = mem as *mut Glob; let gl = &mut *G;
    for i in 0..NW { let w = core::ptr::addr_of_mut!(gl.wins[i]); core::ptr::addr_of_mut!((*w).req).write(Req::None);
        (*w).idx = i; (*w).ed.cols = 80; (*w).ed.nl = 1; }
    for i in 0..16 { core::ptr::addr_of_mut!(gl.pend[i]).write(Pend::None); }
    gl.creating = usize::MAX;
}

#[no_mangle]
pub unsafe extern "C" fn b76_set_font(f: *const u8) { g().font.copy_from_slice(core::slice::from_raw_parts(f, 4096)); }
#[no_mangle]
pub unsafe extern "C" fn b76_counters(o: *mut u32) {
    let a = g(); let open = a.wins.iter().filter(|w| w.used).count() as u32;
    let v = [a.traps, a.hostkeys, a.direct_draws, a.dib_paints, a.doorbells, a.wins.iter().find(|w| w.used).map_or(0, |w| w.ed.n as u32), open, a.opens];
    core::ptr::copy_nonoverlapping(v.as_ptr(), o, 8);
}
/// B61 golden snapshot: the resident stub's registration lives here (host PSRAM), not in guest RAM. wr=0 reads it, wr=1 restores it
/// after a resume (windows are not kept: the golden is taken with none open).
#[no_mangle]
pub unsafe extern "C" fn b76_state_io(v: *mut u32, wr: i32) {
    let a = g();
    if wr == 0 { let s = [a.wndproc, a.hinst, a.dde_hwnd, a.atom_app, a.atom_topic, a.class_ok as u32]; core::ptr::copy_nonoverlapping(s.as_ptr(), v, 6); }
    else { let s = core::slice::from_raw_parts(v, 6); a.wndproc = s[0]; a.hinst = s[1]; a.dde_hwnd = s[2]; a.atom_app = s[3]; a.atom_topic = s[4]; a.class_ok = s[5] != 0; }
}
/// the host input layer asks this before it routes a key to us instead of the guest
#[no_mangle]
pub unsafe extern "C" fn b76_wants_keys() -> i32 { focused_win().is_some() as i32 }
fn focused_win() -> Option<usize> { g().wins.iter().position(|a| a.used && a.hwnd != 0 && a.focused && !a.inmenu && !a.sizing && !a.minimized && a.req == Req::None) }
/// direct host key: ch = character (0 if none), vk = Windows virtual key, mods bit0 shift bit1 ctrl. 0 guest instructions.
#[no_mangle]
pub unsafe extern "C" fn b76_hostkey(ch: u32, vk: u32, mods: u32) {
    let Some(i) = focused_win() else { return }; let a = &mut g().wins[i]; g().hostkeys += 1;
    a.shift = mods & 1 != 0; a.ctrl = mods & 2 != 0;
    if ch != 0 { a.on_char(ch); } else { a.on_key(vk); }
    if a.req != Req::None { g().doorbells += 1; b76g_doorbell(); }
    a.redraw();
}

#[no_mangle]
pub unsafe extern "C" fn b76_trap(op_area: u32) {
    g().traps += 1;
    let op = op_area & 0xfff; let area = op_area & !0xfff;
    let mut h = [0u32; 4]; b76g_read(SH + area, h.as_mut_ptr() as *mut u8, 16);
    let mut s = Sc::new(area);
    match op { 1 => start(&mut s, &h), 2 => msg(&mut s, &h), 3 => ask(&mut s, h[0]), _ => s.end(0) }
    s.flush();
}

impl App {
    fn lvl(s: &Sc) -> usize { (s.area >> 12) as usize & 15 }
    fn rows(&self) -> i32 { ((self.chh - if self.findmode { FINDH } else { 0 }) / CH).max(1) }
    fn tcols(&self) -> i32 { ((self.cw - SBW) / CW).max(1) }
    fn can_direct(&self) -> bool { self.hwnd != 0 && self.focused && !self.inmenu && !self.minimized && self.cw > 0 }

    fn name(&self) -> &[u8] {
        if self.plen == 0 { return b"Untitled"; }
        let p = &self.path[..self.plen]; let i = p.iter().rposition(|&c| c == b'\\' || c == b'/' || c == b':').map_or(0, |i| i + 1); &p[i..]
    }
    fn title(&self, s: &mut Sc) {
        let mut t = [0u8; 300]; let nm = self.name(); let n = nm.len().min(250); t[..n].copy_from_slice(&nm[..n]);
        t[n..n + 10].copy_from_slice(b" - Notepad"); let p = s.zstr(&t[..n + 10]);
        s.call(SETWINDOWTEXTA, &[A::V(self.hwnd), A::V(p)], 0);
    }
    fn read_file_script(&mut self, s: &mut Sc) {
        let p = s.zstr(&self.path[..self.plen]);
        let (hs, gs) = (s.slot(0), s.slot(1));
        s.call(CREATEFILEA, &[A::V(p), A::V(0x8000_0000), A::V(1), A::V(0), A::V(3), A::V(0x80), A::V(0)], hs);
        s.call(READFILE, &[A::I(hs), A::V(SH + FILEBUF), A::V(ed::CAP as u32), A::V(SH + gs), A::V(0)], 0);
        s.call(CLOSEHANDLE, &[A::I(hs)], 0);
        s.ask();
    }
    fn load_after_read(&mut self, s: &Sc) -> bool {
        let h = rd32(SH + s.slot(0)); let got = rd32(SH + s.slot(1)).min(ed::CAP as u32);
        if h == 0xffff_ffff { self.plen = 0; return false; }
        let t = &mut g().tmp; unsafe { b76g_read(SH + FILEBUF, t.as_mut_ptr(), got); } self.ed.set(&t[..got as usize]);
        true
    }

    fn create_window(&mut self, s: &mut Sc) {
        let cls = s.zstr(b"B76Pad");
        g().creating = self.idx;
        let bar = s.slot(2); let pop = s.slot(3);
        s.call(CREATEMENU, &[], bar);
        let menus: [(&[u8], &[(u32, &[u8])]); 4] = [
            (b"&File", &[(ID_NEW, b"&New"), (ID_OPEN, b"&Open..."), (ID_SAVE, b"&Save"), (ID_SAVEAS, b"Save &As..."), (0, b""), (ID_EXIT, b"E&xit")]),
            (b"&Edit", &[(ID_UNDO, b"&Undo\tCtrl+Z"), (0, b""), (ID_CUT, b"Cu&t\tCtrl+X"), (ID_COPY, b"&Copy\tCtrl+C"), (ID_PASTE, b"&Paste\tCtrl+V"),
                         (ID_DEL, b"De&lete\tDel"), (0, b""), (ID_SELALL, b"Select &All"), (ID_TIME, b"Time/&Date\tF5"), (0, b""), (ID_WRAP, b"&Word Wrap")]),
            (b"&Search", &[(ID_FIND, b"&Find..."), (ID_FINDNEXT, b"Find &Next\tF3")]),
            (b"&Help", &[(ID_ABOUT, b"&About Notepad")]),
        ];
        for (title, items) in menus.iter() {
            s.call(CREATEPOPUPMENU, &[], pop);
            for (id, txt) in items.iter() {
                if *id == 0 { s.call(APPENDMENUA, &[A::I(pop), A::V(0x800), A::V(0), A::V(0)], 0); }
                else { let p = s.zstr(txt); s.call(APPENDMENUA, &[A::I(pop), A::V(0), A::V(*id), A::V(p)], 0); }
            }
            let p = s.zstr(title); s.call(APPENDMENUA, &[A::I(bar), A::V(0x10), A::I(pop), A::V(p)], 0);
        }
        let mut t = [0u8; 300]; let nm = self.name(); let n = nm.len().min(250); t[..n].copy_from_slice(&nm[..n]);
        t[n..n + 10].copy_from_slice(b" - Notepad"); let tp = s.zstr(&t[..n + 10]);
        let hw = s.slot(4); let cu = 0x8000_0000;
        s.call(CREATEWINDOWEXA, &[A::V(0), A::V(cls), A::V(tp), A::V(0x00cf_0000), A::V(cu), A::V(cu), A::V(cu), A::V(cu), A::V(0), A::I(bar), A::V(g().hinst), A::V(0)], hw);
        s.call(SHOWWINDOW, &[A::I(hw), A::V(1)], 0);
        s.call(UPDATEWINDOW, &[A::I(hw)], 0);
        s.end(0);
        log(b"B76 create_window script");
    }

    fn wmsg(&mut self, s: &mut Sc, h: &[u32; 4]) {
        let (hw, m, wp, lp) = (h[0], h[1], h[2], h[3]);
        let def = |s: &mut Sc| { s.call(DEFWINDOWPROCA, &[A::V(hw), A::V(m), A::V(wp), A::V(lp)], 0); s.end_last(); };
        match m {
            WM_CREATE => s.end(0),
            WM_DESTROY => { self.used = false; self.hwnd = 0; s.end(0); }
            WM_MOVE => { self.cx = lp as i16 as i32; self.cy = (lp >> 16) as i16 as i32; s.end(0); }
            WM_SIZE => {
                self.minimized = wp == 1;
                if wp != 1 { self.cw = (lp & 0xffff) as i32; self.chh = (lp >> 16) as i32; self.relayout(); }
                s.end(0);
            }
            WM_SETFOCUS => { self.focused = true; s.end(0); }
            WM_KILLFOCUS => { self.focused = false; s.end(0); }
            WM_ENTERMENULOOP => { self.inmenu = true; s.end(0); }
            WM_EXITMENULOOP => { self.inmenu = false; s.end(0); }
            WM_ENTERSIZEMOVE => { self.sizing = true; def(s); }
            WM_EXITSIZEMOVE => { self.sizing = false; def(s); }
            WM_ERASEBKGND => s.end(1),
            WM_PAINT => self.paint(s),
            WM_CLOSE => self.guard(s, Then::Exit),
            WM_INITMENUPOPUP => {
                let (a, b) = self.ed.sel(); let hm = wp; let g = |c: bool| if c { 0 } else { 1 };
                for (id, on) in [(ID_UNDO, self.ed.has_undo), (ID_CUT, a != b), (ID_COPY, a != b), (ID_DEL, a != b)] {
                    s.call(ENABLEMENUITEM, &[A::V(hm), A::V(id), A::V(g(on))], 0);
                }
                s.call(CHECKMENUITEM, &[A::V(hm), A::V(ID_WRAP), A::V(if self.ed.wrap { 8 } else { 0 })], 0);
                s.end(0);
            }
            WM_KEYDOWN => {
                if wp == VK_F12 { self.serve_req(s); return; }
                if wp == VK_SHIFT { self.shift = true; } else if wp == VK_CONTROL { self.ctrl = true; } else { self.on_key(wp); }
                self.after_input(s);
            }
            WM_KEYUP => {
                if wp == VK_SHIFT { self.shift = false; } else if wp == VK_CONTROL { self.ctrl = false; }
                s.end(0);
            }
            WM_CHAR => { self.on_char(wp); self.after_input(s); }
            WM_LBUTTONDOWN => {
                let x = (lp & 0xffff) as i32; let y = (lp >> 16) as i32;
                if x >= self.cw - SBW { self.scrollbar_click(y); } else { self.ed.click((y / CH) as usize, ((x + CW / 2) / CW + self.hs) as usize, wp & 4 != 0); }
                self.after_input(s);
            }
            WM_COMMAND => self.command(s, wp & 0xffff),
            _ => def(s),
        }
    }
    /// after guest-delivered input: draw (direct when visible) and serve a clipboard request inside this same trap
    fn after_input(&mut self, s: &mut Sc) {
        self.redraw();
        if self.req != Req::None { self.serve_req(s); } else { s.end(0); }
    }
    fn serve_req(&mut self, s: &mut Sc) {
        let r = self.req; self.req = Req::None;
        match r {
            Req::Copy | Req::Cut => {
                let (a, b) = self.ed.sel(); if a == b { s.end(0); return; }
                let n = (b - a) as u32;
                s.call(GLOBALALLOC, &[A::V(0x2002), A::V(n + 1)], s.slot(5));
                s.call(GLOBALLOCK, &[A::I(s.slot(5))], 0);
                s.ask(); set_pend(s, Pend::ClipCopy(if r == Req::Cut { 1 } else { 0 }), self.idx);
            }
            Req::Paste => {
                s.call(OPENCLIPBOARD, &[A::V(self.hwnd)], 0);
                s.call(GETCLIPBOARDDATA, &[A::V(1)], s.slot(5));
                s.call(GLOBALLOCK, &[A::I(s.slot(5))], 0);
                s.ask(); set_pend(s, Pend::ClipPaste, self.idx);
            }
            Req::None => s.end(0),
        }
    }
    fn wask(&mut self, s: &mut Sc, last: u32, p: Pend) {
        let l = Self::lvl(s);
        match p {
            Pend::StartRead => { if !self.load_after_read(s) { log(b"B76 start: open failed"); } self.create_window(s); }
            Pend::DdeExec(..) | Pend::Atoms => s.end(0),
            Pend::OpenDlg => {
                if last == 0 { s.end(0); return; }
                let ofn = SH + s.area + AREA_SCRATCH; // the dialog wrote the path into the scratch buffer at +80
                let mut b = [0u8; 260]; unsafe { b76g_read(ofn + 80, b.as_mut_ptr(), 260); }
                self.plen = b.iter().position(|&c| c == 0).unwrap_or(259); self.path[..self.plen].copy_from_slice(&b[..self.plen]);
                set_pend(s, Pend::OpenRead, self.idx); self.read_file_script(s);
            }
            Pend::OpenRead => {
                self.load_after_read(s); self.relayout(); self.title(s);
                s.call(INVALIDATERECT, &[A::V(self.hwnd), A::V(0), A::V(0)], 0); s.end(0);
            }
            Pend::SaveDlg(then) => {
                if last == 0 { s.end(0); return; }
                let mut b = [0u8; 260]; unsafe { b76g_read(SH + s.area + AREA_SCRATCH + 80, b.as_mut_ptr(), 260); }
                self.plen = b.iter().position(|&c| c == 0).unwrap_or(259); self.path[..self.plen].copy_from_slice(&b[..self.plen]);
                self.write_file_script(s, then);
            }
            Pend::SaveDone(then) => {
                if rd32(SH + s.slot(0)) == 0xffff_ffff { let t = s.zstr(b"Cannot write the file."); let c = s.zstr(b"Notepad");
                    s.call(MESSAGEBOXA, &[A::V(self.hwnd), A::V(t), A::V(c), A::V(0x30)], 0); s.end(0); return; }
                self.ed.modified = false; self.title(s); self.then(s, then);
            }
            Pend::Confirm(then) => match last {
                6 => self.save(s, then, false),
                7 => self.then(s, then),
                _ => s.end(0),
            },
            Pend::ClipCopy(cut) => {
                let hg = rd32(SH + s.slot(5)); let ptr = last;
                if hg == 0 || ptr == 0 { s.end(0); return; }
                let (a, b) = self.ed.sel(); let mut ok = unsafe { b76g_write(ptr, self.ed.t[a..].as_ptr(), (b - a) as u32) } == 0;
                ok &= unsafe { b76g_write(ptr + (b - a) as u32, [0u8].as_ptr(), 1) } == 0;
                if !ok { s.touch(ptr, (b - a) as u32 + 1); s.call(GLOBALLOCK, &[A::V(hg)], 0); s.ask(); set_pend(s, Pend::ClipCopy(cut), self.idx); return; }
                s.call(GLOBALUNLOCK, &[A::V(hg)], 0);
                s.call(OPENCLIPBOARD, &[A::V(self.hwnd)], 0);
                s.call(EMPTYCLIPBOARD, &[], 0);
                s.call(SETCLIPBOARDDATA, &[A::V(1), A::V(hg)], 0);
                s.call(CLOSECLIPBOARD, &[], 0);
                s.end(0);
                if cut == 1 { self.ed.del_sel(); self.redraw(); }
            }
            Pend::ClipPaste => {
                let hg = rd32(SH + s.slot(5));
                if hg != 0 && last != 0 {
                    let n = read_cstr(last, &mut g().tmp);
                    self.ed.insert(&g().tmp[..n], false); self.redraw();
                    s.call(GLOBALUNLOCK, &[A::V(hg)], 0);
                }
                s.call(CLOSECLIPBOARD, &[], 0); s.end(0);
            }
            Pend::DibTouch => { g().dib_ok = self.dib_need(); self.paint(s); }
            Pend::None => s.end(0),
        }
    }
    fn then(&mut self, s: &mut Sc, t: Then) {
        match t {
            Then::Nothing => s.end(0),
            Then::New => { self.ed.set(&[]); self.plen = 0; self.relayout(); self.title(s);
                s.call(INVALIDATERECT, &[A::V(self.hwnd), A::V(0), A::V(0)], 0); s.end(0); }
            Then::Open => self.open_dlg(s, false),
            Then::Exit => { s.call(DESTROYWINDOW, &[A::V(self.hwnd)], 0); s.end(0); }
        }
    }
    /// ask "save changes?" first when the text changed, then do `t`
    fn guard(&mut self, s: &mut Sc, t: Then) {
        if !self.ed.modified { self.then(s, t); return; }
        let mut m = [0u8; 300]; let nm = self.name(); let k = nm.len().min(200);
        m[..8].copy_from_slice(b"Save to "); m[8..8 + k].copy_from_slice(&nm[..k]); m[8 + k] = b'?';
        let txt = s.zstr(&m[..9 + k]); let cap = s.zstr(b"Notepad");
        s.call(MESSAGEBOXA, &[A::V(self.hwnd), A::V(txt), A::V(cap), A::V(0x33)], 0);
        s.ask(); set_pend(s, Pend::Confirm(t), self.idx);
    }
    fn ofn(&mut self, s: &mut Sc, save: bool) -> u32 {
        // OPENFILENAME (76 bytes) at scratch+0, path buffer at scratch+80 (260 bytes), then filter and def ext
        let ofn = s.bytes(&[0u8; 80]); let fbuf = s.bytes(&[0u8; 260]);
        if save { let o = (fbuf - ofn) as usize + 80 - 80; let n = self.plen.min(259); s.s[o..o + n].copy_from_slice(&self.path[..n]); }
        let filt = s.bytes(b"Text Documents (*.txt)\0*.txt\0All Files (*.*)\0*.*\0\0");
        let ext = s.zstr(b"txt");
        let flags: u32 = if save { 0x0008_0000 | 0x800 | 4 | 2 } else { 0x0008_0000 | 0x1000 | 0x800 | 4 };
        let v = [76u32, self.hwnd, g().hinst, filt, 0, 0, 1, fbuf, 260, 0, 0, 0, 0, flags, 0, ext, 0, 0, 0];
        let o = (ofn - SH - s.area - AREA_SCRATCH) as usize;
        for (k, x) in v.iter().enumerate() { let at = o + if k < 14 { 4 * k } else { 4 * k }; s.s[at..at + 4].copy_from_slice(&x.to_le_bytes()); }
        ofn
    }
    fn open_dlg(&mut self, s: &mut Sc, _: bool) {
        let ofn = self.ofn(s, false); s.call(GETOPENFILENAMEA, &[A::V(ofn)], 0); s.ask(); set_pend(s, Pend::OpenDlg, self.idx);
    }
    fn save(&mut self, s: &mut Sc, then: Then, as_new: bool) {
        if self.plen == 0 || as_new {
            let ofn = self.ofn(s, true); s.call(GETSAVEFILENAMEA, &[A::V(ofn)], 0); s.ask(); set_pend(s, Pend::SaveDlg(then), self.idx);
        } else { self.write_file_script(s, then); }
    }
    fn write_file_script(&mut self, s: &mut Sc, then: Then) {
        let n = self.ed.n as u32;
        unsafe { b76g_write(SH + FILEBUF, self.ed.t.as_ptr(), n); }
        let p = s.zstr(&self.path[..self.plen]); let (hs, gs) = (s.slot(0), s.slot(1));
        s.call(CREATEFILEA, &[A::V(p), A::V(0x4000_0000), A::V(0), A::V(0), A::V(2), A::V(0x80), A::V(0)], hs);
        s.call(WRITEFILE, &[A::I(hs), A::V(SH + FILEBUF), A::V(n), A::V(SH + gs), A::V(0)], 0);
        s.call(CLOSEHANDLE, &[A::I(hs)], 0);
        s.ask(); set_pend(s, Pend::SaveDone(then), self.idx);
    }
    fn command(&mut self, s: &mut Sc, id: u32) {
        match id {
            ID_NEW => self.guard(s, Then::New),
            ID_OPEN => self.guard(s, Then::Open),
            ID_SAVE => self.save(s, Then::Nothing, false),
            ID_SAVEAS => self.save(s, Then::Nothing, true),
            ID_EXIT => self.guard(s, Then::Exit),
            ID_ABOUT => {
                let t = s.zstr(b"Notepad (B76 native)\nRuns on the ESP32-P4 host; this window is a 588-byte stub.");
                let c = s.zstr(b"About Notepad"); s.call(MESSAGEBOXA, &[A::V(self.hwnd), A::V(t), A::V(c), A::V(0x40)], 0); s.end(0);
            }
            ID_WRAP => { self.ed.wrap = !self.ed.wrap; self.ed.top = 0; self.relayout(); s.call(INVALIDATERECT, &[A::V(self.hwnd), A::V(0), A::V(0)], 0); s.end(0); }
            _ => { self.edit_cmd(id); self.after_input(s); }
        }
    }
    fn edit_cmd(&mut self, id: u32) {
        match id {
            ID_UNDO => self.ed.undo(),
            ID_CUT => self.req = Req::Cut,
            ID_COPY => self.req = Req::Copy,
            ID_PASTE => self.req = Req::Paste,
            ID_DEL => { self.ed.del_sel(); }
            ID_SELALL => self.ed.select_all(),
            ID_TIME => { let mut b = [0u8; 32]; let n = fmt_time(unsafe { b76g_time() }, &mut b); self.ed.insert(&b[..n], false); }
            ID_FIND => { self.findmode = true; }
            ID_FINDNEXT => { if self.flen == 0 { self.findmode = true; } else { let f = self.find; self.ed.find_next(&f[..self.flen]); } }
            _ => {}
        }
    }
    fn on_char(&mut self, ch: u32) {
        if self.findmode {
            match ch { 13 => { let f = self.find; self.ed.find_next(&f[..self.flen]); } 27 => self.findmode = false,
                8 => self.flen = self.flen.saturating_sub(1), c if c >= 32 && self.flen < 64 => { self.find[self.flen] = c as u8; self.flen += 1; } _ => {} }
            self.relayout(); self.follow(); return;
        }
        match ch {
            8 => self.ed.backspace(),
            13 => self.ed.insert(b"\r\n", true),
            3 => self.req = Req::Copy, 22 => self.req = Req::Paste, 24 => self.req = Req::Cut, 26 => self.ed.undo(),
            9 => self.ed.insert(b"\t", true),
            c if (32..256).contains(&c) => self.ed.insert(&[c as u8], true),
            _ => {}
        }
        self.follow();
    }
    fn on_key(&mut self, vk: u32) {
        let page = self.rows() as isize;
        let (e, x) = (&mut self.ed, self.shift);
        match vk {
            0x25 => e.left(x), 0x27 => e.right(x), 0x26 => e.vert(-1, x), 0x28 => e.vert(1, x),
            0x24 => if self.ctrl { e.doc_home(x) } else { e.home(x) },
            0x23 => if self.ctrl { e.doc_end(x) } else { e.end(x) },
            0x21 => { e.vert(-page, x); e.top = e.top.saturating_sub(page as usize); }
            0x22 => { e.vert(page, x); e.top = (e.top + page as usize).min(e.nl.saturating_sub(1)); }
            0x2e => e.delete(),
            0x72 => self.edit_cmd(ID_FINDNEXT), 0x74 => self.edit_cmd(ID_TIME),
            _ => {}
        }
        self.follow();
    }
    fn scrollbar_click(&mut self, y: i32) {
        let r = self.rows() as usize; let (top, nl) = (self.ed.top, self.ed.nl);
        if y < SBW { self.ed.top = top.saturating_sub(1); }
        else if y >= self.chh - SBW { self.ed.top = (top + 1).min(nl.saturating_sub(1)); }
        else { let (t0, t1) = self.thumb(); if y < t0 { self.ed.top = top.saturating_sub(r); } else if y >= t1 { self.ed.top = (top + r).min(nl.saturating_sub(1)); } }
    }
    fn thumb(&self) -> (i32, i32) {
        let track = (self.chh - 2 * SBW).max(1); let nl = self.ed.nl.max(1) as i32;
        let t0 = SBW + track * self.ed.top as i32 / nl; (t0, (t0 + SBW).min(self.chh - SBW))
    }
    fn relayout(&mut self) { self.ed.cols = self.tcols() as usize; self.ed.relayout(); self.follow(); }
    /// keep the caret visible: rows always, columns when not wrapping (jump a quarter of the width, like the 1995 edit control)
    fn follow(&mut self) {
        self.ed.scroll_to_caret(self.rows() as usize);
        if self.ed.wrap { self.hs = 0; return; }
        let c = self.ed.col_of(self.ed.caret) as i32; let v = self.tcols();
        if c < self.hs { self.hs = (c - v / 4).max(0); } else if c >= self.hs + v { self.hs = c - v + 1 + v / 4; }
    }

    // ---------------- drawing ----------------
    /// render band by band (16 px = one text row) and send each band out at once: to the guest DIB (dib), or straight to the
    /// screen surface when its pixels changed (hash per band, no back buffer: 0 B per window instead of 2 MB)
    fn render(&mut self, dib: bool, force: bool) {
        let (w, h) = (self.cw.min(MAXW as i32), self.chh.min(MAXH as i32)); if w <= 0 || h <= 0 { return; }
        let nb = (h + 15) / 16; let stride = (w * 2 + 3) & !3; let mut any = false;
        for b in 0..nb {
            let bh = 16.min(h - b * 16);
            for i in 0..(w * bh) as usize { g().band[i] = 0xffff; }
            self.render_band(b, w, bh);
            let n = (w * bh) as usize; let mut x: u64 = 0xcbf29ce484222325;
            for &v in &g().band[..n] { x = (x ^ v as u64).wrapping_mul(0x100000001b3); }
            if dib {
                for y in 0..bh { let gy = b * 16 + y;
                    unsafe { b76g_write(SH + DIB + ((h - 1 - gy) * stride) as u32, g().band.as_ptr().add((y * w) as usize) as *const u8, (w * 2) as u32); } }
            } else if force || self.bh[b as usize & 63] != x { self.blit_band(b, w, bh); any = true; }
            self.bh[b as usize & 63] = x;
        }
        if any { g().direct_draws += 1; }
    }
    fn render_band(&mut self, b: i32, w: i32, bh: i32) {
        let y0 = b * 16; let tw = w - SBW; let rows = self.rows();
        let row = b; // bands are text rows (16 px)
        let (sa, sb) = self.ed.sel();
        if row < rows {
            let l = self.ed.top + row as usize;
            if l < self.ed.nl {
                let (s, e) = (self.ed.line_start(l), self.ed.line_end(l));
                let mut dc = 0i32;
                for p in s..e {
                    let c = self.ed.t[p]; let wch = if c == 9 { 8 - dc % 8 } else { 1 };
                    let sel = p >= sa && p < sb;
                    for k in 0..wch { let g = if c == 9 { b' ' } else { cp1252_437(c) }; if dc + k >= self.hs { self.glyph((dc + k - self.hs) * CW, 0, g, sel, tw, bh); } }
                    dc += wch;
                }
                // a selected line break shows as one selected cell
                if e < self.ed.n && e >= sa && e < sb && dc >= self.hs { self.fill(((dc - self.hs) * CW).min(tw), 0, CW, bh, 0x0010, tw); }
                if self.ed.caret >= s && self.ed.caret <= e && (self.ed.caret < e || l + 1 >= self.ed.nl || self.ed.line_start(l + 1) != e) {
                    let cx = (self.ed.col_of(self.ed.caret) as i32 - self.hs) * CW; if self.focused || !self.painted { self.fill(cx, 0, 1, bh, 0x0000, tw); }
                }
            }
        }
        // scrollbar (classic grey, bevelled thumb and arrow boxes) in the right SBW px of every band
        let sx = w - SBW;
        for y in 0..bh { let gy = y0 + y; for x in 0..SBW {
            let mut c = if (x + gy) & 1 == 0 { 0xc618 } else { 0xffff }; // dithered track
            let (t0, t1) = self.thumb();
            let in_box = |a: i32, z: i32| gy >= a && gy < z;
            if in_box(0, SBW) || in_box(self.chh - SBW, self.chh) || in_box(t0, t1) {
                let (a, z) = if in_box(0, SBW) { (0, SBW) } else if in_box(t0, t1) { (t0, t1) } else { (self.chh - SBW, self.chh) };
                c = 0xc618; if x == 0 || gy == a { c = 0xffff; } if x == SBW - 1 || gy == z - 1 { c = 0x0000; } else if x == SBW - 2 || gy == z - 2 { c = 0x8410; }
                if a != t0 || z != t1 { let mid = (a + z) / 2; let dy = if a == 0 { gy - (mid - 2) } else { (mid + 2) - gy };
                    if dy >= 0 && dy < 4 && (x - 7).abs() <= dy && x > 1 && x < SBW - 2 { c = 0x0000; } }
            }
            g().band[(y * w + sx + x) as usize] = c;
        } }
        // find bar
        if self.findmode && y0 + bh > self.chh - FINDH {
            for y in 0..bh { let gy = y0 + y; if gy < self.chh - FINDH { continue; }
                for x in 0..tw { g().band[(y * w + x) as usize] = if gy == self.chh - FINDH { 0xffff } else { 0xc618 }; } }
            if y0 <= self.chh - FINDH + 2 && self.chh - FINDH + 2 < y0 + 16 {
                let oy = self.chh - FINDH + 2 - y0; let mut t = [0u8; 80]; t[..6].copy_from_slice(b"Find: ");
                t[6..6 + self.flen].copy_from_slice(&self.find[..self.flen]); t[6 + self.flen] = b'_';
                for (i, &c) in t[..7 + self.flen].iter().enumerate() { self.glyph_y(4 + i as i32 * CW, oy, c, tw, bh); }
            }
        }
    }
    fn fill(&mut self, x: i32, y: i32, wd: i32, hh: i32, c: u16, clipw: i32) {
        let w = self.cw.min(MAXW as i32);
        for yy in y..(y + hh) { for xx in x.max(0)..(x + wd).min(clipw) { g().band[(yy * w + xx) as usize] = c; } }
    }
    fn glyph(&mut self, x: i32, y: i32, c: u8, sel: bool, clipw: i32, bh: i32) {
        let (fg, bg) = if sel { (0xffff, 0x0010) } else { (0x0000, 0xffff) };
        let w = self.cw.min(MAXW as i32);
        for r in 0..bh.min(16) { let bits = g().font[c as usize * 16 + r as usize];
            for k in 0..8 { let xx = x + k; if xx >= clipw { break; } g().band[((y + r) * w + xx) as usize] = if bits & (0x80 >> k) != 0 { fg } else { bg }; } }
    }
    fn glyph_y(&mut self, x: i32, oy: i32, c: u8, clipw: i32, bh: i32) {
        let w = self.cw.min(MAXW as i32);
        for r in 0..16 { let yy = oy + r; if yy < 0 || yy >= bh { continue; } let bits = g().font[c as usize * 16 + r as usize];
            for k in 0..8 { let xx = x + k; if xx >= clipw { break; } if bits & (0x80 >> k) != 0 { g().band[(yy * w + xx) as usize] = 0; } } }
    }
    /// host-side redraw: when the window is visible and on top, straight into the screen surface (no guest work)
    fn redraw(&mut self) { if self.can_direct() { self.render(false, false); } }
    fn blit_band(&mut self, b: i32, w: i32, bh: i32) {
        let (mut sw, mut shh) = (0, 0); let fb = unsafe { b76g_fb(&mut sw, &mut shh) };
        let work_h = shh - 28; // shortcut: the taskbar is the only topmost window and sits at the bottom 28 px; read the work area when B72 gives it
        for y in 0..bh {
            let gy = self.cy + b * 16 + y; if gy < 0 || gy >= work_h { continue; }
            let x0 = self.cx.max(0); let x1 = (self.cx + w).min(sw); if x1 <= x0 { continue; }
            unsafe { let d = fb.add((gy * sw + x0) as usize); let s = g().band.as_ptr().add((y * w + x0 - self.cx) as usize);
                core::ptr::copy_nonoverlapping(s, d, (x1 - x0) as usize); b76g_dirty(gy, x0, x1); }
        }
    }
    fn dib_need(&self) -> u32 { (((self.cw * 2 + 3) & !3) * self.chh) as u32 }
    fn paint(&mut self, s: &mut Sc) {
        if !self.painted { log(b"B76 first paint"); }
        let ps = s.zeros(64);
        if self.can_direct() {
            self.render(false, true); // the guest asked: everything is shown again
            s.call(BEGINPAINT, &[A::V(self.hwnd), A::V(ps)], 0); s.call(ENDPAINT, &[A::V(self.hwnd), A::V(ps)], 0); s.end(0);
            self.painted = true; return;
        }
        // covered or inactive: Windows clips, so hand it the pixels as a DIB (16 bpp BI_BITFIELDS 565 = the surface format, bottom-up)
        let need = self.dib_need();
        if need > g().dib_ok { s.touch(SH + DIB, need); s.ask(); set_pend(s, Pend::DibTouch, self.idx); return; }
        g().dib_paints += 1;
        let (w, h) = (self.cw, self.chh); let stride = (w * 2 + 3) & !3; // DIB rows are dword aligned
        let mut bmi = [0u32; 13]; bmi[0] = 40; bmi[1] = w as u32; bmi[2] = h as u32; bmi[3] = 1 | (16 << 16); bmi[4] = 3;
        bmi[10] = 0xf800; bmi[11] = 0x07e0; bmi[12] = 0x001f;
        unsafe { b76g_write(SH + BMI, bmi.as_ptr() as *const u8, 52); }
        self.render(true, false); let _ = stride;
        let hdc = s.slot(6);
        s.call(BEGINPAINT, &[A::V(self.hwnd), A::V(ps)], hdc);
        s.call(SETDIBITSTODEVICE, &[A::I(hdc), A::V(0), A::V(0), A::V(w as u32), A::V(h as u32), A::V(0), A::V(0), A::V(0), A::V(h as u32),
            A::V(SH + DIB), A::V(SH + BMI), A::V(0)], 0);
        s.call(ENDPAINT, &[A::V(self.hwnd), A::V(ps)], 0); s.end(0); self.painted = true;
    }
}

// ---------------- resident stub, DDE server, window dispatch ----------------
const WM_DDE_INITIATE: u32 = 0x3e0; const WM_DDE_TERMINATE: u32 = 0x3e1; const WM_DDE_ACK: u32 = 0x3e4; const WM_DDE_EXECUTE: u32 = 0x3e8;
const WM_B76_OPEN: u32 = 0x8000 + 76; // WM_APP+76: a forwarding stub tells the resident "open the queued path"
const HKCR: u32 = 0x8000_0000;

fn lvl_of(s: &Sc) -> usize { (s.area >> 12) as usize & 15 }
/// copy a NUL-terminated guest string (page by page) into `o`, returns its length
fn read_cstr(lin: u32, o: &mut [u8]) -> usize {
    let mut n = 0usize; let mut chunk = [0u8; 256];
    while n < o.len() { let m = 256.min(o.len() - n);
        if unsafe { b76g_read(lin + n as u32, chunk.as_mut_ptr(), m as u32) } != 0 { break; }
        for &c in &chunk[..m] { if c == 0 { return n; } o[n] = c; n += 1; } }
    n
}

// ---------------- B82 Doom window ----------------
const DOOM: usize = NW + 1; // `creating` / `pend_win` marker of the Doom window
const DOOM_DIB: u32 = 1280 * 800 * 2;
fn base_name(t: &[u8]) -> &[u8] { t.iter().rposition(|&c| c == b'\\' || c == b'/' || c == b':').map_or(t, |i| &t[i + 1..]) }
/// an empty `*.b76` document (Start menu / desktop entry): its name picks the native app
fn b76_doc(t: &[u8]) -> bool { let b = base_name(t); b.len() > 4 && (b[b.len() - 4..].eq_ignore_ascii_case(b".b76") || game_ext(b).is_some()) }
/// B82 games: own document types so each gets its own icon (DefaultIcon = a .ICO in C:\WINDOWS): .dm1 Doom, .dmu Ultimate Doom, .dm2 Doom II
/// B83: the same three on the Rust engine (.dr1/.dru/.dr2): index 3..5 = engine 1. They reuse the C classes (same DDE open; the
/// extension tells the resident the engine; the desktop/Games .LNK carries the badged icon): six full classes overflowed the 2 KB
/// registration scratch (board B 21 Nov: the guest stopped at login)
const GAMES: [(&[u8], &[u8], &[u8], &[u8]); 6] = [
    (b".dm1", b"b82dm1", b"Doom", b"C:\\WINDOWS\\DOOM1.ICO"),
    (b".dmu", b"b82dmu", b"The Ultimate Doom", b"C:\\WINDOWS\\DOOMU.ICO"),
    (b".dm2", b"b82dm2", b"Doom II", b"C:\\WINDOWS\\DOOM2.ICO"),
    (b".dr1", b"b82dm1", b"", b""),
    (b".dru", b"b82dmu", b"", b""),
    (b".dr2", b"b82dm2", b"", b""),
];
/// game index -> b82_open argument: WAD in bits 0-7, engine (B83: 1 = Rust) in bit 8
fn game_ext(b: &[u8]) -> Option<i32> { if b.len() < 5 { return None; } let e = &b[b.len() - 4..]; GAMES.iter().position(|g| e.eq_ignore_ascii_case(g.0)).map(|i| (i as i32 % 3) | (i as i32 / 3) << 8) }
/// which Doom a `.b76` entry names: "Doom" = shareware, "Ultimate Doom" = doom.wad, "Doom II" = doom2.wad (long or 8.3 names)
fn doom_wad(t: &[u8]) -> Option<i32> {
    let b = base_name(t);
    if let Some(g) = game_ext(b) { return Some(g); } let st = &b[..b.len().saturating_sub(4)];
    let mut n = [0u8; 32]; let mut k = 0;
    for &c in st { if c != b' ' && c != b'_' && k < 32 { n[k] = c.to_ascii_lowercase(); k += 1; } }
    let n = &n[..k];
    if n == b"doom" { Some(0) } else if n.starts_with(b"ultima") { Some(1) } else if n == b"doomii" || n.starts_with(b"doomii~") || n == b"doom2" { Some(2) } else { None }
}
fn is_doom(t: &[u8]) -> bool { doom_wad(t).is_some() }
/// one Doom window at most: a second launch brings it back. Topmost popup over the whole panel (taskbar button, Alt+Tab, no frame)
fn open_doom(s: &mut Sc, wad: i32) {
    let gl = g();
    if gl.doom_hwnd != 0 { s.call(SHOWWINDOW, &[A::V(gl.doom_hwnd), A::V(9)], 0); s.call(SETFOCUS, &[A::V(gl.doom_hwnd)], 0); s.end(0); return; }
    let r = unsafe { b82_open(wad) };
    if r < 0 {
        let t = s.zstr(b"Doom"); let m = s.zstr(if r == -3 { b"Doom is not installed on this board (the Doom app partition is empty)." as &[u8] } else if wad & 255 == 0 { b"Doom needs DOOM\\doom1.wad on the SD card." as &[u8] } else if wad & 255 == 1 { b"The Ultimate Doom needs DOOM\\doom.wad (your own copy) on the SD card." } else { b"Doom II needs DOOM\\doom2.wad (your own copy) on the SD card." });
        s.call(MESSAGEBOXA, &[A::V(0), A::V(m), A::V(t), A::V(0x30)], 0); s.end(0); return;
    }
    let cls = s.zstr(b"B76Pad"); let t = s.zstr(b"Doom"); let hw = s.slot(4);
    gl.creating = DOOM;
    s.call(CREATEWINDOWEXA, &[A::V(8), A::V(cls), A::V(t), A::V(0x9000_0000), A::V(0), A::V(0), A::V(1280), A::V(800), A::V(0), A::V(0), A::V(gl.hinst), A::V(0)], hw);
    s.call(SETFOCUS, &[A::I(hw)], 0);
    s.end(0);
    log(b"B82 doom window");
}
fn doom_msg(s: &mut Sc, h: &[u32; 4]) {
    let (hw, m, wp, lp) = (h[0], h[1], h[2], h[3]); let gl = g();
    match m {
        WM_SETFOCUS => { gl.doom_focus = true; unsafe { b82_focus(1) } s.end(0); }
        WM_KILLFOCUS => { gl.doom_focus = false; unsafe { b82_focus(0) } s.end(0); }
        WM_ERASEBKGND => s.end(1),
        WM_PAINT => doom_paint(s),
        WM_KEYDOWN if wp == VK_F12 => { if unsafe { b82_take_quit() } != 0 { s.call(DESTROYWINDOW, &[A::V(hw)], 0); } s.end(0); }
        WM_DESTROY => { gl.doom_hwnd = 0; gl.doom_focus = false; unsafe { b82_close() } s.end(0); }
        WM_KEYDOWN | WM_KEYUP | WM_CHAR => s.end(0), // the game takes its keys on the host
        _ => { s.call(DEFWINDOWPROCA, &[A::V(hw), A::V(m), A::V(wp), A::V(lp)], 0); s.end_last(); } // Alt+F4, system menu, Alt+Tab: USER's own code
    }
}
/// focused: the game owns the panel, only validate. Otherwise Windows clips: hand it the last frame as a DIB
fn doom_paint(s: &mut Sc) {
    let gl = g(); let hw = gl.doom_hwnd; let ps = s.zeros(64);
    if gl.doom_focus || hw == 0 { s.call(BEGINPAINT, &[A::V(hw), A::V(ps)], 0); s.call(ENDPAINT, &[A::V(hw), A::V(ps)], 0); s.end(0); return; }
    if DOOM_DIB > gl.dib_ok { s.touch(SH + DIB, DOOM_DIB); s.ask(); set_pend(s, Pend::DibTouch, DOOM); return; }
    unsafe { b82_dib(SH + BMI, SH + DIB); }
    let hdc = s.slot(6);
    s.call(BEGINPAINT, &[A::V(hw), A::V(ps)], hdc);
    s.call(SETDIBITSTODEVICE, &[A::I(hdc), A::V(0), A::V(0), A::V(1280), A::V(800), A::V(0), A::V(0), A::V(0), A::V(800), A::V(SH + DIB), A::V(SH + BMI), A::V(0)], 0);
    s.call(ENDPAINT, &[A::V(hw), A::V(ps)], 0); s.end(0);
}

/// program token (path, with or without .EXE, any case) names Notepad
fn is_notepad(t: &[u8]) -> bool {
    let b = t.iter().rposition(|&c| c == b'\\' || c == b'/').map_or(t, |i| &t[i + 1..]);
    let b = if b.len() > 4 && b[b.len() - 4..].eq_ignore_ascii_case(b".exe") { &b[..b.len() - 4] } else { b };
    b.eq_ignore_ascii_case(b"notepad")
}
/// op 1. Command line: none = the resident stub (started from WIN.INI run= or by the shell for a DDE open);
/// "notepad [file]" = a classic launch: forwarded to the resident when one runs, else this process becomes the resident.
fn start(s: &mut Sc, h: &[u32; 4]) {
    let mut cl = [0u8; 300]; let n = read_cstr(h[0], &mut cl);
    let mut toks = [(0usize, 0usize); 3]; let mut nt = 0; let mut i = 0;
    while i < n && nt < 3 {
        while i < n && cl[i] == b' ' { i += 1; } if i >= n { break; }
        let (st, en);
        if cl[i] == b'"' { st = i + 1; i += 1; while i < n && cl[i] != b'"' { i += 1; } en = i; i += 1; }
        else if nt == 2 { st = i; en = n; i = n; } // the file name may contain spaces
        else { st = i; while i < n && cl[i] != b' ' { i += 1; } en = i; }
        toks[nt] = (st, en); nt += 1;
    }
    let gl = g();
    // started under the name NOTEPAD.EXE (our copy in the Windows dir: Start menu, Run box, programs that spawn notepad): always a window, the rest is the file
    let np = nt >= 1 && is_notepad(&cl[toks[0].0..toks[0].1]);
    let (fa, fb) = if np { if nt >= 2 { (toks[1].0, if cl[toks[1].0 - 1] == b'"' { toks[1].1 } else { n }) } else { (0, 0) } } else if nt == 3 { toks[2] } else { (0, 0) };
    let l = (fb - fa).min(259); gl.open_q[..l].copy_from_slice(&cl[fa..fa + l]); gl.open_qn = l;
    gl.open_qv = np || nt >= 2; // "notepad" given = open a window
    if gl.dde_hwnd != 0 {
        // a resident already owns the windows: hand the request over and leave (no second host state)
        if gl.open_qv { s.call(POSTMESSAGEA, &[A::V(gl.dde_hwnd), A::V(WM_B76_OPEN), A::V(0), A::V(0)], 0); }
        s.call(EXITPROCESS, &[A::V(0)], 0); s.end(0); return;
    }
    gl.wndproc = h[1]; gl.hinst = h[2];
    // become the resident: associations (rewritten each login; the disk is a RAM overlay on the board), atoms, class, DDE window
    let keys: [(&[u8], &[u8]); 12] = [
        (b".txt", b"txtfile"),
        (b"txtfile\\shell\\open\\command", b"C:\\WINDOWS\\B76APP.EXE"),
        (b"txtfile\\shell\\open\\ddeexec", b"[open(\"%1\")]"),
        (b"txtfile\\shell\\open\\ddeexec\\application", b"B76Pad"),
        (b"txtfile\\shell\\open\\ddeexec\\topic", b"System"),
        (b".b76", b"b76app"),
        (b"b76app", b"Native app"),
        (b"b76app\\DefaultIcon", b"C:\\WINDOWS\\NOTEPAD.EXE,0"),
        (b"b76app\\shell\\open\\command", b"C:\\WINDOWS\\B76APP.EXE"),
        (b"b76app\\shell\\open\\ddeexec", b"[open(\"%1\")]"), // B82: the document name picks the app (DOOM.B76 = Doom, others = new Notepad)
        (b"b76app\\shell\\open\\ddeexec\\application", b"B76Pad"),
        (b"b76app\\shell\\open\\ddeexec\\topic", b"System"),
    ];
    for (k, v) in keys.iter() { let kp = s.zstr(k); let vp = s.zstr(v); s.call(REGSETVALUEA, &[A::V(HKCR), A::V(kp), A::V(1), A::V(vp), A::V(v.len() as u32)], 0); }
    // B82: one class per game (icon + the same DDE open as .b76); keys built in a stack buffer, all strings go to this area's 2 KB scratch
    for (gi, g) in GAMES.iter().enumerate() {
        if gi >= 3 { let kp = s.zstr(g.0); let vp = s.zstr(g.1); s.call(REGSETVALUEA, &[A::V(HKCR), A::V(kp), A::V(1), A::V(vp), A::V(g.1.len() as u32)], 0); continue; }
        let mut kb = [0u8; 48];
        let mut set = |s: &mut Sc, sub: &[u8], v: &[u8]| {
            let n = g.1.len(); kb[..n].copy_from_slice(g.1); kb[n..n + sub.len()].copy_from_slice(sub);
            let kp = s.zstr(&kb[..n + sub.len()]); let vp = s.zstr(v); s.call(REGSETVALUEA, &[A::V(HKCR), A::V(kp), A::V(1), A::V(vp), A::V(v.len() as u32)], 0);
        };
        { let kp = s.zstr(g.0); let vp = s.zstr(g.1); s.call(REGSETVALUEA, &[A::V(HKCR), A::V(kp), A::V(1), A::V(vp), A::V(g.1.len() as u32)], 0); }
        set(s, b"", g.2); set(s, b"\\DefaultIcon", g.3); set(s, b"\\shell\\open\\command", b"C:\\WINDOWS\\B76APP.EXE");
        set(s, b"\\shell\\open\\ddeexec", b"[open(\"%1\")]"); set(s, b"\\shell\\open\\ddeexec\\application", b"B76Pad");
        set(s, b"\\shell\\open\\ddeexec\\topic", b"System");
    }
    if s.sn > 1900 { log(b"B82 WARNING start scratch nearly full"); }
    let (aa, at) = (s.slot(0), s.slot(1));
    let pa = s.zstr(b"B76Pad"); let pt = s.zstr(b"System");
    s.call(GLOBALADDATOMA, &[A::V(pa)], aa); s.call(GLOBALADDATOMA, &[A::V(pt)], at);
    let cls = s.zstr(b"B76Pad");
    let wc = s.bytes(&[0u8; 40]); // WNDCLASS: style, wndproc, cls, wnd, hinst, icon, cursor, brush, menu, class
    let wcv = [3u32, gl.wndproc, 0, 0, gl.hinst, 0, 0, 0, 0, cls];
    let off = (wc - SH - s.area - AREA_SCRATCH) as usize;
    for (k, v) in wcv.iter().enumerate() { s.s[off + 4 * k..off + 4 * k + 4].copy_from_slice(&v.to_le_bytes()); }
    let wco = wc - SH;
    s.call(LOADICONA, &[A::V(0), A::V(32512)], wco + 20);
    s.call(LOADCURSORA, &[A::V(0), A::V(32513)], wco + 24);
    s.call(REGISTERCLASSA, &[A::V(wc)], 0);
    s.ask(); set_pend(s, Pend::Atoms, usize::MAX);
}
/// allocate a window slot for `path` (empty = Untitled) and run its open script
fn open_window(s: &mut Sc, path: &[u8]) {
    if b76_doc(path) { if let Some(w) = doom_wad(path) { open_doom(s, w); } else { open_window(s, &[]); } return; }
    let Some(i) = g().wins.iter().position(|w| !w.used) else { s.end(0); return; };
    g().opens += 1;
    let w = &mut g().wins[i]; w.used = true; w.hwnd = 0; w.focused = false; w.inmenu = false; w.sizing = false; w.minimized = false;
    w.findmode = false; w.flen = 0; w.hs = 0; w.painted = false; w.bh = [0; 64]; w.req = Req::None; w.ed.wrap = false; w.ed.set(&[]);
    let l = path.len().min(259); w.path[..l].copy_from_slice(&path[..l]); w.plen = l;
    if l > 0 { set_pend(s, Pend::StartRead, i); w.read_file_script(s); } else { w.create_window(s); }
}
fn msg(s: &mut Sc, h: &[u32; 4]) {
    let (hw, m, wp, lp) = (h[0], h[1], h[2], h[3]);
    let gl = g();
    if hw != 0 && hw == gl.doom_hwnd { doom_msg(s, h); return; }
    if let Some(i) = gl.wins.iter().position(|w| w.used && w.hwnd == hw) { gl.wins[i].wmsg(s, h); return; }
    // first message of a window being created: the creating slot (Notepad) or the DDE server window
    if gl.creating != usize::MAX && hw != gl.dde_hwnd {
        let i = gl.creating;
        if i == DOOM { gl.doom_hwnd = hw; gl.creating = usize::MAX; doom_msg(s, h); return; }
        if i == NW { gl.dde_hwnd = hw; } else { gl.wins[i].hwnd = hw; }
        gl.creating = usize::MAX;
        if i < NW { gl.wins[i].wmsg(s, h); return; }
    }
    if hw == gl.dde_hwnd && hw != 0 {
        match m {
            WM_DDE_INITIATE => {
                let (app, top) = (lp & 0xffff, lp >> 16);
                if (app == gl.atom_app || app == 0) && (top == gl.atom_topic || top == 0) {
                    let pa = s.zstr(b"B76Pad"); let pt = s.zstr(b"System");
                    s.call(GLOBALADDATOMA, &[A::V(pa)], 0); s.call(GLOBALADDATOMA, &[A::V(pt)], 0); // the reply owns its own atom references
                    s.call(SENDMESSAGEA, &[A::V(wp), A::V(WM_DDE_ACK), A::V(hw), A::V(gl.atom_app | gl.atom_topic << 16)], 0);
                }
                s.end(0); return;
            }
            WM_DDE_EXECUTE => { s.call(GLOBALLOCK, &[A::V(lp)], 0); s.ask(); set_pend(s, Pend::DdeExec(wp, lp), usize::MAX); return; }
            WM_DDE_TERMINATE => { s.call(POSTMESSAGEA, &[A::V(wp), A::V(WM_DDE_TERMINATE), A::V(hw), A::V(0)], 0); s.end(0); return; }
            WM_B76_OPEN => { let q = gl.open_q; let n = gl.open_qn; open_window(s, &q[..n]); return; }
            _ => {}
        }
    }
    s.call(DEFWINDOWPROCA, &[A::V(hw), A::V(m), A::V(wp), A::V(lp)], 0); s.end_last();
}
fn ask(s: &mut Sc, last: u32) {
    let l = lvl_of(s); let p = g().pend[l]; let wi = g().pend_win[l]; g().pend[l] = Pend::None;
    match p {
        Pend::Atoms => {
            let gl = g(); gl.atom_app = rd32(SH + s.slot(0)) & 0xffff; gl.atom_topic = rd32(SH + s.slot(1)) & 0xffff;
            // hidden top-level DDE server window (receives the shell's broadcast WM_DDE_INITIATE)
            let cls = s.zstr(b"B76Pad"); let t = s.zstr(b"B76 resident");
            gl.creating = NW;
            s.call(CREATEWINDOWEXA, &[A::V(0), A::V(cls), A::V(t), A::V(0x8000_0000), A::V(0), A::V(0), A::V(0), A::V(0), A::V(0), A::V(0), A::V(gl.hinst), A::V(0)], 0);
            if gl.open_qv { s.ask(); set_pend(s, Pend::DdeExec(0, 0), usize::MAX); } else { s.end(0); }
            log(b"B76 resident up");
        }
        Pend::DdeExec(0, 0) => { let q = g().open_q; let n = g().open_qn; open_window(s, &q[..n]); }
        Pend::DdeExec(client, hcmd) => {
            let mut c = [0u8; 300]; let n = if last != 0 { read_cstr(last, &mut c) } else { 0 };
            let ack = s.slot(2);
            s.call(GLOBALUNLOCK, &[A::V(hcmd)], 0);
            s.call(PACKDDELPARAM, &[A::V(WM_DDE_ACK), A::V(if last != 0 { 0x8000 } else { 0 }), A::V(hcmd)], ack);
            s.call(POSTMESSAGEA, &[A::V(client), A::V(WM_DDE_ACK), A::V(g().dde_hwnd), A::I(ack)], 0);
            // [open("path")] or [new()]
            let cmd = &c[..n]; let q0 = cmd.iter().position(|&x| x == b'"');
            let path: &[u8] = match q0 { Some(a) => { let r = &cmd[a + 1..]; &r[..r.iter().position(|&x| x == b'"').unwrap_or(r.len())] } None => &[] };
            let mut pb = [0u8; 260]; let pl = path.len().min(259); pb[..pl].copy_from_slice(&path[..pl]);
            open_window(s, &pb[..pl]);
        }
        Pend::DibTouch if wi == DOOM => { g().dib_ok = DOOM_DIB; doom_paint(s); }
        _ => { if wi < NW { g().wins[wi].wask(s, last, p); } else { s.end(0); } }
    }
}

/// CP1252 byte -> glyph index of the CP437 VGA font (the file keeps its bytes; only the drawing maps). '?' where 437 has no glyph.
fn cp1252_437(c: u8) -> u8 {
    const T: [u8; 128] = [0x45, 0x3f, 0x2c, 0x9f, 0x22, 0x2e, 0x2b, 0x2b, 0x5e, 0x25, 0x53, 0x3c, 0x4f, 0x3f, 0x5a, 0x3f, 0x3f, 0x27, 0x27, 0x22, 0x22, 0x07, 0x2d, 0x2d, 0x7e, 0x54, 0x73, 0x3e, 0x6f, 0x3f, 0x7a, 0x59, 0xff, 0xad, 0x9b, 0x9c, 0x2a, 0x9d, 0x3f, 0x3f, 0x22, 0x63, 0xa6, 0xae, 0xaa, 0x2d, 0x52, 0x2d, 0xf8, 0xf1, 0xfd, 0x33, 0x27, 0xe6, 0x3f, 0xfa, 0x2c, 0x31, 0xa7, 0xaf, 0xac, 0xab, 0x3f, 0xa8, 0x41, 0x41, 0x41, 0x41, 0x8e, 0x8f, 0x92, 0x80, 0x45, 0x90, 0x45, 0x45, 0x49, 0x49, 0x49, 0x49, 0x44, 0xa5, 0x4f, 0x4f, 0x4f, 0x4f, 0x99, 0x78, 0x4f, 0x55, 0x55, 0x55, 0x9a, 0x59, 0x50, 0xe1, 0x85, 0xa0, 0x83, 0x61, 0x84, 0x86, 0x91, 0x87, 0x8a, 0x82, 0x88, 0x89, 0x8d, 0xa1, 0x8c, 0x8b, 0x64, 0xa4, 0x95, 0xa2, 0x93, 0x6f, 0x94, 0xf6, 0x6f, 0x97, 0xa3, 0x96, 0x81, 0x79, 0x70, 0x98];
    if c < 0x80 { c } else { T[(c - 0x80) as usize] }
}

/// "h:mm AM m/d/yyyy" (the 1995 Time/Date order)
pub fn fmt_time(t: u32, o: &mut [u8; 32]) -> usize {
    let days = (t / 86400) as i64; let sod = t % 86400; let (hh, mm) = (sod / 3600, sod % 3600 / 60);
    let z = days + 719468; let era = z.div_euclid(146097); let doe = z - era * 146097;
    let yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153; let d = doy - (153 * mp + 2) / 5 + 1; let m = if mp < 10 { mp + 3 } else { mp - 9 };
    let y = yoe + era * 400 + if m <= 2 { 1 } else { 0 };
    let h12 = if hh % 12 == 0 { 12 } else { hh % 12 };
    fn num(o: &mut [u8; 32], n: &mut usize, v: u64, w: usize) {
        let mut b = [0u8; 8]; let mut k = 0; let mut v = v;
        loop { b[k] = b'0' + (v % 10) as u8; v /= 10; k += 1; if v == 0 && k >= w { break; } }
        while k > 0 { k -= 1; o[*n] = b[k]; *n += 1; }
    }
    fn lit(o: &mut [u8; 32], n: &mut usize, s: &[u8]) { for &c in s { o[*n] = c; *n += 1; } }
    let mut n = 0;
    num(o, &mut n, h12 as u64, 1); lit(o, &mut n, b":"); num(o, &mut n, mm as u64, 2);
    lit(o, &mut n, if hh < 12 { b" AM " } else { b" PM " });
    num(o, &mut n, m as u64, 1); lit(o, &mut n, b"/"); num(o, &mut n, d as u64, 1); lit(o, &mut n, b"/"); num(o, &mut n, y as u64, 4);
    n
}

#[cfg(test)]
mod t {
    #[test] fn time_fmt() { let mut b = [0u8; 32]; let n = super::fmt_time(1794000000, &mut b); assert_eq!(&b[..n], b"9:20 PM 11/6/2026"); }
}
#[cfg(test)]
mod t82 { #[test] fn doc() { use super::*; assert!(b76_doc(b"C:\\WINDOWS\\Desktop\\Doom.b76") && is_doom(b"C:\\WINDOWS\\Desktop\\Doom.b76") && is_doom(b"DOOM.B76")
    && b76_doc(b"C:\\WINDOWS\\NOTEPAD.B76") && !is_doom(b"C:\\WINDOWS\\NOTEPAD.B76") && !b76_doc(b"x.txt") && !is_doom(b"doomed.b76")
    && doom_wad(b"Ultimate Doom.b76") == Some(1) && doom_wad(b"ULTIMA~1.B76") == Some(1) && doom_wad(b"Doom II.b76") == Some(2) && doom_wad(b"DOOMII~1.B76") == Some(2)); } }
#[cfg(test)]
mod t3 { #[test] fn np() { use super::is_notepad as f; assert!(f(b"C:\\WINDOWS\\NOTEPAD.EXE") && f(b"notepad") && f(b"Notepad.exe") && !f(b"C:\\WINDOWS\\B76APP.EXE") && !f(b"xnotepad")); } }
#[cfg(test)]
mod t2 { #[test] fn cp() { use super::cp1252_437 as f; assert_eq!((f(0xe9), f(0xc4), f(0xfc), f(0xa3), f(0x41), f(0xff)), (0x82, 0x8e, 0x81, 0x9c, 0x41, 0x98)); } }

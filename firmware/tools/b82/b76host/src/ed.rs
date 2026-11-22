//! Text model of the B76 Notepad: bytes kept exactly as in the file (CP1252, CRLF = line break), visual lines with
//! optional word wrap and 8-column tab stops. Pure logic, no I/O: unit-tested on the PC (cargo test).

pub const CAP: usize = 0x10000; // 64 KB per window (= the 1995 limit); PSRAM is shared with the disk write overlay (board B mx3: 2 MB host state starved it)
pub const MAXL: usize = 0x1000; // visual lines per window

pub struct Ed {
    pub t: [u8; CAP],
    pub n: usize,
    pub caret: usize,
    pub anchor: usize,
    pub modified: bool,
    pub wrap: bool,
    pub cols: usize,
    pub starts: [u32; MAXL],
    pub nl: usize,
    pub top: usize,
    pub want_col: usize,
    undo: [u8; CAP],
    un: usize,
    ucaret: usize,
    pub has_undo: bool,
    typing: bool,
}

impl Ed {
    pub const fn new() -> Ed {
        Ed { t: [0; CAP], n: 0, caret: 0, anchor: 0, modified: false, wrap: false, cols: 80, starts: [0; MAXL], nl: 1,
             top: 0, want_col: 0, undo: [0; CAP], un: 0, ucaret: 0, has_undo: false, typing: false }
    }
    pub fn text(&self) -> &[u8] { &self.t[..self.n] }
    pub fn set(&mut self, b: &[u8]) {
        let n = b.len().min(CAP); self.t[..n].copy_from_slice(&b[..n]); self.n = n;
        self.caret = 0; self.anchor = 0; self.top = 0; self.modified = false; self.has_undo = false; self.typing = false;
        self.relayout();
    }
    fn is_crlf(&self, p: usize) -> bool { p + 1 < self.n && self.t[p] == b'\r' && self.t[p + 1] == b'\n' }

    /// Visual line starts. A line ends at CRLF (the CRLF belongs to it), or at the wrap column.
    pub fn relayout(&mut self) {
        let cols = self.cols.max(1);
        self.starts[0] = 0; self.nl = 1;
        let (mut p, mut s, mut dc, mut last_sp) = (0usize, 0usize, 0usize, usize::MAX);
        while p < self.n {
            if self.is_crlf(p) { p += 2; self.push(p); s = p; dc = 0; last_sp = usize::MAX; continue; }
            let w = if self.t[p] == 9 { 8 - dc % 8 } else { 1 };
            if self.wrap && dc + w > cols && p > s {
                let brk = if last_sp != usize::MAX { last_sp + 1 } else { p };
                self.push(brk); s = brk; dc = self.cols_between(s, p); last_sp = usize::MAX;
                continue;
            }
            if self.t[p] == b' ' { last_sp = p; }
            dc += w; p += 1;
        }
    }
    fn push(&mut self, p: usize) { if self.nl < MAXL { self.starts[self.nl] = p as u32; self.nl += 1; } }
    fn cols_between(&self, a: usize, b: usize) -> usize {
        let mut dc = 0; for &c in &self.t[a..b] { dc += if c == 9 { 8 - dc % 8 } else { 1 }; } dc
    }
    pub fn line_start(&self, l: usize) -> usize { self.starts[l] as usize }
    /// end of the visible content of line l (excludes its CRLF)
    pub fn line_end(&self, l: usize) -> usize {
        let e = if l + 1 < self.nl { self.starts[l + 1] as usize } else { self.n };
        if e >= 2 && e > self.line_start(l) + 1 && self.t[e - 2] == b'\r' && self.t[e - 1] == b'\n' { e - 2 } else { e }
    }
    pub fn line_of(&self, p: usize) -> usize {
        // last line whose start <= p (a position at a wrap break belongs to the next line)
        let (mut lo, mut hi) = (0usize, self.nl);
        while hi - lo > 1 { let m = (lo + hi) / 2; if self.starts[m] as usize <= p { lo = m } else { hi = m } }
        lo
    }
    pub fn col_of(&self, p: usize) -> usize { let l = self.line_of(p); self.cols_between(self.line_start(l), p) }
    pub fn pos_at(&self, l: usize, col: usize) -> usize {
        let (s, e) = (self.line_start(l), self.line_end(l));
        let mut dc = 0; let mut p = s;
        while p < e { let w = if self.t[p] == 9 { 8 - dc % 8 } else { 1 }; if dc + w > col { break; } dc += w; p += 1; }
        p
    }
    pub fn sel(&self) -> (usize, usize) { (self.caret.min(self.anchor), self.caret.max(self.anchor)) }

    fn snapshot(&mut self, typing: bool) {
        if typing && self.typing { return; }
        self.undo[..self.n].copy_from_slice(&self.t[..self.n]); self.un = self.n; self.ucaret = self.caret;
        self.has_undo = true; self.typing = typing;
    }
    pub fn undo(&mut self) {
        if !self.has_undo { return; }
        // swap text and undo copy (1995 behaviour: Undo toggles the last change)
        let (a, b) = (self.n, self.un); let m = a.max(b);
        for i in 0..m { core::mem::swap(&mut self.t[i], &mut self.undo[i]); }
        self.n = b; self.un = a; let c = self.ucaret; self.ucaret = self.caret; self.caret = c.min(self.n); self.anchor = self.caret;
        self.typing = false; self.modified = true; self.relayout();
    }
    fn replace(&mut self, a: usize, b: usize, ins: &[u8]) -> bool {
        let ins = &ins[..ins.len().min(CAP - (self.n - (b - a)))];
        let tail = self.n - b;
        self.t.copy_within(b..self.n, a + ins.len());
        self.t[a..a + ins.len()].copy_from_slice(ins);
        self.n = a + ins.len() + tail; self.caret = a + ins.len(); self.anchor = self.caret; self.modified = true;
        self.relayout(); true
    }
    pub fn insert(&mut self, s: &[u8], typing: bool) {
        self.snapshot(typing); let (a, b) = self.sel(); self.replace(a, b, s);
    }
    pub fn del_sel(&mut self) -> bool {
        let (a, b) = self.sel(); if a == b { return false; } self.snapshot(false); self.replace(a, b, &[]); true
    }
    pub fn backspace(&mut self) {
        if self.del_sel() { return; }
        if self.caret == 0 { return; }
        let a = if self.caret >= 2 && self.is_crlf(self.caret - 2) { self.caret - 2 } else { self.caret - 1 };
        self.snapshot(true); let c = self.caret; self.replace(a, c, &[]);
    }
    pub fn delete(&mut self) {
        if self.del_sel() { return; }
        if self.caret >= self.n { return; }
        let b = if self.is_crlf(self.caret) { self.caret + 2 } else { self.caret + 1 };
        self.snapshot(false); let c = self.caret; self.replace(c, b, &[]);
    }
    pub fn select_all(&mut self) { self.anchor = 0; self.caret = self.n; }
    fn set_caret(&mut self, p: usize, extend: bool, keep_col: bool) {
        self.caret = p; if !extend { self.anchor = p; } self.typing = false;
        if !keep_col { self.want_col = self.col_of(p); }
    }
    pub fn left(&mut self, ext: bool) {
        let p = if self.caret >= 2 && self.is_crlf(self.caret - 2) { self.caret - 2 } else { self.caret.saturating_sub(1) };
        self.set_caret(p, ext, false);
    }
    pub fn right(&mut self, ext: bool) {
        let p = if self.is_crlf(self.caret) { self.caret + 2 } else { (self.caret + 1).min(self.n) };
        self.set_caret(p, ext, false);
    }
    pub fn vert(&mut self, d: isize, ext: bool) {
        let l = self.line_of(self.caret) as isize + d;
        let l = l.clamp(0, self.nl as isize - 1) as usize;
        let p = self.pos_at(l, self.want_col); self.set_caret(p, ext, true);
    }
    pub fn home(&mut self, ext: bool) { let p = self.line_start(self.line_of(self.caret)); self.set_caret(p, ext, false); }
    pub fn end(&mut self, ext: bool) { let p = self.line_end(self.line_of(self.caret)); self.set_caret(p, ext, false); }
    pub fn doc_home(&mut self, ext: bool) { self.set_caret(0, ext, false); }
    pub fn doc_end(&mut self, ext: bool) { let n = self.n; self.set_caret(n, ext, false); }
    pub fn click(&mut self, row: usize, col: usize, ext: bool) {
        let l = (self.top + row).min(self.nl - 1); let p = self.pos_at(l, col); self.set_caret(p, ext, false);
    }
    /// keep the caret line inside [top, top+rows)
    pub fn scroll_to_caret(&mut self, rows: usize) {
        let l = self.line_of(self.caret); let rows = rows.max(1);
        if l < self.top { self.top = l; } else if l >= self.top + rows { self.top = l + 1 - rows; }
    }
    /// case-insensitive forward search from the caret (wraps around once); selects the hit
    pub fn find_next(&mut self, pat: &[u8]) -> bool {
        if pat.is_empty() || pat.len() > self.n { return false; }
        let lc = |c: u8| c.to_ascii_lowercase();
        let start = self.sel().1;
        for k in 0..=self.n { let i = (start + k) % (self.n + 1); if i + pat.len() > self.n { continue; }
            if self.t[i..i + pat.len()].iter().zip(pat).all(|(a, b)| lc(*a) == lc(*b)) {
                self.anchor = i; self.caret = i + pat.len(); self.typing = false; return true; } }
        false
    }
}

#[cfg(test)]
mod t {
    use super::*;
    fn ed(s: &[u8], wrap: bool, cols: usize) -> Box<Ed> {
        let mut e: Box<Ed> = unsafe { let l = std::alloc::Layout::new::<Ed>(); Box::from_raw(std::alloc::alloc_zeroed(l) as *mut Ed) };
        e.wrap = wrap; e.cols = cols; e.nl = 1; e.set(s); e
    }
    #[test] fn crlf_lines() {
        let e = ed(b"ab\r\ncd\r\n", false, 80);
        assert_eq!(e.nl, 3); assert_eq!((e.line_start(1), e.line_end(1)), (4, 6)); assert_eq!(e.line_end(2), 8);
        assert_eq!(e.line_end(0), 2);
    }
    #[test] fn wrap_at_space_and_hard() {
        let e = ed(b"hello world foo", true, 8);
        assert_eq!(&e.starts[..e.nl], &[0, 6, 12]);
        let e = ed(b"abcdefghij", true, 4);
        assert_eq!(&e.starts[..e.nl], &[0, 4, 8]);
    }
    #[test] fn tabs_cols() { let e = ed(b"a\tb", false, 80); assert_eq!(e.col_of(2), 8); assert_eq!(e.pos_at(0, 8), 2); }
    #[test] fn edit_keeps_bytes() {
        let mut e = ed(b"ab\r\ncd", false, 80);
        e.caret = 4; e.anchor = 4; e.backspace(); assert_eq!(e.text(), b"abcd"); // CRLF removed as one
        e.insert(b"\r\n", true); assert_eq!(e.text(), b"ab\r\ncd");
        e.left(false); assert_eq!(e.caret, 2); e.right(false); assert_eq!(e.caret, 4);
        e.caret = 2; e.anchor = 2; e.delete(); assert_eq!(e.text(), b"abcd");
        e.undo(); assert_eq!(e.text(), b"ab\r\ncd"); e.undo(); assert_eq!(e.text(), b"abcd");
    }
    #[test] fn find_wraps() {
        let mut e = ed(b"One two ONE", false, 80);
        assert!(e.find_next(b"one")); assert_eq!(e.sel(), (0, 3));
        assert!(e.find_next(b"one")); assert_eq!(e.sel(), (8, 11));
        assert!(e.find_next(b"one")); assert_eq!(e.sel(), (0, 3));
    }
    #[test] fn vertical_keeps_col() {
        let mut e = ed(b"abcdef\r\nab\r\nabcdef", false, 80);
        e.caret = 5; e.anchor = 5; e.want_col = 5; e.vert(1, false); assert_eq!(e.caret, 10); e.vert(1, false); assert_eq!(e.caret, 17);
    }
}

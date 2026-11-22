//! B83 preloaded WAD: the IWAD is read into PSRAM once, in big sequential reads (FOR_DOOM: VFS small reads cost ~1.4 ms/sector); each
//! lump read is then a memcpy instead of fseek+fread on the SD card. Lumps are still copied into the zone as in C (`mapped` stays NULL):
//! zero-copy mapping changed 2 demo1 frames (vanilla tutti-frutti: a wall column reads past a short patch post, and the bytes after a
//! lump differ between a zone copy and the WAD). Falls back to stdio when the PSRAM heap cannot hold the file (Doom II 14.6 MB).
use crate::src::dg::w_file_stdc::{stdc_wad_file, stdc_wad_file_t, wad_file_t, FILE, PU_STATIC};
use core::ffi::{c_char, c_int, c_long, c_uint, c_void};

extern "C" {
    fn fopen(p: *const c_char, m: *const c_char) -> *mut FILE;
    fn fclose(f: *mut FILE) -> c_int;
    fn fread(b: *mut c_void, s: usize, n: usize, f: *mut FILE) -> usize;
    fn fseek(f: *mut FILE, o: c_long, w: c_int) -> c_int;
    fn ftell(f: *mut FILE) -> c_long;
    fn malloc(n: usize) -> *mut c_void;
    fn free(p: *mut c_void);
    fn Z_Malloc(size: c_int, tag: c_int, user: *mut c_void) -> *mut c_void;
    fn Z_Free(p: *mut c_void);
    fn printf(f: *const c_char, ...) -> c_int;
}

/// 0 = plain stdio path (A/B); set by the host before the engine starts
#[no_mangle]
pub static mut b83_wadmap: i32 = 0;   // board B s10: preloading 4 MB took 3.3 s at Doom start (SD ~1.3 MB/s) = slower launch than per-lump reads; off

unsafe extern "C" fn open(path: *mut c_char) -> *mut wad_file_t {
    let f = fopen(path, b"rb\0".as_ptr() as *const c_char);
    if f.is_null() {
        return core::ptr::null_mut();
    }
    fseek(f, 0, 2);
    let len = ftell(f) as usize;
    fseek(f, 0, 0);
    let mut buf = if b83_wadmap != 0 && PRE.is_null() { malloc(len) as *mut u8 } else { core::ptr::null_mut() };
    if !buf.is_null() {
        let mut got = 0usize;
        while got < len {
            let n = fread(buf.add(got) as *mut c_void, 1, (len - got).min(256 * 1024), f);
            if n == 0 {
                break;
            }
            got += n;
        }
        if got != len {
            free(buf as *mut c_void);
            buf = core::ptr::null_mut();
            fseek(f, 0, 0);
        }
    }
    let r = Z_Malloc(core::mem::size_of::<stdc_wad_file_t>() as c_int, PU_STATIC as c_int, core::ptr::null_mut()) as *mut stdc_wad_file_t;
    (*r).wad.file_class = &raw mut stdc_wad_file;
    (*r).wad.mapped = core::ptr::null_mut();
    (*r).wad.length = len as c_uint;
    if buf.is_null() {
        (*r).fstream = f;
    } else {
        fclose(f);
        (*r).fstream = buf as *mut FILE;   // preloaded: fstream holds the PSRAM copy (tagged by PRE)
        PRE = r;
    }
    printf(b"[b83] WAD %s: %u B %s\n\0".as_ptr() as *const c_char, path, len as c_uint,
        if buf.is_null() { b"stdio\0".as_ptr() } else { b"preloaded in PSRAM\0".as_ptr() });
    &raw mut (*r).wad
}

/// the one preloaded file (the IWAD); others (PWADs, demo lumps) use stdio
static mut PRE: *mut stdc_wad_file_t = core::ptr::null_mut();

unsafe extern "C" fn close(wad: *mut wad_file_t) {
    let s = wad as *mut stdc_wad_file_t;
    if s == PRE {
        free((*s).fstream as *mut c_void);
        PRE = core::ptr::null_mut();
    } else {
        fclose((*s).fstream);
    }
    Z_Free(s as *mut c_void);
}

unsafe extern "C" fn read(wad: *mut wad_file_t, offset: c_uint, buffer: *mut c_void, len: usize) -> usize {
    let s = wad as *mut stdc_wad_file_t;
    if s != PRE {
        fseek((*s).fstream, offset as c_long, 0);
        return fread(buffer, 1, len, (*s).fstream);
    }
    let n = len.min(((*s).wad.length as usize).saturating_sub(offset as usize));
    core::ptr::copy_nonoverlapping(((*s).fstream as *const u8).add(offset as usize), buffer as *mut u8, n);
    n
}

/// after b82_data_init's copies: the stdio WAD class gets these functions
pub unsafe fn install() {
    stdc_wad_file.OpenFile = Some(open);
    stdc_wad_file.CloseFile = Some(close);
    stdc_wad_file.Read = Some(read);
}

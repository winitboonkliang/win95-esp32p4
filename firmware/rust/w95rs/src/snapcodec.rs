//! B61 snapshot codec (own code, no_std, no alloc, no panics on any input):
//!  * CRC-32 (IEEE, slicing-by-4, tables built at compile time)
//!  * LZ4 block format compressor / decompressor (format = LZ4 block spec, written from the spec, not from lz4.c)
//! The decoder treats the input as hostile: every read and write is bounds-checked, an error returns -1 and the
//! caller falls back to a cold boot.
use core::ffi::c_int;

const fn make_tables() -> [[u32; 256]; 8] {
    let mut t = [[0u32; 256]; 8];
    let mut i = 0;
    while i < 256 {
        let mut c = i as u32;
        let mut k = 0;
        while k < 8 { c = if c & 1 != 0 { 0xEDB8_8320 ^ (c >> 1) } else { c >> 1 }; k += 1; }
        t[0][i] = c;
        i += 1;
    }
    let mut i = 0;
    while i < 256 {
        let mut s = 1;
        while s < 8 { t[s][i] = (t[s - 1][i] >> 8) ^ t[0][(t[s - 1][i] & 255) as usize]; s += 1; }
        i += 1;
    }
    t
}
/// 8 slicing tables; the asm CRC (esp/main/lz4_dec.S b61_crc_fast) reads this symbol, the Rust slicing-by-4 uses the first four.
#[no_mangle] pub static SNAP_CRC8: [[u32; 256]; 8] = make_tables();
use SNAP_CRC8 as T;

pub fn crc32(mut crc: u32, d: &[u8]) -> u32 {
    crc = !crc;
    let mut i = 0;
    while i + 4 <= d.len() {
        let w = crc ^ u32::from_le_bytes([d[i], d[i + 1], d[i + 2], d[i + 3]]);
        crc = T[3][(w & 255) as usize] ^ T[2][((w >> 8) & 255) as usize] ^ T[1][((w >> 16) & 255) as usize] ^ T[0][(w >> 24) as usize];
        i += 4;
    }
    while i < d.len() { crc = T[0][((crc ^ d[i] as u32) & 255) as usize] ^ (crc >> 8); i += 1; }
    !crc
}

/// worst-case compressed size for n input bytes
pub const fn bound(n: usize) -> usize { n + n / 255 + 32 }

#[inline(always)]
fn rd32(s: &[u8], i: usize) -> u32 { u32::from_le_bytes([s[i], s[i + 1], s[i + 2], s[i + 3]]) }

/// tab = 4096 u16 scratch (caller supplies it from PSRAM). src.len() <= 65536. Returns compressed length or -1.
pub fn compress(src: &[u8], dst: &mut [u8], tab: &mut [u16]) -> i32 {
    let n = src.len();
    if n > 65536 || tab.len() < 4096 || dst.len() < bound(n) { return -1; }
    for t in tab[..4096].iter_mut() { *t = 0xFFFF; }
    let mut op = 0usize;
    let mut anchor = 0usize;
    let mut ip = 0usize;
    if n >= 13 {
        let limit = n - 12; // a match may start only before this
        while ip < limit {
            let v = rd32(src, ip);
            let h = (v.wrapping_mul(2_654_435_761) >> 20) as usize;
            let r = tab[h] as usize;
            tab[h] = ip as u16;
            if r != 0xFFFF && rd32(src, r) == v {
                let mut m = 4;
                while ip + m < n - 5 && src[r + m] == src[ip + m] { m += 1; }
                // token
                let lit = ip - anchor;
                let tok = op; op += 1;
                let mut t = ((if lit >= 15 { 15 } else { lit }) as u8) << 4;
                if lit >= 15 { let mut l = lit - 15; while l >= 255 { dst[op] = 255; op += 1; l -= 255; } dst[op] = l as u8; op += 1; }
                dst[op..op + lit].copy_from_slice(&src[anchor..ip]); op += lit;
                let off = ip - r;
                dst[op] = off as u8; dst[op + 1] = (off >> 8) as u8; op += 2;
                let ml = m - 4;
                t |= (if ml >= 15 { 15 } else { ml }) as u8;
                if ml >= 15 { let mut l = ml - 15; while l >= 255 { dst[op] = 255; op += 1; l -= 255; } dst[op] = l as u8; op += 1; }
                dst[tok] = t;
                ip += m; anchor = ip;
            } else { ip += 1; }
        }
    }
    let lit = n - anchor;
    dst[op] = ((if lit >= 15 { 15 } else { lit }) as u8) << 4; op += 1;
    if lit >= 15 { let mut l = lit - 15; while l >= 255 { dst[op] = 255; op += 1; l -= 255; } dst[op] = l as u8; op += 1; }
    dst[op..op + lit].copy_from_slice(&src[anchor..n]); op += lit;
    op as i32
}

/// Returns decoded length (<= dst.len()) or -1 on any malformed input.
pub fn decompress(src: &[u8], dst: &mut [u8]) -> i32 {
    let (sn, dn) = (src.len(), dst.len());
    let (mut ip, mut op) = (0usize, 0usize);
    while ip < sn {
        let tok = src[ip]; ip += 1;
        let mut lit = (tok >> 4) as usize;
        if lit == 15 { loop { if ip >= sn { return -1; } let b = src[ip] as usize; ip += 1; lit += b; if b != 255 { break; } if lit > dn { return -1; } } }
        if lit > sn - ip || lit > dn - op { return -1; }
        dst[op..op + lit].copy_from_slice(&src[ip..ip + lit]); ip += lit; op += lit;
        if ip == sn { break; } // last sequence has no match
        if ip + 2 > sn { return -1; }
        let off = src[ip] as usize | (src[ip + 1] as usize) << 8; ip += 2;
        if off == 0 || off > op { return -1; }
        let mut ml = (tok & 15) as usize;
        if ml == 15 { loop { if ip >= sn { return -1; } let b = src[ip] as usize; ip += 1; ml += b; if b != 255 { break; } if ml > dn { return -1; } } }
        ml += 4;
        if ml > dn - op { return -1; }
        let from = op - off;
        if off >= ml { let (a, b) = dst.split_at_mut(op); b[..ml].copy_from_slice(&a[from..from + ml]); }
        else {   // overlapping match (period off): copy the already-written run, doubling it each pass, instead of byte by byte
            let (mut o, end) = (op, op + ml);
            while o < end { let n = core::cmp::min(o - from, end - o); dst.copy_within(from..from + n, o); o += n; }
        }
        op += ml;
    }
    op as i32
}

#[no_mangle] pub unsafe extern "C" fn snap_crc32(crc: u32, p: *const u8, n: usize) -> u32 { crc32(crc, core::slice::from_raw_parts(p, n)) }
#[no_mangle] pub unsafe extern "C" fn snap_lz4_compress(src: *const u8, n: usize, dst: *mut u8, cap: usize, tab: *mut u16) -> c_int {
    compress(core::slice::from_raw_parts(src, n), core::slice::from_raw_parts_mut(dst, cap), core::slice::from_raw_parts_mut(tab, 4096))
}
#[no_mangle] pub unsafe extern "C" fn snap_lz4_decompress(src: *const u8, n: usize, dst: *mut u8, cap: usize) -> c_int {
    decompress(core::slice::from_raw_parts(src, n), core::slice::from_raw_parts_mut(dst, cap))
}

/// Fast decoder: on the board it is the hand-written RV32 asm in esp/main/lz4_dec.S (same contract, verified against decompress() in dec/);
/// on other targets (PC build, tests) it is the Rust oracle itself.
#[cfg(not(target_arch = "riscv32"))]
#[no_mangle] pub unsafe extern "C" fn snap_lz4_decompress_fast(src: *const u8, n: usize, dst: *mut u8, cap: usize) -> c_int {
    decompress(core::slice::from_raw_parts(src, n), core::slice::from_raw_parts_mut(dst, cap))
}
#[cfg(not(target_arch = "riscv32"))]
#[no_mangle] pub unsafe extern "C" fn b61_crc_fast(crc: u32, p: *const u8, n: usize) -> u32 { crc32(crc, core::slice::from_raw_parts(p, n)) }
#[cfg(not(target_arch = "riscv32"))]
#[no_mangle] pub unsafe extern "C" fn snap_lz4_dec_pie(src: *const u8, n: usize, dst: *mut u8, cap: usize) -> c_int {
    decompress(core::slice::from_raw_parts(src, n), core::slice::from_raw_parts_mut(dst, cap))
}

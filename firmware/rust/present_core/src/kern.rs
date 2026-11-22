use crate::Rect;

/// Reference kernel (portable Rust). Panel pixel (px,py) = C[xmap[py]][yrev[px]];
/// consecutive rows with equal xmap are identical (row copy).
pub fn expand_rect_ref(cache: &[u16], h: usize, xmap: &[u16], yrev: &[u16], r: &Rect, dst: &mut [u16], stride: usize) {
    if r.is_empty() { return; }
    let (px0, px1) = (r.x0 as usize, r.x1 as usize);
    let mut last_sx = usize::MAX;
    let mut last_row = 0usize;
    for py in r.y0 as usize..r.y1 as usize {
        let sx = xmap[py] as usize;
        let row = py * stride;
        if sx == last_sx {
            dst.copy_within(last_row + px0..last_row + px1, row + px0);
        } else {
            let col = &cache[sx * h..sx * h + h];
            let d = &mut dst[row + px0..row + px1];
            for (o, &yi) in d.iter_mut().zip(&yrev[px0..px1]) { *o = col[yi as usize]; }
            last_sx = sx;
            last_row = row;
        }
    }
}

#[cfg(target_arch = "riscv32")]
core::arch::global_asm!(
    r#"
    .section .text.b20_kern,"ax",@progbits
    .balign 4
    .global b20_gather_row
    .type b20_gather_row,@function
# void b20_gather_row(u16 *dst /*a0*/, const u16 *yoff /*a1, byte offsets*/, const u16 *col /*a2*/, usize n /*a3*/)
b20_gather_row:
    beqz a3, 9f
    andi t0, a0, 2
    beqz t0, 1f
    lhu t1, 0(a1)
    add t1, t1, a2
    lhu t1, 0(t1)
    sh t1, 0(a0)
    addi a0, a0, 2
    addi a1, a1, 2
    addi a3, a3, -1
1:  srli a4, a3, 2
    beqz a4, 3f
2:  lhu t0, 0(a1)
    lhu t1, 2(a1)
    lhu t2, 4(a1)
    lhu t3, 6(a1)
    add t0, t0, a2
    add t1, t1, a2
    add t2, t2, a2
    add t3, t3, a2
    lhu t0, 0(t0)
    lhu t1, 0(t1)
    lhu t2, 0(t2)
    lhu t3, 0(t3)
    slli t1, t1, 16
    or t0, t0, t1
    slli t3, t3, 16
    or t2, t2, t3
    sw t0, 0(a0)
    sw t2, 4(a0)
    addi a0, a0, 8
    addi a1, a1, 8
    addi a4, a4, -1
    bnez a4, 2b
3:  andi a3, a3, 3
    beqz a3, 9f
4:  lhu t1, 0(a1)
    add t1, t1, a2
    lhu t1, 0(t1)
    sh t1, 0(a0)
    addi a0, a0, 2
    addi a1, a1, 2
    addi a3, a3, -1
    bnez a3, 4b
9:  ret
    .size b20_gather_row, .-b20_gather_row

    .balign 4
    .global b20_copy_row
    .type b20_copy_row,@function
# void b20_copy_row(u16 *dst /*a0*/, const u16 *src /*a1*/, usize n_halfwords /*a2*/)
# src and dst rows have the same alignment (same x offset, row stride multiple of 4)
b20_copy_row:
    beqz a2, 9f
    andi t0, a0, 2
    beqz t0, 1f
    lhu t1, 0(a1)
    sh t1, 0(a0)
    addi a0, a0, 2
    addi a1, a1, 2
    addi a2, a2, -1
1:  srli a3, a2, 3
    beqz a3, 3f
2:  lw t0, 0(a1)
    lw t1, 4(a1)
    lw t2, 8(a1)
    lw t3, 12(a1)
    sw t0, 0(a0)
    sw t1, 4(a0)
    sw t2, 8(a0)
    sw t3, 12(a0)
    addi a1, a1, 16
    addi a0, a0, 16
    addi a3, a3, -1
    bnez a3, 2b
3:  andi a2, a2, 7
    beqz a2, 9f
4:  lhu t1, 0(a1)
    sh t1, 0(a0)
    addi a0, a0, 2
    addi a1, a1, 2
    addi a2, a2, -1
    bnez a2, 4b
9:  ret
    .size b20_copy_row, .-b20_copy_row
"#
);

#[cfg(target_arch = "riscv32")]
extern "C" {
    fn b20_gather_row(dst: *mut u16, yoff: *const u16, col: *const u16, n: usize);
    fn b20_copy_row(dst: *mut u16, src: *const u16, n: usize);
}

/// Production kernel: hand-written RV32 (gather + row copy). yoff = yrev * 2 (byte offsets into a column).
#[cfg(target_arch = "riscv32")]
pub fn expand_rect(cache: &[u16], h: usize, xmap: &[u16], yoff: &[u16], r: &Rect, dst: &mut [u16], stride: usize) {
    if r.is_empty() { return; }
    let (px0, px1) = (r.x0 as usize, r.x1 as usize);
    let n = px1 - px0;
    assert!(r.y1 as usize * stride <= dst.len() && px1 <= stride);
    let mut last_sx = usize::MAX;
    let mut last_row = 0usize;
    for py in r.y0 as usize..r.y1 as usize {
        let sx = xmap[py] as usize;
        let row = py * stride;
        unsafe {
            if sx == last_sx {
                b20_copy_row(dst.as_mut_ptr().add(row + px0), dst.as_ptr().add(last_row + px0), n);
            } else {
                assert!(sx * h + h <= cache.len());
                b20_gather_row(dst.as_mut_ptr().add(row + px0), yoff.as_ptr().add(px0), cache.as_ptr().add(sx * h), n);
                last_sx = sx;
                last_row = row;
            }
        }
    }
}

/// Host build: the reference kernel (yoff is only used by the asm kernel; the host passes yrev).
#[cfg(not(target_arch = "riscv32"))]
pub fn expand_rect(cache: &[u16], h: usize, xmap: &[u16], yrev: &[u16], r: &Rect, dst: &mut [u16], stride: usize) {
    expand_rect_ref(cache, h, xmap, yrev, r, dst, stride)
}

/* GFX B29b: BitBlt / fill inner loops for the ESP32-P4 v1.0 (PIE, 8 x 16-bit pixels per q register).
 * Same CFG rule as eto_pie.S: unaligned q loads/stores via CFG mis_ld/mis_st (bits 0..1), set on entry (one instruction of delay)
 * and restored on exit (CFG is per core, not saved on a context switch).  PIE address operands: a0-a5 / t3-t6 only.
 * C references = gfx_*_c in gfxnat.c; gfx_selftest() compares both at first use (on FAIL the C loops are used, nat_rej[47] counts). */
    .text
    .align 2

/* void gfx_pie_fill16(uint16_t *o, int n, const uint16_t *v8)   o[i] = v8[i & 7]; v8 16-byte aligned (solid colour or an 8-px pattern
 * row already rotated to o's first pixel), o any 2-byte alignment */
    .global gfx_pie_fill16
    .type gfx_pie_fill16, @function
gfx_pie_fill16:
    esp.movx.r.cfg a5
    ori     a4, a5, 3
    esp.movx.w.cfg a4
    mv      t3, a2
    esp.vld.128.ip q0, t3, 0
    srli    t4, a1, 3
    andi    a1, a1, 7
    beqz    t4, 2f
1:  esp.vst.128.ip q0, a0, 16
    addi    t4, t4, -1
    bnez    t4, 1b
2:  beqz    a1, 4f
3:  lhu     t5, 0(a2)
    sh      t5, 0(a0)
    addi    a0, a0, 2
    addi    a2, a2, 2
    addi    a1, a1, -1
    bnez    a1, 3b
4:  esp.movx.w.cfg a5
    ret
    .size gfx_pie_fill16, .-gfx_pie_fill16

/* void gfx_pie_xor16(uint16_t *o, int n, const uint16_t *v8)   o[i] ^= v8[i & 7] (PATINVERT / DSTINVERT with v8 = 0xffff) */
    .global gfx_pie_xor16
    .type gfx_pie_xor16, @function
gfx_pie_xor16:
    esp.movx.r.cfg a5
    ori     a4, a5, 3
    esp.movx.w.cfg a4
    mv      t3, a2
    esp.vld.128.ip q1, t3, 0
    srli    t4, a1, 3
    andi    a1, a1, 7
    beqz    t4, 2f
1:  esp.vld.128.ip q0, a0, 0
    esp.xorq q0, q0, q1
    esp.vst.128.ip q0, a0, 16
    addi    t4, t4, -1
    bnez    t4, 1b
2:  beqz    a1, 4f
3:  lhu     t5, 0(a2)
    lhu     t6, 0(a0)
    xor     t5, t5, t6
    sh      t5, 0(a0)
    addi    a0, a0, 2
    addi    a2, a2, 2
    addi    a1, a1, -1
    bnez    a1, 3b
4:  esp.movx.w.cfg a5
    ret
    .size gfx_pie_xor16, .-gfx_pie_xor16

/* void gfx_pie_copy16(uint16_t *d, const uint16_t *s, int n)   forward copy, both any 2-byte alignment; d and s must not overlap
 * (overlapping screen-to-screen blits go through the row buffer) */
    .global gfx_pie_copy16
    .type gfx_pie_copy16, @function
gfx_pie_copy16:
    esp.movx.r.cfg a5
    ori     a4, a5, 3
    esp.movx.w.cfg a4
    srli    t4, a2, 3
    andi    a2, a2, 7
    beqz    t4, 2f
1:  esp.vld.128.ip q0, a1, 16
    esp.vst.128.ip q0, a0, 16
    addi    t4, t4, -1
    bnez    t4, 1b
2:  beqz    a2, 4f
3:  lhu     t5, 0(a1)
    sh      t5, 0(a0)
    addi    a0, a0, 2
    addi    a1, a1, 2
    addi    a2, a2, -1
    bnez    a2, 3b
4:  esp.movx.w.cfg a5
    ret
    .size gfx_pie_copy16, .-gfx_pie_copy16

/* void gfx_pie_and16(uint16_t *d, const uint16_t *s, int n)   d &= s (SRCAND, icon masks) */
    .global gfx_pie_and16
    .type gfx_pie_and16, @function
gfx_pie_and16:
    esp.movx.r.cfg a5
    ori     a4, a5, 3
    esp.movx.w.cfg a4
    srli    t4, a2, 3
    andi    a2, a2, 7
    beqz    t4, 2f
1:  esp.vld.128.ip q0, a1, 16
    esp.vld.128.ip q1, a0, 0
    esp.andq q1, q1, q0
    esp.vst.128.ip q1, a0, 16
    addi    t4, t4, -1
    bnez    t4, 1b
2:  beqz    a2, 4f
3:  lhu     t5, 0(a1)
    lhu     t6, 0(a0)
    and     t5, t5, t6
    sh      t5, 0(a0)
    addi    a0, a0, 2
    addi    a1, a1, 2
    addi    a2, a2, -1
    bnez    a2, 3b
4:  esp.movx.w.cfg a5
    ret
    .size gfx_pie_and16, .-gfx_pie_and16

/* void gfx_pie_xors16(uint16_t *d, const uint16_t *s, int n)   d ^= s (SRCINVERT) */
    .global gfx_pie_xors16
    .type gfx_pie_xors16, @function
gfx_pie_xors16:
    esp.movx.r.cfg a5
    ori     a4, a5, 3
    esp.movx.w.cfg a4
    srli    t4, a2, 3
    andi    a2, a2, 7
    beqz    t4, 2f
1:  esp.vld.128.ip q0, a1, 16
    esp.vld.128.ip q1, a0, 0
    esp.xorq q1, q1, q0
    esp.vst.128.ip q1, a0, 16
    addi    t4, t4, -1
    bnez    t4, 1b
2:  beqz    a2, 4f
3:  lhu     t5, 0(a1)
    lhu     t6, 0(a0)
    xor     t5, t5, t6
    sh      t5, 0(a0)
    addi    a0, a0, 2
    addi    a1, a1, 2
    addi    a2, a2, -1
    bnez    a2, 3b
4:  esp.movx.w.cfg a5
    ret
    .size gfx_pie_xors16, .-gfx_pie_xors16

/* void gfx_rgb24_565(uint16_t *o, const uint8_t *s, int n)   24 bpp BGR -> 5-6-5 by truncation (DIBENG's 24 bpp rule), RV32 scalar
 * (3 bytes -> 1 pixel has no PIE lane mapping; 4 pixels per round from 3 word loads when s is 4-byte aligned) */
    .global gfx_rgb24_565
    .type gfx_rgb24_565, @function
gfx_rgb24_565:
    andi    t0, a1, 3
    bnez    t0, 5f
    srli    t4, a2, 2
    andi    a2, a2, 3
    beqz    t4, 5f
    li      t6, 0xf800
1:  lw      t0, 0(a1)              /* b0 g0 r0 b1 */
    lw      t1, 4(a1)              /* g1 r1 b2 g2 */
    lw      t2, 8(a1)              /* r2 b3 g3 r3 */
    /* px0: b = t0[7:3], g = t0[15:10], r = t0[23:19] */
    srli    a3, t0, 3
    andi    a3, a3, 0x1f
    srli    a4, t0, 5
    andi    a4, a4, 0x7e0
    or      a3, a3, a4
    srli    a4, t0, 8
    and     a4, a4, t6
    or      a3, a3, a4
    sh      a3, 0(a0)
    /* px1: b = t0[31:27], g = t1[7:2], r = t1[15:11] */
    srli    a3, t0, 27
    andi    a4, t1, 0xfc
    slli    a4, a4, 3
    or      a3, a3, a4
    and     a4, t1, t6
    or      a3, a3, a4
    sh      a3, 2(a0)
    /* px2: b = t1[23:19], g = t1[31:26], r = t2[7:3] */
    srli    a3, t1, 19
    andi    a3, a3, 0x1f
    srli    a4, t1, 26
    slli    a4, a4, 5
    or      a3, a3, a4
    andi    a4, t2, 0xf8
    slli    a4, a4, 8
    or      a3, a3, a4
    sh      a3, 4(a0)
    /* px3: b = t2[15:11], g = t2[23:18], r = t2[31:27] */
    srli    a3, t2, 11
    andi    a3, a3, 0x1f
    srli    a4, t2, 13
    andi    a4, a4, 0x7e0
    or      a3, a3, a4
    srli    a4, t2, 16
    and     a4, a4, t6
    or      a3, a3, a4
    sh      a3, 6(a0)
    addi    a1, a1, 12
    addi    a0, a0, 8
    addi    t4, t4, -1
    bnez    t4, 1b
5:  beqz    a2, 9f
6:  lbu     t0, 0(a1)
    lbu     t1, 1(a1)
    lbu     t2, 2(a1)
    srli    t0, t0, 3
    srli    t1, t1, 2
    slli    t1, t1, 5
    srli    t2, t2, 3
    slli    t2, t2, 11
    or      t0, t0, t1
    or      t0, t0, t2
    sh      t0, 0(a0)
    addi    a1, a1, 3
    addi    a0, a0, 2
    addi    a2, a2, -1
    bnez    a2, 6b
9:  ret
    .size gfx_rgb24_565, .-gfx_rgb24_565

/* B61 snapshot decoder: LZ4 block format, hand-written RV32 asm for the ESP32-P4 (owner rule 46).
 * Oracle = rust/w95rs/src/snapcodec.rs decompress(); same contract: returns the decoded length or -1 on ANY malformed input,
 * never reads outside src[0..n) nor writes outside dst[0..cap) for the bytes that stay (see overshoot below).
 *
 * int snap_lz4_decompress_fast(const uint8_t *src (a0), size_t n (a1), uint8_t *dst (a2), size_t cap (a3))
 *
 * Speed: literal and match copies move 16 bytes per round (4 x lw/sw) and may OVERSHOOT up to 15 bytes past the copy end. That is exact because
 * (a) it is only done when dst has >= len+16 bytes left (so it never leaves the chunk: zero pages after a chunk stay zero) and src has
 * >= len+16 bytes left, (b) every later sequence rewrites the overshot bytes in order before any read can reach them (reads are < op).
 * Matches: off >= 16: 16 B steps; 8..15: 8 B steps; 4..7: 4 B steps (a step never reads a byte that is not written yet); off 1/2: pattern
 * splat; off 3 and every copy near the end of dst/src: exact byte loop.
 * Misaligned lw/sw are plain hardware accesses on the P4 (no trap), 1 cycle aligned, 2-3 misaligned (study/HW.md 2.3).
 * Registers (leaf, no stack): a0 ip, a1 iend, a2 op, a3 dend, a4 dbase, a5 token, t0 len, t1 off, t2..t6/a6/a7 temps.
 */
    .option norvc
    .text
    .align 2

    .macro LZ4DEC name, pie
    .global \name
    .type \name, @function
\name:
    .if \pie
    addi    sp, sp, -16             /* PIE CFG is per core: old value kept in s0 (saved on the stack), mis_ld|mis_st (bits 0,1) set, restored before every ret. */
    sw      s0, 0(sp)               /* CFG operands must be x8-x15 / x24-x31 */
    esp.movx.r.cfg s0
    ori     t3, s0, 3
    esp.movx.w.cfg t3
    .endif
    add     a1, a0, a1              /* iend */
    mv      a4, a2                  /* dbase */
    add     a3, a2, a3              /* dend */
    bgeu    a0, a1, .Ldone_\name    /* n == 0 -> 0 bytes */
/* FAST PATH (about 90 % of the sequences in a real snapshot: literal < 15, match code < 15, >= 24 src bytes and >= 48 dst bytes of slack): the literal is ONE
 * 16 B copy (no loop), the offset is one lhu, then it joins the general match code at .Lmlok. Anything else takes the exact general path below for that
 * sequence, which comes back here. */
.Lfast_\name:
    addi    t0, a1, -24
    bgeu    a0, t0, .Lseq_\name
    addi    t0, a3, -48
    bgeu    a2, t0, .Lseq_\name
    lbu     a5, 0(a0)
    srli    t0, a5, 4
    andi    t1, a5, 15
    li      t2, 15
    beq     t0, t2, .Lseq_\name
    beq     t1, t2, .Lseq_\name
    addi    a0, a0, 1
    .if \pie
    esp.vld.128.ip q0, a0, 0
    esp.vst.128.ip q0, a2, 0
    .else
    lw      t3, 0(a0)
    lw      t4, 4(a0)
    lw      t5, 8(a0)
    lw      t6, 12(a0)
    sw      t3, 0(a2)
    sw      t4, 4(a2)
    sw      t5, 8(a2)
    sw      t6, 12(a2)
    .endif
    add     a0, a0, t0
    add     a2, a2, t0
    lhu     t2, 0(a0)               /* offset (misaligned lhu is a plain access) */
    addi    a0, a0, 2
    sub     t3, a2, a4              /* bytes produced so far */
    addi    t4, t2, -1
    bgeu    t4, t3, .Lfail_\name    /* off == 0 or off > produced */
    mv      t0, t1                  /* match code (length - 4) */
    mv      t1, t2                  /* off */
    j       .Lmlok_\name
.Lseq_\name:
    lbu     a5, 0(a0)
    addi    a0, a0, 1
    srli    t0, a5, 4
    li      t1, 15
    bne     t0, t1, .Llitok_\name
    li      t3, 255
.Llitext_\name:
    bgeu    a0, a1, .Lfail_\name
    lbu     t2, 0(a0)
    addi    a0, a0, 1
    add     t0, t0, t2
    beq     t2, t3, .Llitext_\name
.Llitok_\name:
    sub     t1, a1, a0              /* src left */
    sub     t2, a3, a2              /* dst left */
    bltu    t1, t0, .Lfail_\name
    bltu    t2, t0, .Lfail_\name
    beqz    t0, .Llitdone_\name
    addi    t3, t0, 16
    bltu    t1, t3, .Llitslow_\name
    bltu    t2, t3, .Llitslow_\name
    add     t6, a2, t0              /* end of literal in dst */
    mv      t4, a0
    mv      t5, a2
.Llitf_\name:
    .if \pie
    esp.vld.128.ip q0, t4, 16
    esp.vst.128.ip q0, t5, 16
    .else
    lw      t1, 0(t4)
    lw      t2, 4(t4)
    lw      a6, 8(t4)
    lw      a7, 12(t4)
    sw      t1, 0(t5)
    sw      t2, 4(t5)
    sw      a6, 8(t5)
    sw      a7, 12(t5)
    addi    t4, t4, 16
    addi    t5, t5, 16
    .endif
    bltu    t5, t6, .Llitf_\name
    add     a0, a0, t0
    mv      a2, t6
    j       .Llitdone_\name
.Llitslow_\name:
    add     t6, a2, t0
1:  lbu     t1, 0(a0)
    addi    a0, a0, 1
    sb      t1, 0(a2)
    addi    a2, a2, 1
    bltu    a2, t6, 1b
.Llitdone_\name:
    beq     a0, a1, .Ldone_\name    /* last sequence has no match */
    addi    t1, a0, 2
    bltu    a1, t1, .Lfail_\name
    lbu     t1, 0(a0)
    lbu     t2, 1(a0)
    addi    a0, a0, 2
    slli    t2, t2, 8
    or      t1, t1, t2              /* off */
    beqz    t1, .Lfail_\name
    sub     t2, a2, a4
    bltu    t2, t1, .Lfail_\name    /* off > produced bytes */
    andi    t0, a5, 15
    li      t3, 15
    bne     t0, t3, .Lmlok_\name
    li      t3, 255
.Lmlext_\name:
    bgeu    a0, a1, .Lfail_\name
    lbu     t2, 0(a0)
    addi    a0, a0, 1
    add     t0, t0, t2
    beq     t2, t3, .Lmlext_\name
.Lmlok_\name:
    addi    t0, t0, 4               /* match length */
    sub     t2, a3, a2
    bltu    t2, t0, .Lfail_\name
    sub     t3, a2, t1              /* from */
    addi    t4, t0, 16
    bltu    t2, t4, .Lmslow_\name   /* no room for the overshoot */
    add     t6, a2, t0              /* end of match in dst */
    li      t5, 16
    bltu    t1, t5, .Lmsmall_\name
.Lm16_\name:
    .if \pie
    esp.vld.128.ip q0, t3, 16
    esp.vst.128.ip q0, a2, 16
    .else
    lw      t1, 0(t3)
    lw      t2, 4(t3)
    lw      a6, 8(t3)
    lw      a7, 12(t3)
    sw      t1, 0(a2)
    sw      t2, 4(a2)
    sw      a6, 8(a2)
    sw      a7, 12(a2)
    addi    t3, t3, 16
    addi    a2, a2, 16
    .endif
    bltu    a2, t6, .Lm16_\name
    mv      a2, t6
    bltu    a0, a1, .Lfast_\name
    j       .Ldone_\name
.Lmsmall_\name:
    li      t5, 8
    bltu    t1, t5, .Lmlt8_\name
.Lm8_\name:
    lw      t1, 0(t3)
    lw      t2, 4(t3)
    sw      t1, 0(a2)
    sw      t2, 4(a2)
    addi    t3, t3, 8
    addi    a2, a2, 8
    bltu    a2, t6, .Lm8_\name
    mv      a2, t6
    bltu    a0, a1, .Lfast_\name
    j       .Ldone_\name
.Lmlt8_\name:
    li      t5, 4
    bltu    t1, t5, .Lmlt4_\name
.Lm4_\name:
    lw      t1, 0(t3)
    sw      t1, 0(a2)
    addi    t3, t3, 4
    addi    a2, a2, 4
    bltu    a2, t6, .Lm4_\name
    mv      a2, t6
    bltu    a0, a1, .Lfast_\name
    j       .Ldone_\name
.Lmlt4_\name:                       /* off 1, 2, 3 */
    li      t5, 3
    beq     t1, t5, .Lmslow_\name
    lbu     t4, 0(t3)
    li      t5, 2
    beq     t1, t5, .Lm2_\name
    slli    t5, t4, 8               /* off 1: byte splat */
    or      t4, t4, t5
    slli    t5, t4, 16
    or      t4, t4, t5
    j       .Lsplat_\name
.Lm2_\name:
    lbu     t5, 1(t3)
    slli    t5, t5, 8
    or      t4, t4, t5
    slli    t5, t4, 16
    or      t4, t4, t5
.Lsplat_\name:
    sw      t4, 0(a2)
    addi    a2, a2, 4
    bltu    a2, t6, .Lsplat_\name
    mv      a2, t6
    bltu    a0, a1, .Lfast_\name
    j       .Ldone_\name
.Lmslow_\name:                      /* exact byte loop, forward (overlap-correct) */
    add     t6, a2, t0
1:  lbu     t1, 0(t3)
    addi    t3, t3, 1
    sb      t1, 0(a2)
    addi    a2, a2, 1
    bltu    a2, t6, 1b
    bltu    a0, a1, .Lfast_\name
.Ldone_\name:
    sub     a0, a2, a4
    .if \pie
    esp.movx.w.cfg s0
    lw      s0, 0(sp)
    addi    sp, sp, 16
    nop
    .endif
    ret
.Lfail_\name:
    li      a0, -1
    .if \pie
    esp.movx.w.cfg s0
    lw      s0, 0(sp)
    addi    sp, sp, 16
    nop
    .endif
    ret
    .size \name, .-\name
    .endm

    LZ4DEC snap_lz4_decompress_fast, 0
#ifndef NO_PIE
    LZ4DEC snap_lz4_dec_pie, 1
#endif

/* uint32_t b61_crc_fast(uint32_t crc (a0), const uint8_t *p (a1), size_t n (a2))   CRC-32 (IEEE, reflected, same contract as snap_crc32)
 * slicing-by-8 over the tables SNAP_CRC8[8][256] (generated at compile time by rust/w95rs/src/snapcodec.rs). Index = (byte << 2) via shift + mask;
 * the table bases are kept at T+2048 (a4: T0..T3 at -2048..+1024) and T+6144 (a5: T4..T7) so every lw offset fits the 12-bit immediate. */
    .global b61_crc_fast
    .type b61_crc_fast, @function
b61_crc_fast:
    not     a0, a0
    la      a3, SNAP_CRC8
    li      t0, 2048
    add     a4, a3, t0
    li      t0, 6144
    add     a5, a3, t0
    srli    a6, a2, 3
    beqz    a6, .Lcrc_tail
.Lcrc_l8:
    lw      t0, 0(a1)
    lw      t1, 4(a1)
    xor     t0, t0, a0
    andi    t2, t0, 255
    slli    t2, t2, 2
    add     t2, t2, a5
    lw      t2, 1024(t2)            /* T7[b0] */
    srli    t3, t0, 6
    andi    t3, t3, 1020
    add     t3, t3, a5
    lw      t3, 0(t3)               /* T6[b1] */
    srli    t4, t0, 14
    andi    t4, t4, 1020
    add     t4, t4, a5
    lw      t4, -1024(t4)           /* T5[b2] */
    srli    t5, t0, 22
    andi    t5, t5, 1020
    add     t5, t5, a5
    lw      t5, -2048(t5)           /* T4[b3] */
    xor     t2, t2, t3
    xor     t4, t4, t5
    xor     t2, t2, t4
    andi    t3, t1, 255
    slli    t3, t3, 2
    add     t3, t3, a4
    lw      t3, 1024(t3)            /* T3[c0] */
    srli    t4, t1, 6
    andi    t4, t4, 1020
    add     t4, t4, a4
    lw      t4, 0(t4)               /* T2[c1] */
    srli    t5, t1, 14
    andi    t5, t5, 1020
    add     t5, t5, a4
    lw      t5, -1024(t5)           /* T1[c2] */
    srli    t6, t1, 22
    andi    t6, t6, 1020
    add     t6, t6, a4
    lw      t6, -2048(t6)           /* T0[c3] */
    xor     t3, t3, t4
    xor     t5, t5, t6
    xor     t3, t3, t5
    xor     a0, t2, t3
    addi    a1, a1, 8
    addi    a6, a6, -1
    bnez    a6, .Lcrc_l8
.Lcrc_tail:
    andi    a2, a2, 7
    beqz    a2, .Lcrc_done
.Lcrc_t1:
    lbu     t0, 0(a1)
    xor     t0, t0, a0
    andi    t0, t0, 255
    slli    t0, t0, 2
    add     t0, t0, a4
    lw      t0, -2048(t0)           /* T0 */
    srli    a0, a0, 8
    xor     a0, a0, t0
    addi    a1, a1, 1
    addi    a2, a2, -1
    bnez    a2, .Lcrc_t1
.Lcrc_done:
    not     a0, a0
    ret
    .size b61_crc_fast, .-b61_crc_fast

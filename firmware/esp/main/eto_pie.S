/* RED B29b: native ExtTextOut inner loops for the ESP32-P4 v1.0 (PIE, 8 x 16-bit pixels per q register).
 * Unaligned q loads/stores use PIE CFG mis_ld/mis_st (bits 0..1, bit-exact on v1.0, METHODS_INDEX 14 Nov); CFG is per core and not saved on a
 * context switch, so every entry sets it, waits one instruction (the next ld/st still sees the old CFG) and restores it on exit.
 * PIE address operands: a0-a5 / t3-t6 only.  C reference = the PC versions in gfxnat.c; eto_selftest() compares both at first use. */
    .text
    .align 2

/* void eto_pie_or(uint16_t *m, const uint8_t *bits, int nbytes, const uint16_t *lut)
 * m[8k..8k+7] |= lut[bits[k]*8 ..] for k < nbytes; lut 16-byte aligned (256 x 8 x u16), m any 2-byte alignment */
    .global eto_pie_or
    .type eto_pie_or, @function
eto_pie_or:
    beqz    a2, 9f
    esp.movx.r.cfg a5
    ori     a4, a5, 3
    esp.movx.w.cfg a4
    nop
1:  lbu     t3, 0(a1)
    addi    a1, a1, 1
    slli    t3, t3, 4
    add     t3, t3, a3
    esp.vld.128.ip q1, t3, 0
    esp.vld.128.ip q0, a0, 0
    esp.orq q0, q0, q1
    esp.vst.128.ip q0, a0, 16
    addi    a2, a2, -1
    bnez    a2, 1b
    esp.movx.w.cfg a5
9:  ret
    .size eto_pie_or, .-eto_pie_or

/* void eto_pie_tr(uint16_t *o, const uint16_t *m, int n, const uint16_t *cv)   transparent: o ^= (o ^ tx) & m
 * cv 16-byte aligned {tx x 8, bk x 8}; m 16-byte aligned; o any 2-byte alignment; n pixels */
    .global eto_pie_tr
    .type eto_pie_tr, @function
eto_pie_tr:
    esp.movx.r.cfg a5
    ori     a4, a5, 3
    esp.movx.w.cfg a4
    lhu     t5, 0(a3)
    esp.vld.128.ip q2, a3, 0
    srli    t4, a2, 3
    andi    a2, a2, 7
    beqz    t4, 2f
1:  esp.vld.128.ip q0, a0, 0
    esp.vld.128.ip q1, a1, 16
    esp.xorq q3, q0, q2
    esp.andq q3, q3, q1
    esp.xorq q0, q0, q3
    esp.vst.128.ip q0, a0, 16
    addi    t4, t4, -1
    bnez    t4, 1b
2:  beqz    a2, 4f
3:  lhu     t3, 0(a1)
    beqz    t3, 5f
    sh      t5, 0(a0)
5:  addi    a0, a0, 2
    addi    a1, a1, 2
    addi    a2, a2, -1
    bnez    a2, 3b
4:  esp.movx.w.cfg a5
    ret
    .size eto_pie_tr, .-eto_pie_tr

/* void eto_pie_op(uint16_t *o, const uint16_t *m, int n, const uint16_t *cv)   opaque: o = bk ^ ((bk ^ tx) & m) */
    .global eto_pie_op
    .type eto_pie_op, @function
eto_pie_op:
    esp.movx.r.cfg a5
    ori     a4, a5, 3
    esp.movx.w.cfg a4
    lhu     t5, 0(a3)
    lhu     t6, 16(a3)
    esp.vld.128.ip q2, a3, 16
    esp.vld.128.ip q4, a3, 0
    esp.xorq q5, q2, q4
    srli    t4, a2, 3
    andi    a2, a2, 7
    beqz    t4, 2f
1:  esp.vld.128.ip q1, a1, 16
    esp.andq q3, q5, q1
    esp.xorq q0, q4, q3
    esp.vst.128.ip q0, a0, 16
    addi    t4, t4, -1
    bnez    t4, 1b
2:  beqz    a2, 4f
3:  lhu     t3, 0(a1)
    mv      a4, t6
    beqz    t3, 5f
    mv      a4, t5
5:  sh      a4, 0(a0)
    addi    a0, a0, 2
    addi    a1, a1, 2
    addi    a2, a2, -1
    bnez    a2, 3b
4:  esp.movx.w.cfg a5
    ret
    .size eto_pie_op, .-eto_pie_op

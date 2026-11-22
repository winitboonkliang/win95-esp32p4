/* B82 Doom: 8-bit palette frame -> RGB565, the per-frame CPU pass before the PPA (FOR_DOOM: the PPA has no palette lookup).
 * void b82_conv_asm(const uint8_t *src, uint16_t *dst, const uint16_t *lut, int n)   n = multiple of 8; src 4-aligned, dst 4-aligned.
 * 8 pixels per trip: 2 word loads, 8 LUT halfword loads (lut in internal RAM), 4 word stores (2 px each). Plain branch loop: no
 * esp.lp hardware loops (v1.x silicon bug, FOR_DOOM). Oracle = b82_conv_c in b82doom.c (bit-exact, checked at the first frame). */
	.text
	.align 2
	.global b82_conv_asm
	.type b82_conv_asm, @function
b82_conv_asm:
	srli a3, a3, 3
	beqz a3, 2f
1:	lw t0, 0(a0)
	lw t1, 4(a0)
	addi a0, a0, 8
	/* pixels 0,1 */
	andi a4, t0, 255
	slli a4, a4, 1
	add a4, a4, a2
	srli a5, t0, 7
	andi a5, a5, 510
	add a5, a5, a2
	lhu a4, 0(a4)
	lhu a5, 0(a5)
	/* pixels 2,3 */
	srli a6, t0, 15
	andi a6, a6, 510
	add a6, a6, a2
	srli a7, t0, 23
	andi a7, a7, 510
	add a7, a7, a2
	lhu a6, 0(a6)
	lhu a7, 0(a7)
	slli a5, a5, 16
	or a4, a4, a5
	slli a7, a7, 16
	or a6, a6, a7
	sw a4, 0(a1)
	sw a6, 4(a1)
	/* pixels 4,5 */
	andi a4, t1, 255
	slli a4, a4, 1
	add a4, a4, a2
	srli a5, t1, 7
	andi a5, a5, 510
	add a5, a5, a2
	lhu a4, 0(a4)
	lhu a5, 0(a5)
	/* pixels 6,7 */
	srli a6, t1, 15
	andi a6, a6, 510
	add a6, a6, a2
	srli a7, t1, 23
	andi a7, a7, 510
	add a7, a7, a2
	lhu a6, 0(a6)
	lhu a7, 0(a7)
	slli a5, a5, 16
	or a4, a4, a5
	slli a7, a7, 16
	or a6, a6, a7
	sw a4, 8(a1)
	sw a6, 12(a1)
	addi a1, a1, 16
	addi a3, a3, -1
	bnez a3, 1b
2:	ret
	.size b82_conv_asm, .-b82_conv_asm

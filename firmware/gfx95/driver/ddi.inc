# RED B29b: common entry of every exported DDI function.  Stub: "pushw $b_Name; movw $ordinal, %ax; jmp ddi_enter".
# 1. card index 0x50 <- ordinal: the host sees the call (arguments on SS:SP) and either EXECUTES it natively (status 1) or not (0).
# 2. status 1: the host has drawn; read arg byte count (0x52) and the DX:AX result (0x54 hi, 0x53 lo), drop the body offset and
#    the pascal arguments, return to the caller.  Guest work = this stub only.
# 3. status 0 (counted fallback): park the caller's far return in D_PSTACK (LIFO: interrupt-time calls nest), return through
#    ddi_ret (card index 0x51 = exit event for the per-export profile) and run the old body (DIBENG forward) with the caller's stack.
ddi_enter:
	pushw %bp
	movw %sp, %bp
	pushw %ax
	pushw %bx
	pushw %dx
	pushw %es
ddi_ask:
	movw $0xe8, %dx
	movw $0x5a, %ax                 # enter (index 0x5a = this stub understands status 2; an old driver uses 0x50 and never gets it)
	outw %ax, %dx
	movw $0xea, %dx
	movw -2(%bp), %ax
	outw %ax, %dx
	inw %dx, %ax
	testw %ax, %ax
	jz 7f
	cmpw $2, %ax
	jne ddi_native
	jmp ddi_touch
7:
	LOADSEG %ax
	movw %ax, %es
	movw %es:D_PSP, %bx
	cmpw $D_PSTACKN*4, %bx
	jae 8f
	movw 4(%bp), %ax
	movw %ax, %es:D_PSTACK(%bx)
	movw 6(%bp), %ax
	movw %ax, %es:D_PSTACK+2(%bx)
	addw $4, %bx
	movw %bx, %es:D_PSP
	movw $ddi_ret, 4(%bp)
	movw %cs, 6(%bp)
	jmp 9f
8:	movw $0xe8, %dx                 # D_PSTACK full: no exit event, tell the host to drop this entry
	movw $0x51, %ax
	outw %ax, %dx
	movw $0xea, %dx
	xorw %ax, %ax
	outw %ax, %dx
9:	popw %es
	popw %dx
	popw %bx
	popw %ax
	popw %bp
	ret

ddi_native:                         # executed by the host: return to the caller with its arguments removed
	movw $0xe8, %dx
	movw $0x52, %ax
	outw %ax, %dx
	movw $0xea, %dx
	inw %dx, %ax
	movw %ax, %cx                   # pascal argument bytes of this export
	movw %bp, %sp
	popw %bp
	popw %ax                        # body offset (unused)
	movw %sp, %bx
	addw %cx, %bx
	popw %ax                        # far return
	popw %dx
	movw %ax, %ss:(%bx)
	movw %dx, %ss:2(%bx)
	movw %bx, %sp
	movw $0xe8, %dx
	movw $0x54, %ax
	outw %ax, %dx
	movw $0xea, %dx
	inw %dx, %ax
	pushw %ax
	movw $0xe8, %dx
	movw $0x53, %ax
	outw %ax, %dx
	movw $0xea, %dx
	inw %dx, %ax
	popw %dx
	lret

ddi_ret:
	pushw %ax
	pushw %ax
	pushw %bp
	movw %sp, %bp
	pushw %ax
	pushw %bx
	pushw %dx
	pushw %es
	LOADSEG %ax
	movw %ax, %es
	movw %es:D_PSP, %bx
	movw %es:D_PSTACK-4(%bx), %ax   # read the entry before freeing it (an interrupt-time call may push right after)
	movw %ax, 2(%bp)
	movw %es:D_PSTACK-2(%bx), %ax
	movw %ax, 4(%bp)
	subw $4, %bx
	movw %bx, %es:D_PSP
	movw $0xe8, %dx
	movw $0x51, %ax
	outw %ax, %dx
	movw $0xea, %dx
	movw $1, %ax
	outw %ax, %dx
	popw %es
	popw %dx
	popw %bx
	popw %ax
	popw %bp
	lret

# GFX B29b: status 2 = the host would execute the call natively but a page it needs is not present.  Touch every listed page here in
# ring 3 (Windows pages it in: the same fault DIBENG would take) and ask again.  List entries via index 0x55: flags (0 = end, 1 = read,
# 3 = read + write back the same byte), 0x56 selector, 0x57/0x58 offset low/high.  The host gives up after a few rounds (status 0).
ddi_touch:
	pushl %esi
	pushw %fs
	pushw %cx
1:	movw $0xe8, %dx
	movw $0x55, %ax
	outw %ax, %dx
	movw $0xea, %dx
	inw %dx, %ax
	testw %ax, %ax
	jz 9f
	movw %ax, %cx
	movw $0xe8, %dx
	movw $0x56, %ax
	outw %ax, %dx
	movw $0xea, %dx
	inw %dx, %ax
	movw %ax, %fs
	movw $0xe8, %dx
	movw $0x58, %ax
	outw %ax, %dx
	movw $0xea, %dx
	inw %dx, %ax
	shll $16, %eax
	movw $0xe8, %dx
	pushl %eax
	movw $0x57, %ax
	outw %ax, %dx
	popl %eax
	movw $0xea, %dx
	inw %dx, %ax
	movl %eax, %esi
	movb %fs:(%esi), %al
	testw $2, %cx
	jz 1b
	movb %al, %fs:(%esi)
	jmp 1b
9:	popw %cx
	popw %fs
	popl %esi
	jmp ddi_ask

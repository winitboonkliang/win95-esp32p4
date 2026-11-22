# P4PTR.DRV - BLUE B45 own Windows 95 pointer driver (replaces MOUSE.DRV; 16-bit NE DLL in x86 asm, no imports).
# The host decides every gesture and queues finished records; this driver only hands them to USER's mouse event entry.
# Device (host side, see DESIGN.md):  IRQ 10 (INT 72h in the System VM) while records are queued
#   in  eax, 0xE4   pop next record: low word = USER flags (0 = queue empty), high word = x (0..65535, absolute)
#   in  ax,  0xE6   y of the record just popped
#   out 0xE4, ax    1 = enable (host sends pointer input here), 0 = disable, 2 = ISR finished (host measures guest cost)
# Exports keep the stock MOUSE.DRV ordinals/names; entries that need VMOUSE return what the stock driver returns without it.
	.code16
	.equ D_PROC,    0x00       # dword USER event proc (lpEventProc from Enable)
	.equ D_OLDVEC,  0x04       # dword previous PM vector of INT 72h (selector:offset as cx:dx)
	.equ D_HOOKED,  0x08       # word
	.equ D_INFO,    0x10       # 18-byte MOUSEINFO returned by Inquire
	.equ DATASIZE,  0x30
	.equ VEC,       0x72       # IRQ 10 = slave PIC base 70h + 2
	.equ PORT,      0xe4

	.macro FIXE n type kind a b
	.pushsection .fix,"a"
	.long 1b - \n
	.long \type
	.long \kind
	.long \a
	.long \b
	.popsection
	.endm
	.macro LOADDS              # ds := our data segment (internal segment 2)
	movw $0, %ax
1:
	FIXE 2 2 0 2 0
	movw %ax, %ds
	.endm

	.text
	.globl start
start:                          # LibMain: fill MOUSEINFO (exists, relative-capable, 2 buttons, rate 34, thresholds 2/2 = stock values)
	pushw %ds
	LOADDS
	movw $0x0101, D_INFO
	movw $2, D_INFO+2
	movw $0x22, D_INFO+4
	movw $2, D_INFO+6
	movw $2, D_INFO+8
	popw %ds
	movw $1, %ax
	lret

	.globl x_Inquire
x_Inquire:                      # WORD Inquire(LPMOUSEINFO) -> bytes copied (Win95 size 18)
	pushw %bp
	movw %sp, %bp
	pushw %ds
	pushw %si
	pushw %di
	LOADDS
	movw $D_INFO, %si
	lesw 6(%bp), %di
	cld
	movw $18, %cx
	rep movsb
	movw $18, %ax
	popw %di
	popw %si
	popw %ds
	popw %bp
	lret $4

	.globl x_Enable
x_Enable:                       # Enable(lpEventProc)
	pushw %bp
	movw %sp, %bp
	pushw %ds
	pushw %bx
	pushw %cx
	pushw %dx
	LOADDS
	cli
	movl 6(%bp), %eax
	movl %eax, D_PROC
	cmpw $0, D_HOOKED
	jne 1f
	movw $0x0204, %ax          # DPMI get PM vector -> cx:dx
	movb $VEC, %bl
	int $0x31
	movw %dx, D_OLDVEC
	movw %cx, D_OLDVEC+2
	movw %cs, %cx               # DPMI set PM vector: our fixed (page-locked) code segment
	movw $isr, %dx
	movw $0x0205, %ax
	movb $VEC, %bl
	int $0x31
	jc 1f
	movw $1, D_HOOKED
	inb $0xa1, %al              # unmask IRQ 10 (virtual PIC)
	andb $0xfb, %al
	outb %al, $0xa1
1:	movw $PORT, %dx
	movw $1, %ax
	outw %ax, %dx               # host: route pointer input to us (raises IRQ 10 if records wait)
	sti
	popw %dx
	popw %cx
	popw %bx
	popw %ds
	popw %bp
	lret $4

	.globl x_Disable
x_Disable:
	pushw %ds
	pushw %bx
	pushw %cx
	pushw %dx
	LOADDS
	movw $PORT, %dx
	xorw %ax, %ax
	outw %ax, %dx
	cmpw $0, D_HOOKED
	je 1f
	cli
	inb $0xa1, %al
	orb $0x04, %al
	outb %al, $0xa1
	movw D_OLDVEC, %dx
	movw D_OLDVEC+2, %cx
	movw $0x0205, %ax
	movb $VEC, %bl
	int $0x31
	movw $0, D_HOOKED
	sti
1:	popw %dx
	popw %cx
	popw %bx
	popw %ds
	lret

# IRQ 10: drain every queued record straight into USER's event entry, then one EOI pair.
isr:                            # IRQ 10 can land in 32-bit code (KERNEL32, VMM ring-3 thunks): save all 32-bit registers and every segment register
	pushw %ds
	pushw %es
	pushw %fs
	pushw %gs
	pushal
	LOADDS
2:	movw $PORT, %dx
	inl %dx, %eax
	testw %ax, %ax
	jz 3f
	movw %ax, %si              # flags
	shrl $16, %eax
	movw %ax, %bx              # x
	movw $PORT+2, %dx
	inw %dx, %ax
	movw %ax, %cx              # y
	movw %si, %ax
	movw $2, %dx               # number of buttons
	xorw %si, %si              # extra info = 0
	xorw %di, %di
	.ifndef NOCALL             # measurement build (-defsym NOCALL=1): drain only, USER not called
	lcall *D_PROC
	.endif
	LOADDS                     # USER may change ds
	jmp 2b
3:	movw $2, %ax
	outw %ax, %dx              # dx = PORT here: "ISR finished"
	movb $0x20, %al
	outb %al, $0xa0
	outb %al, $0x20
	popal
	popw %gs
	popw %fs
	popw %es
	popw %ds
	iret

	.globl x_GetIntVect
x_GetIntVect:                   # no mouse interrupt a DOS box could share
	movw $0xffff, %ax
	lret
	.globl x_GetSetData
x_GetSetData:                   # stock value without VMOUSE
	movw $0x900, %ax
	xorw %dx, %dx
	lret $8
	.globl x_CplApplet
x_CplApplet:
	xorw %ax, %ax
	xorw %dx, %dx
	lret $12
	.globl x_PowerEvent
x_PowerEvent:
	xorw %ax, %ax
	xorw %dx, %dx
	lret $6
	.globl x_ExtraPoints
x_ExtraPoints:
	movw $2, %ax
	lret $10
	.globl x_Wep
x_Wep:
	movw $1, %ax
	lret $2
	.globl x_Redetect
x_Redetect:
	movw $1, %ax
	lret

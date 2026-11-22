# B29GFX.DRV - BLUE B29a own Windows 95 display minidriver for the GFX95 paravirtual card (16-bit NE DLL, written in x86 asm).
# The DIBENGINE / GDIINFO contract was learned by reading FRAMEBUF.DRV and DIBENG.DLL (reference only).  See ../DESIGN.md.
# Assembled with GNU as (.code16); tools/mkne.py turns text + .fix records + layout into the NE file.
	.code16
	.include "layout.inc"

# fixup record in section .fix: site (offset in .text), src type (3 far ptr, 2 selector, 5 offset), kind (1 import, 0 internal segment),
# a = module index (import) or segment no (internal), b = ordinal (import) or offset (internal).  Local label 1: must follow the patched field.
	.macro FIXE n type kind a b
	.pushsection .fix,"a"
	.long 1b - \n
	.long \type
	.long \kind
	.long \a
	.long \b
	.popsection
	.endm
	.equ K, 1                  # KERNEL module reference index
	.equ D, 2                  # DIBENG module reference index

	.macro LCALL mod ord       # far call to an import (offset:selector patched by the loader)
	.byte 0x9a
	.long 0
1:
	FIXE 4 3 1 \mod \ord
	.endm
	.macro LJMP mod ord
	.byte 0xea
	.long 0
1:
	FIXE 4 3 1 \mod \ord
	.endm
	.macro LOADSEG reg         # reg := selector of our data segment (internal segment 2)
	movw $0, \reg
1:
	FIXE 2 2 0 2 0
	.endm
	.macro LOADDS
	LOADSEG %ax
	movw %ax, %ds
	.endm
	.macro FWDX ord            # forward to DIBENG *EXT: append lpDevice as the last pascal argument, same as the stock minidriver
	LOADSEG %ax
	movw %ax, %es
	popl %ecx
	pushl %es:D_DEV
	pushl %ecx
	LJMP D \ord
	.endm
	.macro OUTIDX idx          # card index port
	movw $0xe8, %dx
	movw $\idx, %ax
	outw %ax, %dx
	.endm
	.macro OUTDAT val
	movw $0xea, %dx
	movw $\val, %ax
	outw %ax, %dx
	.endm

	.macro DBG val             # debug marker to the card (index 0x3f), only in -DDBG builds
	.ifdef DBG
	pushw %ax
	pushw %dx
	OUTIDX 0x3f
	OUTDAT \val
	popw %dx
	popw %ax
	.endif
	.endm

	.text
	.globl start
start:                          # LibMain
	DBG 0xe000
	movw $1, %ax
	lret

# ---------------- 1:1 forwards to DIBENG (hook points for B29b live here) ----------------
	.globl x_BitBlt, x_ColorInfo, x_EnumDFonts, x_Output, x_Pixel, x_ScanLR, x_DeviceMode, x_ExtTextOut, x_GetCharWidth, x_DeviceBitmap
	.globl x_FastBorder, x_SetAttribute, x_CreateDIBitmap, x_DIBToDevice, x_StretchBlt, x_StretchDIBits, x_SelectBitmap, x_BitmapBits, x_Inquire, x_Control
b_BitBlt:
	DBG 0xd001
	LJMP D 1
b_ColorInfo:
	DBG 0xd002
	LJMP D 2
b_Control:
	DBG 0xd003
	LJMP D 3
b_EnumDFonts:
	DBG 0xd006
	LJMP D 6
b_Output:
	DBG 0xd008
	LJMP D 8
b_Pixel:
	DBG 0xd009
	LJMP D 9
b_ScanLR:
	DBG 0xd00c
	LJMP D 12
b_DeviceMode:
	DBG 0xd00d
	LJMP D 13
b_ExtTextOut:
	DBG 0xd00e
	jmp x_ExtTextOut_dmg
b_GetCharWidth:
	DBG 0xd00f
	LJMP D 15
b_DeviceBitmap:
	DBG 0xd010
	LJMP D 16
b_FastBorder:
	DBG 0xd011
	LJMP D 17
b_SetAttribute:
	DBG 0xd012
	LJMP D 18
b_CreateDIBitmap:LJMP D 20
b_DIBToDevice:
	DBG 0xd015
	LJMP D 21
b_StretchBlt:
	DBG 0xd01b
	LJMP D 27
b_StretchDIBits:
	DBG 0xd01c
	LJMP D 28
b_SelectBitmap:
	DBG 0xd01d
	LJMP D 29
b_BitmapBits:
	DBG 0xd01e
	LJMP D 30
b_Inquire:
	DBG 0xd065
	LJMP D 101

	.globl x_EnumObj, x_RealizeObject, x_GetPalette, x_SetPaletteTranslate, x_GetPaletteTranslate, x_UpdateColors, x_DIBBlt, x_StrBlt
b_EnumObj:
	DBG 0xd007
	FWDX 402
b_RealizeObject:
	DBG 0xd00a
	FWDX 400
b_GetPalette:
	DBG 0xd017
	FWDX 406
b_SetPaletteTranslate:
	DBG 0xd018
	FWDX 407
b_GetPaletteTranslate:
	DBG 0xd019
	FWDX 408
b_UpdateColors:
	DBG 0xd01a
	FWDX 404
b_DIBBlt:                       # one extra word (palettized flag = 0: true colour) below the return address
	popl %ecx
	pushw $0
	pushl %ecx
	LJMP D 401
b_StrBlt:                       # StrBlt = ExtTextOut with zeroed trailing arguments (as the stock driver)
	popl %ecx
	pushl $0
	pushl $0
	pushw $0
	pushl %ecx
	LJMP D 14

	.globl x_SetPalette
b_SetPalette:                   # DIBENG keeps the table; no DAC on this card; stock returns 0 after the call
	pushw %bp
	movw %sp, %bp
	pushw %ds
	LOADDS
	pushw 0xc(%bp)
	pushw 0xa(%bp)
	pushl 6(%bp)
	pushl D_DEV
	LCALL D 405
	xorw %ax, %ax
	cwd
	leaw -2(%bp), %sp
	popw %ds
	popw %bp
	lret $8

	.globl x_UserRepaintDisable
b_UserRepaintDisable:
	xorw %ax, %ax
	lret $2

# ---------------- pointer trio -------------------------------------------------------------------------------------------
# overlay mode (D_HWCUR != 0): the host draws and moves the pointer.  MoveCursor = ONE dword OUT (host also moves it from the input
# event itself, this OUT only reconciles); CheckCursor = nothing.  Software mode (colour / >32x32 shapes): DIBENG draws it as stock.
	.globl x_CheckCursor
b_CheckCursor:
	DBG 0xd068
	LOADSEG %ax
	movw %ax, %es
	cmpw $0, %es:D_ENABLED
	je 9f
	cmpw $0, %es:D_HWCUR
	jne 9f
	popl %ecx
	pushl %es:D_DEV
	pushl %ecx
	LJMP D 104
9:	lret

	.globl x_MoveCursor
b_MoveCursor:
	DBG 0xd066
	LOADSEG %ax
	movw %ax, %es
	cmpw $0, %es:D_HWCUR
	je 5f
	pushw %bp
	movw %sp, %bp
	movzwl 6(%bp), %ecx          # y   (pascal: the last argument is nearest the frame)
	movzwl 8(%bp), %eax          # x
	shll $16, %ecx
	orl %ecx, %eax
	movw $0xec, %dx
	outl %eax, %dx               # (y<<16)|x : the only guest work of a pointer move
	popw %bp
	lret $4
5:	popl %ecx
	pushl %es:D_DEV
	pushl %ecx
	LJMP D 103

	.globl x_SetCursor
b_SetCursor:
	pushw %bp
	movw %sp, %bp
	pushw %ds
	pushl %esi
	pushl %edi
	LOADDS
	cmpw $0, D_ENABLED
	je .Lsc_out
	cmpl $0, 6(%bp)
	je .Lsc_null
	.ifdef NOHW
	jmp .Lsc_sw                  # oracle build: DIBENG software pointer for every shape
	.endif
	lesw 6(%bp), %si
	movb %es:10(%si), %al        # Planes * BitsPixel must be 1 (mono AND/XOR) and size <= 32x32 to use the overlay
	mulb %es:11(%si)
	cmpb $1, %al
	jne .Lsc_sw
	cmpw $32, %es:4(%si)
	ja .Lsc_sw
	cmpw $32, %es:6(%si)
	ja .Lsc_sw
	cmpw $0, D_HWCUR             # leaving software mode: DIBENG must take its own pointer out of the surface first
	jne 1f
	pushl $0
	pushl D_DEV
	LCALL D 102
	lesw 6(%bp), %si
1:	movw $1, D_HWCUR
	OUTIDX 0x55                  # RED B29b: host may draw on the screen natively (no pointer pixels in VRAM)
	OUTDAT 0
# normalised shape in D_SHAPEBUF: hdr(hotx hoty w h), AND 32 rows x 4 bytes (pad 0xFF), XOR 32 rows x 4 bytes (pad 0)
	movw %es:0(%si), %ax
	movw %ax, D_SHAPEBUF
	movw %es:2(%si), %ax
	movw %ax, D_SHAPEBUF+2
	movw %es:4(%si), %ax
	movw %ax, D_SHAPEBUF+4
	movw %es:6(%si), %ax
	movw %ax, D_SHAPEBUF+6
	movw %es:8(%si), %dx
	movw %dx, D_CB
	cmpw $4, %dx
	jbe 2f
	movw $4, %dx
2:	movw %dx, D_NB
	movw %es:6(%si), %ax
	imulw D_CB, %ax              # h * cbWidth = offset of the XOR mask from the AND mask
	movw %ax, D_XOFF
	movw %es:6(%si), %ax
	cmpw $32, %ax
	jbe 3f
	movw $32, %ax
3:	movw %ax, D_ROWS
	pushw %es                    # fill AND with 1s, XOR with 0s (es temporarily = data)
	pushw %ds
	popw %es
	cld
	leaw D_SHAPEBUF+8, %di
	movl $0xffffffff, %eax
	movw $32, %cx
	rep stosl
	xorl %eax, %eax
	movw $32, %cx
	rep stosl
	popw %es
	leaw 12(%si), %bx            # src row pointer (AND mask)
	xorw %di, %di
.Lrow:
	movw D_NB, %cx
	pushw %bx
	pushw %di
.Lcol:
	movb %es:(%bx), %al
	movb %al, D_SHAPEBUF+8(%di)
	addw D_XOFF, %bx
	movb %es:(%bx), %al
	movb %al, D_SHAPEBUF+136(%di)
	subw D_XOFF, %bx
	incw %bx
	incw %di
	decw %cx
	jnz .Lcol
	popw %di
	popw %bx
	addw $4, %di
	addw D_CB, %bx
	decw D_ROWS
	jnz .Lrow
	OUTIDX 0x24                  # reset the shape stream, then 132 words
	OUTIDX 0x23
	movw $0xea, %dx
	leaw D_SHAPEBUF, %si
	movw $132, %cx
	cld
	rep outsw %ds:(%si), %dx
	OUTIDX 0x22
	OUTDAT 1                     # pointer visible
	jmp .Lsc_out
.Lsc_null:
	cmpw $0, D_HWCUR
	je .Lsc_sw
	OUTIDX 0x22
	OUTDAT 0
	jmp .Lsc_out
.Lsc_sw:                         # software pointer through DIBENG
	OUTIDX 0x55                  # RED B29b: DIBENG's pointer lives in VRAM: host must not draw on the screen
	OUTDAT 1
	cmpw $0, D_HWCUR
	je 4f
	movw $0, D_HWCUR
	OUTIDX 0x22
	OUTDAT 0
4:	pushl 6(%bp)
	pushl D_DEV
	LCALL D 102
.Lsc_out:
	popl %edi
	popl %esi
	leaw -2(%bp), %sp
	popw %ds
	popw %bp
	lret $4

# ---------------- Enable / Disable / ReEnable / ValidateMode ----------------------------------------------------------
# LFB selector: card says where the LFB is (PCI BAR0), DPMI 0800h maps it, KERNEL selector API builds the 2 MB data selector.
setup_lfb:                       # CF set on failure
	cmpw $0, D_LFBSEL
	jne .Lsl_ok
	OUTIDX 0
	movw $0xea, %dx
	inw %dx, %ax
	cmpw $0xb295, %ax
	jne .Lsl_fail
	OUTIDX 2
	movw $0xea, %dx
	inw %dx, %ax
	movw %ax, %cx                # low
	OUTIDX 3
	movw $0xea, %dx
	inw %dx, %ax
	movw %ax, %bx                # high
	movw $0x20, %si              # size 2 MB (SI:DI = 0x0020:0000)
	xorw %di, %di
	movw $0x0800, %ax
	int $0x31
	jc .Lsl_fail
	pushw %bx                    # linear address (BX:CX)
	pushw %cx
	pushw %ds
	LCALL K 175                  # AllocSelector(ds) -> ax
	popw %cx
	popw %bx
	orw %ax, %ax
	jz .Lsl_fail
	movw %ax, D_LFBSEL
	pushw %ax
	pushw %bx
	pushw %cx
	LCALL K 187                  # SetSelectorBase(sel, linear)
	pushw D_LFBSEL
	pushw $0x1f
	pushw $0xffff
	LCALL K 189                  # SetSelectorLimit(sel, 2 MB - 1)
.Lsl_ok:
	clc
	ret
.Lsl_fail:
	movw $0xdead, %ax
	OUTIDX 0x3f
	OUTDAT 0xdead
	stc
	ret

mode_on:                         # this driver's mode (W x H x 16) through the card: size registers first, then the enable bit (VBE LFB model on the PC build; DSI buffers on the board)
	OUTIDX 0x10
	OUTDAT W
	OUTIDX 0x11
	OUTDAT H
	OUTIDX 0x12
	OUTDAT 16
	OUTIDX 0x14
	OUTDAT 1
	ret

	.globl x_Enable
b_Enable:
	pushw %bp
	movw %sp, %bp
	pushw %ds
	pushl %esi
	pushl %edi
	LOADDS
	DBG 0xe001
	testw $1, 0x12(%bp)
	jnz .Lenq
	movl 0x14(%bp), %eax
	movl %eax, D_DEV
	cmpw $0, D_REEN
	jne 1f
	movw $0x4000, %ax            # tell the VDD the Windows VM goes to graphics (as the stock driver)
	int $0x2f
1:	DBG 0xe002
	call setup_lfb
	jc .Len_fail
	DBG 0xe003
	.ifdef DBG
	pushw %es
	movw D_LFBSEL, %ax
	movw %ax, %es
	xorw %di, %di
	movl $0x12345678, %es:(%di)
	movl $0x9abcdef0, %es:0x4000(%di)
	movl %es:(%di), %eax
	movw %ax, %dx
	popw %es
	pushw %ax
	pushw %dx
	OUTIDX 0x3f
	OUTDAT 0xe0f0
	popw %dx
	popw %ax
	.endif
	call mode_on
	pushl 0x14(%bp)
	pushw 0x12(%bp)
	pushl 0xe(%bp)
	pushl 0xa(%bp)
	pushl 6(%bp)
	LCALL D 5                    # DIB_Enable lays down its default device block
	lesw 0x14(%bp), %di
	movw $0x5250, %es:0(%di)     # deType 'PR'
	movw $W, %es:2(%di)
	movw $H, %es:4(%di)
	movw $PITCH, %es:6(%di)
	movw $0x1001, %es:8(%di)     # planes 1 | bits per pixel 16 << 8
	movl $0, %es:0xa(%di)
	movl $PITCH, %es:0xe(%di)
	movl $0, %es:0x12(%di)
	movl $0, %es:0x16(%di)
	movw D_LFBSEL, %ax
	movw %ax, %es:0x1a(%di)      # surface = our selector, offset 0, no banking
	movw $0x40, %es:0x1c(%di)    # deFlags: 0x40 = 5-6-5 (nonzero also marks the block as the screen device, not a memory bitmap)
	movw $0x400, %es:0x1e(%di)   # deVersion
	leaw 0x30(%di), %bx
	movw %bx, %es:0x20(%di)      # deBitmapInfo = far ptr to the header that follows the device block
	movw %es, %es:0x22(%di)
	movw $x_BeginAccess, %es:0x24(%di)   # BeginAccess far pointer = OUR wrapper (reports the damage rect, then DIB_BeginAccess)
	movw $0, %es:0x26(%di)
1:	FIXE 2 2 0 1 0               # selector of our code segment (internal segment 1)
	movw $x_EndAccess, %es:0x28(%di)     # EndAccess far pointer = OUR wrapper (tells the host the GDI call is finished, then DIB_EndAccess)
	movw $0, %es:0x2a(%di)
1:	FIXE 2 2 0 1 0               # selector of our code segment
	movl $0x28, %es:0(%bx)       # BITMAPINFOHEADER 16 bpp BI_RGB (565 is signalled by deFlags 0x40)
	movl $W, %es:4(%bx)
	movl $H, %es:8(%bx)
	movl $0x00100001, %es:0xc(%bx)
	movl $0, %es:0x10(%bx)
	movl $0, %es:0x14(%bx)
	movl $0, %es:0x18(%bx)
	movl $0, %es:0x1c(%bx)
	movl $0, %es:0x20(%bx)
	movl $0, %es:0x24(%bx)
	movw $1, D_ENABLED
	DBG 0xe004
	movw $1, %ax
	jmp .Len_out
.Lenq:                           # style 1: GDIINFO
	DBG 0xe005
	pushl 0x14(%bp)
	pushw 0x12(%bp)
	pushl 0xe(%bp)
	pushl 0xa(%bp)
	pushl 6(%bp)
	LCALL D 5
	lesw 0x14(%bp), %di
	leaw gdiinfo_tpl, %si
	movw $6, %cx
	rep movsl %cs:(%si), %es:(%di)
	addw $0x18, %di
	addw $0x18, %si
	movw $10, %cx
	rep movsl %cs:(%si), %es:(%di)
	movw 0x14(%bp), %di
	movw $96, %es:0x58(%di)      # LogPixelsX / Y
	movw $96, %es:0x5a(%di)
	movw $BPP, %es:0xc(%di)
	movw $4, %es:0x5c(%di)
	orw $0x2d80, %es:0x5e(%di)
	movw $0xffff, %es:0x10(%di)
	movw $0xffff, %es:0x12(%di)
	movw $0xffff, %es:0x18(%di)
	movw $0, %es:0x68(%di)
	movw $0, %es:0x6a(%di)
	movw $0, %es:0x6c(%di)
	orw $0x200, %es:0x26(%di)    # RC_DIBTODEV
	movw $0x58, %es:0x1a(%di)    # dpDEVICEsize = 0x30 device block + 0x28 header
	movw $0x6e, %ax
	jmp .Len_out
.Len_fail:
	xorw %ax, %ax
.Len_out:
	popl %edi
	popl %esi
	leaw -2(%bp), %sp
	popw %ds
	popw %bp
	lret $0x12

	.globl x_Disable
b_Disable:
	pushw %bp
	movw %sp, %bp
	pushw %ds
	pushl %esi
	pushl %edi
	LOADDS
	DBG 0xe006
	movw $0, D_ENABLED
	movw $0, D_HWCUR
	lesw 6(%bp), %si
	lock orw $0x10, %es:0x1c(%si)
	movw $0x4007, %ax
	int $0x2f
	OUTIDX 0x22
	OUTDAT 0
	OUTIDX 0x14
	OUTDAT 0
	movw $3, %ax                 # back to the VGA text path (SeaBIOS VGA BIOS) so the shutdown screen is shown
	int $0x10
	movw $1, %ax
	popl %edi
	popl %esi
	leaw -2(%bp), %sp
	popw %ds
	popw %bp
	lret $4

	.globl x_ReEnable
b_ReEnable:                      # ReEnable(lpDevice, lpGDIINFO): same mode again, device block and GDIINFO rebuilt
	pushw %bp
	movw %sp, %bp
	pushw %ds
	pushl %esi
	pushl %edi
	LOADDS
	movw $1, D_REEN
	pushl 0xa(%bp)               # BeginAccess(lpDevice, 0, 0, W-1, H-1, 8)
	pushl $0
	pushw $W-1
	pushw $H-1
	pushw $8
	LCALL D 105
	pushl 0xa(%bp)               # Enable(lpDevice, 0, 0, 0, 0)
	pushw $0
	pushl $0
	pushl $0
	pushl $0
	pushw %cs
	call x_Enable
	pushw %ax
	pushl 0xa(%bp)               # EndAccess(lpDevice, 8)
	pushw $8
	LCALL D 106
	popw %ax
	testw %ax, %ax
	jz 1f
	pushl 6(%bp)                 # Enable(style 1, lpGDIINFO)
	pushw $1
	pushl $0
	pushl $0
	pushl $0
	pushw %cs
	call x_Enable
	movw $1, %ax
1:	movw $0, D_REEN
	popl %edi
	popl %esi
	leaw -2(%bp), %sp
	popw %ds
	popw %bp
	lret $8

	.globl x_ValidateMode
b_ValidateMode:                  # three modes (1280x800, 1024x640, 640x400 at 16 bpp); 0 = valid, 2 = not supported
	DBG 0xe007
	pushw %bp
	movw %sp, %bp
	lesw 6(%bp), %bx
	movw $2, %ax
	cmpw $BPP, %es:2(%bx)
	jne 1f
	cmpw $1280, %es:4(%bx)
	jne 2f
	cmpw $800, %es:6(%bx)
	jne 1f
	jmp 3f
2:	cmpw $1024, %es:4(%bx)
	jne 4f
	cmpw $640, %es:6(%bx)
	jne 1f
	jmp 3f
4:	cmpw $640, %es:4(%bx)
	jne 1f
	cmpw $400, %es:6(%bx)
	jne 1f
3:	xorw %ax, %ax
1:	popw %bp
	lret $4

# BeginAccess(lpDevice, left, top, right, bottom, flags): DIBENG calls it through the device block before drawing on the screen surface.
# Damage = the rect (inclusive) -> card index 0x30, 4 words; then tail-jump to DIB_BeginAccess.
	.globl x_BeginAccess
x_BeginAccess:
	pushw %bp
	movw %sp, %bp
	pushw %ax
	pushw %dx
	movw $0xe8, %dx
	movw $0x30, %ax
	outw %ax, %dx
	movw $0xea, %dx
	movw 0xe(%bp), %ax
	outw %ax, %dx
	movw 0xc(%bp), %ax
	outw %ax, %dx
	movw 0xa(%bp), %ax
	outw %ax, %dx
	movw 8(%bp), %ax
	outw %ax, %dx
	popw %dx
	popw %ax
	popw %bp
	LJMP D 105

# EndAccess(lpDevice, flags): GDI call on the screen surface finished -> card index 0x31 (host releases the rects reported since BeginAccess), then DIB_EndAccess.
	.globl x_EndAccess
x_EndAccess:
	pushw %bp
	movw %sp, %bp
	pushw %ax
	pushw %dx
	movw $0xe8, %dx
	movw $0x31, %ax
	outw %ax, %dx
	movw $0xea, %dx
	xorw %ax, %ax
	outw %ax, %dx
	popw %dx
	popw %ax
	popw %bp
	LJMP D 106

# ExtTextOut(lpDestDev, x, y, lpClipRect, lpString, count, lpFontInfo, lpDrawMode, lpXform, lpDx, lpOpaqueRect, options) = 40 argument bytes.
# DIBENG draws text without calling BeginAccess (T3 check found the clock tile unreported), so report the damage here:
# the clip rect (exclusive right/bottom) or the whole screen when there is none.  Only for our screen device block (deType 'PR').
x_ExtTextOut_dmg:
	pushw %bp
	movw %sp, %bp
	pushw %ax
	pushw %dx
	pushw %bx
	pushw %es
	lesw 0x2a(%bp), %bx          # lpDestDev
	movw %es, %ax
	orw %bx, %ax
	jz 9f
	cmpw $0x5250, %es:(%bx)
	jne 9f
	movw $0xe8, %dx
	movw $0x30, %ax
	outw %ax, %dx
	movw $0xea, %dx
	cmpl $0, 0x22(%bp)           # lpClipRect null -> whole screen
	je 5f
	lesw 0x22(%bp), %bx
	movw %es:(%bx), %ax
	outw %ax, %dx                # left
	movw %es:2(%bx), %ax
	outw %ax, %dx                # top
	movw %es:4(%bx), %ax
	decw %ax
	outw %ax, %dx                # right - 1
	movw %es:6(%bx), %ax
	decw %ax
	outw %ax, %dx                # bottom - 1
	jmp 8f
5:	xorw %ax, %ax
	outw %ax, %dx
	outw %ax, %dx
	movw $W-1, %ax
	outw %ax, %dx
	movw $H-1, %ax
	outw %ax, %dx
8:	popw %es                      # our screen device: report at entry, draw, then tell the host the call is finished
	popw %bx
	popw %dx
	popw %ax
	.irp off,0x2c,0x2a,0x28,0x26,0x24,0x22,0x20,0x1e,0x1c,0x1a,0x18,0x16,0x14,0x12,0x10,0xe,0xc,0xa,8,6
	pushw \off(%bp)               # replay the 40 argument bytes for DIBENG's ExtTextOut
	.endr
	LCALL D 14
	pushw %ax
	pushw %dx
	movw $0xe8, %dx
	movw $0x31, %ax
	outw %ax, %dx
	movw $0xea, %dx
	xorw %ax, %ax
	outw %ax, %dx
	popw %dx
	popw %ax
	popw %bp
	lret $40
9:	popw %es
	popw %bx
	popw %dx
	popw %ax
	popw %bp
	LJMP D 14


	.include "ddi.inc"

# ---------------- RED B29b: host entry stubs for every export (ddi.inc) ----------------
	.globl x_BitBlt
x_BitBlt:
	pushw $b_BitBlt
	movw $1, %ax
	jmp ddi_enter
	.globl x_ColorInfo
x_ColorInfo:
	pushw $b_ColorInfo
	movw $2, %ax
	jmp ddi_enter
	.globl x_Control
x_Control:
	pushw $b_Control
	movw $3, %ax
	jmp ddi_enter
	.globl x_Disable
x_Disable:
	pushw $b_Disable
	movw $4, %ax
	jmp ddi_enter
	.globl x_Enable
x_Enable:
	pushw $b_Enable
	movw $5, %ax
	jmp ddi_enter
	.globl x_EnumDFonts
x_EnumDFonts:
	pushw $b_EnumDFonts
	movw $6, %ax
	jmp ddi_enter
	.globl x_EnumObj
x_EnumObj:
	pushw $b_EnumObj
	movw $7, %ax
	jmp ddi_enter
	.globl x_Output
x_Output:
	pushw $b_Output
	movw $8, %ax
	jmp ddi_enter
	.globl x_Pixel
x_Pixel:
	pushw $b_Pixel
	movw $9, %ax
	jmp ddi_enter
	.globl x_RealizeObject
x_RealizeObject:
	pushw $b_RealizeObject
	movw $10, %ax
	jmp ddi_enter
	.globl x_StrBlt
x_StrBlt:
	pushw $b_StrBlt
	movw $11, %ax
	jmp ddi_enter
	.globl x_ScanLR
x_ScanLR:
	pushw $b_ScanLR
	movw $12, %ax
	jmp ddi_enter
	.globl x_DeviceMode
x_DeviceMode:
	pushw $b_DeviceMode
	movw $13, %ax
	jmp ddi_enter
	.globl x_ExtTextOut
x_ExtTextOut:
	pushw $b_ExtTextOut
	movw $14, %ax
	jmp ddi_enter
	.globl x_GetCharWidth
x_GetCharWidth:
	pushw $b_GetCharWidth
	movw $15, %ax
	jmp ddi_enter
	.globl x_DeviceBitmap
x_DeviceBitmap:
	pushw $b_DeviceBitmap
	movw $16, %ax
	jmp ddi_enter
	.globl x_FastBorder
x_FastBorder:
	pushw $b_FastBorder
	movw $17, %ax
	jmp ddi_enter
	.globl x_SetAttribute
x_SetAttribute:
	pushw $b_SetAttribute
	movw $18, %ax
	jmp ddi_enter
	.globl x_DIBBlt
x_DIBBlt:
	pushw $b_DIBBlt
	movw $19, %ax
	jmp ddi_enter
	.globl x_CreateDIBitmap
x_CreateDIBitmap:
	pushw $b_CreateDIBitmap
	movw $20, %ax
	jmp ddi_enter
	.globl x_DIBToDevice
x_DIBToDevice:
	pushw $b_DIBToDevice
	movw $21, %ax
	jmp ddi_enter
	.globl x_SetPalette
x_SetPalette:
	pushw $b_SetPalette
	movw $22, %ax
	jmp ddi_enter
	.globl x_GetPalette
x_GetPalette:
	pushw $b_GetPalette
	movw $23, %ax
	jmp ddi_enter
	.globl x_SetPaletteTranslate
x_SetPaletteTranslate:
	pushw $b_SetPaletteTranslate
	movw $24, %ax
	jmp ddi_enter
	.globl x_GetPaletteTranslate
x_GetPaletteTranslate:
	pushw $b_GetPaletteTranslate
	movw $25, %ax
	jmp ddi_enter
	.globl x_UpdateColors
x_UpdateColors:
	pushw $b_UpdateColors
	movw $26, %ax
	jmp ddi_enter
	.globl x_StretchBlt
x_StretchBlt:
	pushw $b_StretchBlt
	movw $27, %ax
	jmp ddi_enter
	.globl x_StretchDIBits
x_StretchDIBits:
	pushw $b_StretchDIBits
	movw $28, %ax
	jmp ddi_enter
	.globl x_SelectBitmap
x_SelectBitmap:
	pushw $b_SelectBitmap
	movw $29, %ax
	jmp ddi_enter
	.globl x_BitmapBits
x_BitmapBits:
	pushw $b_BitmapBits
	movw $30, %ax
	jmp ddi_enter
	.globl x_ReEnable
x_ReEnable:
	pushw $b_ReEnable
	movw $31, %ax
	jmp ddi_enter
	.globl x_Inquire
x_Inquire:
	pushw $b_Inquire
	movw $101, %ax
	jmp ddi_enter
	.globl x_SetCursor
x_SetCursor:
	pushw $b_SetCursor
	movw $102, %ax
	jmp ddi_enter
	.globl x_MoveCursor
x_MoveCursor:
	pushw $b_MoveCursor
	movw $103, %ax
	jmp ddi_enter
	.globl x_CheckCursor
x_CheckCursor:
	pushw $b_CheckCursor
	movw $104, %ax
	jmp ddi_enter
	.globl x_UserRepaintDisable
x_UserRepaintDisable:
	pushw $b_UserRepaintDisable
	movw $500, %ax
	jmp ddi_enter
	.globl x_ValidateMode
x_ValidateMode:
	pushw $b_ValidateMode
	movw $700, %ax
	jmp ddi_enter

	.balign 4
gdiinfo_tpl:
	.include "gdiinfo.inc"

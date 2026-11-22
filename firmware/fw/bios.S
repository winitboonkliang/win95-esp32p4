# B60 BLUE - own system BIOS, 64 KB at F000:0000.  Hand written x86 real-mode asm (GNU as, AT&T, .code16).
# Guest part only: reset vector, POST, IVT/BDA, tick and dummy IRQ handlers, boot loader, trap stubs.
# Everything else is served by the Rust host (fwhost) through "out imm8,al" to ports 0xE0-0xEF:
#   E1 INT13h  E2 INT15h  E3 INT16h  E4 INT1Ah  E5 INT09h  E6 INT10h (stage 2b)
# Trap protocol: INT frame (IP,CS,FLAGS) is on the stack, the host edits registers + the stacked FLAGS, the stub irets.
        .code16
        .text
        .set CMOS_IDX, 0x70
        .set CMOS_DAT, 0x71

post:   cli
        cld
        xor  %ax, %ax
        mov  %ax, %ss
        mov  $0x7000, %sp
        mov  %ax, %ds
        mov  %ax, %es
        out  %al, $0xE0             # host builds the whole post-POST state (IVT, BDA, devices, boot sector at 0:7C00)
        mov  $0x7000, %sp
1:      xor  %ax, %ax
        mov  %ax, %ss
        mov  $0x7000, %sp
        mov  $0x0003, %ax           # 80x25 text mode (our own INT 10h)
        int  $0x10
        xor  %ax, %ax
        mov  %ax, %ds
        mov  %ax, %es
        mov  $0x7C00, %sp
        cmpw $0xAA55, 0x7DFE
        jne  int18
        cld
        mov  $0x80, %dl
        sti
        ljmp $0, $0x7C00

irq_master_eoi:
        push %ax ; mov $0x20, %al ; out %al, $0x20 ; pop %ax ; iret
irq_slave_eoi:
        push %ax ; mov $0x20, %al ; out %al, $0xA0 ; out %al, $0x20 ; pop %ax ; iret

int08:  push %ds ; push %ax
        mov  $0x40, %ax ; mov %ax, %ds
        incw 0x6C
        jnz  1f
        incw 0x6E
1:      cmpw $0x18, 0x6E
        jne  2f
        cmpw $0xB0, 0x6C
        jne  2f
        movw $0, 0x6E ; movw $0, 0x6C ; movb $1, 0x70
2:      sti
        int  $0x1C
        cli
        mov  $0x20, %al ; out %al, $0x20
        pop  %ax ; pop %ds
        iret

int70:  push %ax
        mov  $0x0C, %al ; out %al, $CMOS_IDX ; in $CMOS_DAT, %al
        mov  $0x20, %al ; out %al, $0xA0 ; out %al, $0x20
        pop  %ax ; iret
int75:  push %ax ; xor %al, %al ; out %al, $0xF0 ; mov $0x20, %al ; out %al, $0xA0 ; out %al, $0x20 ; pop %ax ; iret

int74:  push %ds ; push %ax ; push %bx ; push %cx ; push %dx
        xor  %ax, %ax ; mov %ax, %ds
        in   $0x64, %al
        test $0x20, %al
        jz   9f
        in   $0x60, %al
        mov  0x4F2, %bx              # bytes collected so far
        mov  %al, 0x4F4(%bx)
        inc  %bx
        cmp  $3, %bx
        jb   8f
        xor  %bx, %bx
        mov  %bx, 0x4F2
        cmpw $0, 0x4FA               # user handler segment
        je   9f
        xor  %ax, %ax
        mov  0x4F4, %bx              # status
        and  $0xFF, %bx
        push %bx
        mov  0x4F5, %bx ; and $0xFF, %bx ; push %bx   # dx
        mov  0x4F6, %bx ; and $0xFF, %bx ; push %bx   # dy
        push %ax                     # dz
        lcall *0x4F8
        add  $8, %sp
        jmp  9f
8:      mov  %bx, 0x4F2
9:      mov  $0x20, %al ; out %al, $0xA0 ; out %al, $0x20
        pop  %dx ; pop  %cx ; pop  %bx ; pop  %ax ; pop  %ds
        iret

int11:  push %ds ; mov $0x40, %ax ; mov %ax, %ds ; mov 0x10, %ax ; pop %ds ; iret
int12:  push %ds ; mov $0x40, %ax ; mov %ax, %ds ; mov 0x13, %ax ; pop %ds ; iret
int14:  mov  $0x8000, %ax ; iret           # no serial ports: timeout
int17:  xor  %ah, %ah ; iret
int18:  push %cs ; pop %ds ; mov $nobootmsg, %si
1:      lodsb
        test %al, %al
        jz   2f
        mov  $0x0E, %ah ; mov $7, %bx ; int $0x10
        jmp  1b
2:      xor  %ax, %ax ; int $0x16 ; int $0x19 ; iret

int19:  cli
        xor  %ax, %ax ; mov %ax, %ds ; mov %ax, %es ; mov %ax, %ss ; mov $0x7C00, %sp
        sti
        mov  $0x7C00, %bx
        mov  $0x0201, %ax ; mov $0x0001, %cx ; mov $0x0080, %dx
        int  $0x13
        jc   1f
        cmpw $0xAA55, 0x7DFE
        jne  1f
        mov  $0x80, %dl
        ljmp $0, $0x7C00
1:      int  $0x18

# ---- trap stubs
int15:  cmp  $0xC0, %ah
        je   1f
        out  %al, $0xE2 ; iret
1:      push %bp ; mov %sp, %bp ; andw $0xFFFE, 6(%bp) ; pop %bp   # CF=0 in the stacked flags
        push %cs ; pop %es ; mov $sysconf, %bx ; xor %ah, %ah ; iret
int1a:  out  %al, $0xE4 ; iret
int09:  out  %al, $0xE5 ; iret
int16:  sti                             # host sets live CF = "no key yet" for blocking reads
1:      out  %al, $0xE3
        jnc  2f
        hlt
        jmp  1b
2:      iret

nobootmsg: .asciz "No bootable disk.\r\n"

# 1024 cyl, 16 heads, 63 sectors (legacy INT 41h table; real geometry is served by INT 13h/08)
hd0_table:
        .word 1024 ; .byte 16 ; .word 0 ; .word 0xFFFF ; .byte 0 ; .byte 0xC8 ; .byte 0,0,0 ; .word 1024 ; .byte 63 ; .byte 0
diskette_table:
        .byte 0xAF,0x02,0x25,0x02,0x12,0x1B,0xFF,0x6C,0xF6,0x0F,0x08


        .org 0x2000
font16: .incbin "font16.bin"
font8:  .incbin "font8.bin"

# ---- PnP BIOS 1.0 installation structure (devmgr fix: Device Manager "Plug and Play BIOS" Code 24).  Win95 was installed under SeaBIOS
# and keeps a root devnode Enum\Root\*PNP0C00 (PnPStrucOffset 7DD0) whose driver BIOS.VXD looks for the "$PnP" header at every boot and
# calls function 00h; with no header (or a failing call) the device is "not present" (Code 24).  Same offset as SeaBIOS, paragraph aligned.
        .org 0x7DD0
pnp_hdr: .ascii "$PnP"
        .byte 0x10, 0x21                # version 1.0, length 33
        .word 0                         # control: no event notification
        .byte 0                         # checksum: set by build_bios.sh (sum of the 33 bytes = 0)
        .long 0                         # event notification flag address
        .word pnp_real, 0xF000          # real-mode entry
        .word pnp_prot                  # 16-bit protected-mode entry offset ...
        .long 0xF0000                   # ... and its code segment base
        .long 0                         # OEM device id
        .word 0xF000                    # real-mode data segment
        .long 0xF0000                   # protected-mode data segment base
# Entry (PnP BIOS 1.0a sec. 4.4): far call, C-style word args on the caller's stack.  Function 00h Get Number of System Device Nodes is the
# only one BIOS.VXD needs (it declares the BIOS absent, Code 24, when it fails): 0 nodes.  Everything else: 82h FUNCTION_NOT_SUPPORTED.
pnp_real:
        movzwl %sp, %esp                # real mode: the stack is 16-bit, ESP high bits may be junk
pnp_prot:                               # 16-bit PM (BIOS.VXD, selector base F0000): use the full ESP like the caller set it
        push %ebp
        mov  %esp, %ebp
        push %es
        push %bx
        mov  $0x82, %ax
        cmpw $0, 8(%ebp)                # Function
        jne  1f
        les  10(%ebp), %bx              # BYTE far *NumNodes
        movb $0, %es:(%bx)
        les  14(%ebp), %bx              # WORD far *NodeSize
        movw $0, %es:(%bx)
        xor  %ax, %ax                   # SUCCESS
1:      pop  %bx
        pop  %es
        pop  %ebp
        lret

        .balign 16
        .org 0xE000
bios32_dir:                         # PCI BIOS: BIOS32 service directory ("_32_"), found by scanning E0000-FFFFF
        .ascii "_32_"
        .long  0xF0000 + bios32_entry
        .byte  0, 1, 0, 0,0,0,0,0
        .code32
bios32_entry:                       # EAX = service id; only "$PCI" is offered
        cmp  $0x49435024, %eax
        jne  1f
        mov  $0xF0000, %ebx
        mov  $0x10000, %ecx
        mov  $pci32_entry, %edx
        xor  %al, %al
        lret
1:      mov  $0x80, %al
        lret
pci32_entry:                        # AH=B1h AL=function, same as INT 1Ah; the host sets the live flags
        out  %al, $0xE6
        lret
apm32_entry:                        # APM 32-bit protected-mode entry (far call, AX=53xx): CPU idle halts, the rest succeeds
        cmp  $0x5305, %ax
        jne  1f
        sti
        hlt
        xor  %ah, %ah
        clc
        lret
1:      cmp  $0x5307, %ax            # set power state: host decides (CX=3 off = Windows Shut Down)
        jne  3f
        out  %al, $0xE8
        lret
3:      cmp  $0x530B, %ax            # get PM event: none pending
        jne  2f
        mov  $0x80, %ah
        stc
        lret
2:      xor  %ah, %ah
        clc
        lret
        .code16

        .org 0xE3FE
int13:  out  %al, $0xE1
        push %ds ; push %ax
        xor  %ax, %ax ; mov %ax, %ds
        cmpb $0, 0x4E8                  # host asks for a "shadow" PIO read (1-sector reads seen from V86): Win95's
        je   9f                         # ESDI_506 traps the IDE ports to prove INT 13h really drives the controller;
        movb $0, 0x4E8                  # without traffic it logs "ESDI trap handler not called" and drops C: to
        push %cx ; push %dx             # MS-DOS compatibility mode
        mov  $0x1F2, %dx ; mov $1, %al ; out %al, %dx
        inc  %dx ; mov 0x4EC, %al ; out %al, %dx
        inc  %dx ; mov 0x4ED, %al ; out %al, %dx
        inc  %dx ; mov 0x4EE, %al ; out %al, %dx
        inc  %dx ; mov 0x4EF, %al ; and $0x0F, %al ; or $0xE0, %al ; out %al, %dx
        inc  %dx ; mov $0x20, %al ; out %al, %dx
        mov  $0x1F0, %dx ; mov $256, %cx
8:      in   %dx, %ax
        loop 8b
        mov  $0x1F7, %dx ; in %dx, %al      # status read acks IRQ14 like a real BIOS (judge R4 fix: else ESDI times out and retries)
        pop  %dx ; pop %cx
9:      pop  %ax ; pop %ds
        iret

        .org 0xE6F5
sysconf:
        .word 8 ; .byte 0xFC, 0x00, 0x01, 0x74, 0x40, 0x00   # model FC/00 rev 1, features as the reference BIOS reports them
        .org 0xF065                     # INT 10h: host-served; AX=1130h (font pointer) also needs ES=F000
int10:  cmp  $0x1130, %ax
        je   1f
        out  %al, $0xE7
        iret
1:      out  %al, $0xE7
        push %cs ; pop %es
        iret

oem:    .asciz "WIN95_P4 VGA BIOS (own)"

        .org 0xFF53
dummy_iret:
        iret
        .org 0xFFF0
        ljmp $0xF000, $post
        .ascii "06/23/99"             # date as the reference BIOS shows it: HIMEM treats a year below 93 as an old, quirky machine
        .byte 0, 0xFC, 0

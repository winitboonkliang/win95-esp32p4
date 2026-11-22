# P4IDLE.VXD - own idle VxD for Windows 95 on the P4 machine (replaces *VPOWERD + the APM BIOS round trip).
# Registers one VMM idle callback (Call_When_Idle = VMM service 0x003A (0x012B is the cancel service), found by tracing VPOWERD in a RAM snapshot).
# VMM calls the callback when the system is idle; carry clear = "idle handled, stop the list" (VMM @c000a793).
# The callback does STI;HLT directly at ring 0: the host emulator sleeps until the next IRQ (deadline HLT, B40).
        .intel_syntax noprefix
        .text
        .globl _start
#ifdef P4TDBG
        .macro DBG v                    # debug byte out of the guest: port 0x402 (emulator copy prints it)
        push edx
        push eax
        mov  edx, 0x402
        mov  al, \v
        out  dx, al
        pop  eax
        pop  edx
        .endm
#else
        .macro DBG v
        .endm
#endif
_start:
ddb:    .long 0                 # 00 DDB_Next
        .word 0x0400            # 04 SDK version
        .word 0                 # 06 device number (undefined: nobody calls an API of ours)
        .byte 1, 0              # 08 major, minor
        .word 0                 # 0A flags
        .ascii "P4IDLE  "       # 0C name
        .long 0x80000000        # 14 init order (undefined)
        .long ctrl              # 18 control proc  (fixup in the LE builder)
        .long 0, 0, 0, 0, 0     # 1C v86 api, 20 pm api, 24 v86 csip, 28 pm csip, 2C ref data
        .long 0, 0, 0           # 30 service table, 34 count, 38 win32 service table
        .ascii "verP"           # 3C DDB_Prev
        .long 0x50              # 40 DDB_Size
        .ascii "1vsR2vsR3vsR"   # 44.. reserved
ctrl:
#ifdef DEBUG_LOG
        pushad                  # debug: ring of the first 64 control messages, read from a RAM dump (log, idx at DDB+0x2C)
        call 5f
5:      pop  ebp
        sub  ebp, offset 5b - ddb
        lea  edx, [eax-0x1D]    # skip the very frequent VM messages 1Dh..21h
        cmp  edx, 4
        jbe  6f
        mov  ecx, [ebp+0x2C]
        cmp  ecx, 64
        jae  6f
        lea  edx, [ebp+offset logbuf - ddb]
        mov  [edx+ecx*4], eax
        inc  ecx
        mov  [ebp+0x2C], ecx
6:      popad
#endif
        cmp  eax, 1             # Device_Init
        jne  9f
        call 1f                 # position independent: esi = address of idle_cb
1:      pop  esi
        add  esi, offset idle_cb - 1b
        int  0x20               # VMMcall Call_When_Idle
        .word 0x003A, 0x0001
9:      clc
        ret
idle_cb:
#ifdef P4TIME
        pushad
        call 1f                          # ebp = load address
1:      pop  ebp
        sub  ebp, offset 1b - ddb
        cmp  byte ptr [ebp + pending - ddb], 0
        jne  2f
        dec  byte ptr [ebp + pollcnt - ddb]   # poll the host port every 8th idle call (~9 Hz)
        jnz  3f
        mov  byte ptr [ebp + pollcnt - ddb], 8
        in   al, 0xEA                    # host: 0xA5 = "resumed, set the clock from the RTC" (read clears it)
        cmp  al, 0xA5
        jne  3f
        mov  byte ptr [ebp + pending - ddb], 1
        DBG 0x50
2:      call resync                      # CF set = not possible now (not the System VM), retry at the next idle call
        jc   3f
        call 4f
4:      pop  ebp
        sub  ebp, offset 4b - ddb
        mov  byte ptr [ebp + pending - ddb], 0
        inc  dword ptr [ebp + done - ddb]
3:      popad
#endif
        sti
        hlt
        clc
        ret
#ifdef P4TIME
# resync: read the BIOS RTC (INT 1Ah AH=02/04, BCD) and set the DOS clock (INT 21h AH=2Bh/2Dh) in the System VM with a nested V86 exec.
# DOS then keeps date/time, the BDA tick count and the RTC; Windows 95 reads time and file stamps through it.
        .macro VMMCALL svc
        int  0x20
        .word \svc, 0x0001
        .endm
bcd:    mov  edx, eax                    # eax = BCD byte -> binary
        shr  eax, 4
        lea  eax, [eax + eax*4]
        add  eax, eax
        and  edx, 15
        add  eax, edx
        ret
resync: pushad
        DBG 0x41
        VMMCALL 0x0001                   # Get_Cur_VM_Handle -> ebx
        mov  esi, ebx
        VMMCALL 0x0003                   # Get_Sys_VM_Handle -> ebx
        cmp  esi, ebx
        jne  8f
        call 1f
1:      pop  ebp
        sub  ebp, offset 1b - ddb
        mov  [ebp + vmh - ddb], ebx
        lea  edi, [ebp + sbuf - ddb]
        VMMCALL 0x008D                   # Save_Client_State (edi = buffer)
        mov  ebx, [ebp + vmh - ddb]
        VMMCALL 0x0082                   # Begin_Nest_V86_Exec
        mov  ebp, [ebx + 8]              # Client_Reg_Struc
        mov  dword ptr [ebp + 0x1C], 0x0200      # INT 1Ah AH=02 read RTC time: CH hour, CL min, DH sec (BCD)
        mov  eax, 0x1A
        VMMCALL 0x0084                   # Exec_Int
        DBG [ebp + 0x19]
        DBG [ebp + 0x18]
        DBG [ebp + 0x15]
        DBG [ebp + 0x2C]
        movzx eax, byte ptr [ebp + 0x19]
        call bcd
        push eax                         # hour
        movzx eax, byte ptr [ebp + 0x18]
        call bcd
        push eax                         # minute
        movzx eax, byte ptr [ebp + 0x15]
        call bcd
        push eax                         # second
        mov  dword ptr [ebp + 0x1C], 0x0400      # INT 1Ah AH=04 read RTC date: CH century, CL year, DH month, DL day (BCD)
        mov  eax, 0x1A
        VMMCALL 0x0084
        DBG [ebp + 0x19]
        DBG [ebp + 0x18]
        DBG [ebp + 0x15]
        DBG [ebp + 0x14]
        movzx eax, byte ptr [ebp + 0x19]
        call bcd
        imul eax, eax, 100
        mov  ecx, eax
        movzx eax, byte ptr [ebp + 0x18]
        call bcd
        add  ecx, eax
        push ecx                         # year
        movzx eax, byte ptr [ebp + 0x15]
        call bcd
        push eax                         # month
        movzx eax, byte ptr [ebp + 0x14]
        call bcd
        push eax                         # day
        pop  eax
        mov  [ebp + 0x14], al            # DL = day
        pop  eax
        mov  [ebp + 0x15], al            # DH = month
        pop  eax
        mov  [ebp + 0x18], ax            # CX = year
        mov  dword ptr [ebp + 0x1C], 0x2B00      # INT 21h AH=2Bh set date
        mov  eax, 0x21
        VMMCALL 0x0084
        DBG [ebp + 0x1C]
        pop  eax
        mov  byte ptr [ebp + 0x14], 0    # DL = 1/100 s
        mov  [ebp + 0x15], al            # DH = second
        pop  eax
        mov  [ebp + 0x18], al            # CL = minute
        pop  eax
        mov  [ebp + 0x19], al            # CH = hour
        mov  dword ptr [ebp + 0x1C], 0x2D00      # INT 21h AH=2Dh set time
        mov  eax, 0x21
        VMMCALL 0x0084
        DBG [ebp + 0x1C]
        call 2f
2:      pop  eax
        sub  eax, offset 2b - ddb        # eax = base
        mov  ebx, [eax + vmh - ddb]
        lea  esi, [eax + sbuf - ddb]
        VMMCALL 0x0086                   # End_Nest_Exec
        VMMCALL 0x008E                   # Restore_Client_State (esi = buffer)
        # Explorer's tray clock only re-reads the time at its own minute timer; tell every window the time changed (what SetLocalTime does):
        # _SHELL_PostMessage(HWND_BROADCAST 0xFFFF, WM_TIMECHANGE 0x1E, 0, 0, 0, 0)  (SHELL = device 0x17, service 6, cdecl)
        push 0
        push 0
        push 0
        push 0
        push 0x1E
        push 0xFFFF
        int  0x20
        .word 0x0006, 0x0017
        add  esp, 24
        DBG  al
        DBG 0x45
        popad
        clc
        ret
8:      DBG 0x4E
9:      popad
        stc
        ret
pending: .byte 0
pollcnt: .byte 1
        .align 4
done:   .long 0
vmh:    .long 0
sbuf:   .fill 0x6C, 1, 0
#endif
#ifdef DEBUG_LOG
logbuf: .fill 256, 1, 0
#endif

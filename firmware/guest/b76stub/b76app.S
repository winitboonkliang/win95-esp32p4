# B76APP.EXE - generic guest stub for host-native apps (own code, GNU as, i386).
# The host app is in charge. The stub only (1) runs a message loop, (2) sends every window message to the host with ONE
# OUT instruction, and (3) executes the "call script" the host wrote into the shared area in reply (Win32 calls by index).
# Trap: out dx(0xF1E0), eax   eax = op | area_offset (area = SHARED + depth*0x1000; the host reads/writes guest memory itself).
# Area layout: +0 hwnd/arg0  +4 msg/arg1  +8 wparam/arg2  +12 lparam/arg3  +16 script ...
# Script entry: fn, nargs, store (byte offset into SHARED, 0 = none), ind (bit k: arg k = dword at SHARED+arg), args[nargs]
#   fn = -1 END   (return args[0], or the last result when nargs = 0)
#   fn = -2 TOUCH (args: ptr, len)    - make guest pages present before the host writes them
#   fn = -3 ASK   - trap again (op 3, area[0] = last result); the host writes a new script into this area
.intel_syntax noprefix
.include "imports.inc"
.set PORT, 0xF1E0
.set OP_START, 1
.set OP_MSG, 2
.set OP_ASK, 3
.globl _start
.text
_start:
    mov edi, SHARED                 # make the first 0x60000 bytes of the shared area present (scripts, structs, file buffer)
1:  mov byte ptr [edi], 0
    add edi, 0x1000
    cmp edi, SHARED + 0x60000
    jb 1b
    call [imp_GetCommandLineA]
    mov [SHARED + 0], eax
    mov dword ptr [SHARED + 4], offset wndproc
    push 0
    call [imp_GetModuleHandleA]
    mov [SHARED + 8], eax
    mov dword ptr [SHARED + 12], offset fntab
    mov ebx, SHARED
    mov eax, OP_START
    call trap_run                   # host script: RegisterClass, CreateWindowEx, menus, ShowWindow, open file ...
msgloop:
    push 0
    push 0
    push 0
    push offset msg
    call [imp_GetMessageA]
    test eax, eax
    jz quit
    push offset msg
    call [imp_TranslateMessage]
    push offset msg
    call [imp_DispatchMessageA]
    jmp msgloop
quit:
    push 0
    call [imp_ExitProcess]

wndproc:                            # stdcall (hwnd, msg, wparam, lparam)
    push ebx
    push esi
    push edi
    mov ebx, [depth]
    inc dword ptr [depth]
    shl ebx, 12
    add ebx, SHARED + 0x1000        # nesting level 0 = area 1 (area 0 belongs to _start)
    mov eax, [esp + 16]
    mov [ebx], eax
    mov eax, [esp + 20]
    mov [ebx + 4], eax
    mov eax, [esp + 24]
    mov [ebx + 8], eax
    mov eax, [esp + 28]
    mov [ebx + 12], eax
    mov eax, OP_MSG
    call trap_run
    dec dword ptr [depth]
    pop edi
    pop esi
    pop ebx
    ret 16

# trap_run: eax = op, ebx = area. Traps, then interprets the script at area+16. Returns eax = END value.
trap_run:
    push ebp
    push ebx
2:  lea eax, [eax + ebx - SHARED]   # op | area offset (area offsets are multiples of 0x1000, ops < 0x1000)
    mov edx, PORT
    out dx, eax                     # the host answers by writing the script; no guest exception, no VMM
    lea esi, [ebx + 16]
    xor ebp, ebp                    # ebp = last result
3:  mov eax, [esi]                  # fn
    cmp eax, -1
    je 9f
    cmp eax, -2
    je 7f
    cmp eax, -3
    je 8f
    mov ecx, [esi + 4]              # nargs
    mov edi, [esi + 12]             # ind mask
4:  test ecx, ecx                   # push args[nargs-1] .. args[0]
    jz 5f
    dec ecx
    mov edx, [esi + 16 + ecx*4]
    bt edi, ecx
    jnc 6f
    mov edx, [SHARED + edx]
6:  push edx
    jmp 4b
5:  mov eax, [fntab + eax*4]
    call [eax]                      # stdcall: callee pops
    mov ebp, eax
    mov edx, [esi + 8]
    test edx, edx
    jz 10f
    mov [SHARED + edx], eax
10: mov ecx, [esi + 4]
    lea esi, [esi + 16 + ecx*4]
    jmp 3b
7:  mov edi, [esi + 16]             # TOUCH ptr, len
    mov ecx, [esi + 20]
    add ecx, edi
11: cmp edi, ecx
    jae 12f
    or byte ptr [edi], 0
    add edi, 0x1000
    jmp 11b
12: or byte ptr [ecx - 1], 0
    add esi, 24
    jmp 3b
8:  mov [ebx], ebp                  # ASK: report the last result, get a new script
    mov eax, OP_ASK
    jmp 2b
9:  mov eax, ebp
    cmp dword ptr [esi + 4], 0
    je 13f
    mov eax, [esi + 16]
13: pop ebx
    pop ebp
    ret

.align 4
fntab: FNTAB
depth: .long 0
msg: .space 28

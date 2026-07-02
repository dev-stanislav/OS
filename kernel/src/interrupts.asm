extern interrupt_dispatcher

section .text

%macro INTERRUPT_STUB_NOERR 1
global interrupt_stub_%1
interrupt_stub_%1:
    push dword 0
    push dword %1
    jmp interrupt_common
%endmacro

%macro INTERRUPT_STUB_ERR 1
global interrupt_stub_%1
interrupt_stub_%1:
    push dword %1
    jmp interrupt_common
%endmacro

INTERRUPT_STUB_NOERR 0
INTERRUPT_STUB_NOERR 1
INTERRUPT_STUB_NOERR 2
INTERRUPT_STUB_NOERR 3
INTERRUPT_STUB_NOERR 4
INTERRUPT_STUB_NOERR 5
INTERRUPT_STUB_NOERR 6
INTERRUPT_STUB_NOERR 7
INTERRUPT_STUB_ERR 8
INTERRUPT_STUB_NOERR 9
INTERRUPT_STUB_ERR 10
INTERRUPT_STUB_ERR 11
INTERRUPT_STUB_ERR 12
INTERRUPT_STUB_ERR 13
INTERRUPT_STUB_ERR 14
INTERRUPT_STUB_NOERR 15
INTERRUPT_STUB_NOERR 16
INTERRUPT_STUB_ERR 17
INTERRUPT_STUB_NOERR 18
INTERRUPT_STUB_NOERR 19
INTERRUPT_STUB_NOERR 20
INTERRUPT_STUB_NOERR 21
INTERRUPT_STUB_NOERR 22
INTERRUPT_STUB_NOERR 23
INTERRUPT_STUB_NOERR 24
INTERRUPT_STUB_NOERR 25
INTERRUPT_STUB_NOERR 26
INTERRUPT_STUB_NOERR 27
INTERRUPT_STUB_NOERR 28
INTERRUPT_STUB_NOERR 29
INTERRUPT_STUB_NOERR 30
INTERRUPT_STUB_NOERR 31

%assign vector 32
%rep 224
INTERRUPT_STUB_NOERR vector
%assign vector vector+1
%endrep

interrupt_common:
    pusha

    xor eax, eax
    mov ax, ds
    push eax
    mov ax, es
    push eax
    mov ax, fs
    push eax
    mov ax, gs
    push eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call interrupt_dispatcher
    add esp, 4

    pop eax
    mov gs, ax
    pop eax
    mov fs, ax
    pop eax
    mov es, ax
    pop eax
    mov ds, ax

    popa
    add esp, 8
    iretd

section .data
global interrupt_stub_table
interrupt_stub_table:
%assign vector 0
%rep 256
    dd interrupt_stub_%+vector
%assign vector vector+1
%endrep

extern gdt_pointer

global load_gdt
global load_tss

load_gdt:
    lgdt [gdt_pointer]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush
.flush:
    ret

load_tss:
    mov ax, 0x28
    ltr ax
    ret

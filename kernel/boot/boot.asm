; Multiboot Header
MAGIC equ 0x1badb002
FLAGS equ 0x0
CHECKSUM equ -(MAGIC + FLAGS)

; Multiboot header
section .multiboot
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

; QEMU direct ELF boot requires a Xen PVH entry note.  The value is the
; physical entry address where QEMU must transfer control.
section .note.Xen note alloc align=4
    dd 4                  ; name size, including the trailing NUL
    dd 4                  ; descriptor size
    dd 18                 ; XEN_ELFNOTE_PHYS32_ENTRY
    db 'Xen', 0
    dd _start

; Stack
section .bss
    align 16
    stack_bottom:
        resb 16384
    stack_top:

; Entry point
section .text
    extern kernel_main
    global _start

    _start:
        mov esp, stack_top
        
        ; Clear the screen
        xor eax, eax
        mov edi, 0xb8000
        mov ecx, 80 * 25 * 2
        cld
        rep stosb
        
        ; Call kernel main
        call kernel_main
        
        ; Infinite loop
        cli
        hlt
        jmp $

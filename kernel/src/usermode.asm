global enter_user_mode
global user_exit_to_kernel
global user_kernel_stack_top
global user_demo_entry

extern user_demo_message

section .bss
align 16
user_kernel_stack:
    resb 4096
user_kernel_stack_top:
user_stack:
    resb 4096
user_stack_top:
saved_kernel_esp: resd 1
saved_kernel_eip: resd 1

section .text
enter_user_mode:
    mov [saved_kernel_esp], esp
    mov dword [saved_kernel_eip], .returned
    push dword 0x23
    push dword user_stack_top
    pushfd
    push dword 0x1B
    push dword user_demo_entry
    iretd
.returned:
    ret

user_exit_to_kernel:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov esp, [saved_kernel_esp]
    sti
    jmp dword [saved_kernel_eip]

user_demo_entry:
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov eax, 1
    mov ebx, user_demo_message
    int 0x80
    mov eax, 2
    int 0x80
.halt:
    jmp .halt

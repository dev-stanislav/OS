global irq0_stub
global irq1_stub
global syscall_stub

extern timer_irq_handler
extern keyboard_irq_handler
extern syscall_handler
extern user_exit_to_kernel

section .text
irq0_stub:
    pusha
    call timer_irq_handler
    mov al, 0x20
    out 0x20, al
    popa
    iretd

irq1_stub:
    pusha
    call keyboard_irq_handler
    mov al, 0x20
    out 0x20, al
    popa
    iretd

syscall_stub:
    pushad
    push dword [esp + 16]       ; saved EBX: syscall argument
    push dword [esp + 32]       ; saved EAX: syscall number
    call syscall_handler
    add esp, 8
    test eax, eax
    jnz user_exit_to_kernel
    popad
    iretd

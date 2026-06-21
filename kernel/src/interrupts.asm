global irq0_stub
global irq1_stub
global irq12_stub

extern timer_irq_handler
extern keyboard_irq_handler
extern mouse_irq_handler

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

irq12_stub:
    pusha
    call mouse_irq_handler
    mov al, 0x20
    out 0xA0, al
    out 0x20, al
    popa
    iretd

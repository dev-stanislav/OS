extern idt_pointer

global load_idt

load_idt:
    lidt [idt_pointer]
    ret

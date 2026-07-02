#include "idt.h"
#include "io.h"
#include "irq.h"
#include "keyboard.h"
#include "mouse.h"
#include "panic.h"
#include "syscall.h"
#include "timer.h"

#define IDT_SIZE 256
#define GDT_KERNEL_CODE 0x08
#define IDT_INTERRUPT_GATE 0x8E
#define IDT_USER_INTERRUPT_GATE 0xEE

static idt_entry_t idt_entries[IDT_SIZE];
// Accessed by load_idt in boot/idt_load.asm.
idt_pointer_t idt_pointer;

extern void (*interrupt_stub_table[IDT_SIZE])(void);

void idt_set_entry(int index, uint32_t offset, uint16_t selector, uint8_t type) {
    if (index < 0 || index >= IDT_SIZE) return;
    
    idt_entries[index].offset_low = offset & 0xFFFF;
    idt_entries[index].offset_high = (offset >> 16) & 0xFFFF;
    idt_entries[index].selector = selector;
    idt_entries[index].zero = 0;
    idt_entries[index].type_attributes = type;
}

void idt_init(void) {
    idt_pointer.limit = (sizeof(idt_entry_t) * IDT_SIZE) - 1;
    idt_pointer.base = (uint32_t)&idt_entries;
    
    for (int i = 0; i < IDT_SIZE; i++) {
        idt_set_entry(i, (uint32_t)interrupt_stub_table[i], GDT_KERNEL_CODE, IDT_INTERRUPT_GATE);
    }
    idt_set_entry(SYSCALL_VECTOR, (uint32_t)interrupt_stub_table[SYSCALL_VECTOR], GDT_KERNEL_CODE, IDT_USER_INTERRUPT_GATE);

    irq_init();
    irq_register_handler(0, timer_irq_handler);
    irq_register_handler(1, keyboard_irq_handler);
    irq_register_handler(12, mouse_irq_handler);
    irq_unmask(0);
    irq_unmask(1);
    irq_unmask(2);
    irq_unmask(12);
    load_idt();
}

void interrupt_dispatcher(interrupt_frame_t *frame) {
    if (frame->interrupt < 32) {
        panic("CPU exception", frame);
    }
    if (frame->interrupt == SYSCALL_VECTOR) {
        syscall_dispatch(frame);
        return;
    }

    if (frame->interrupt >= 32 && frame->interrupt < 48) {
        irq_dispatch((uint8_t)(frame->interrupt - 32));
        return;
    }

    panic("Unhandled interrupt", frame);
}

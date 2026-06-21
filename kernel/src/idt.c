#include "idt.h"
#include "io.h"

#define IDT_SIZE 256

static idt_entry_t idt_entries[IDT_SIZE];
// Accessed by load_idt in boot/idt_load.asm.
idt_pointer_t idt_pointer;

extern void irq0_stub(void);
extern void irq1_stub(void);

static void pic_remap(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0xFC); /* timer + keyboard only */
    outb(0xA1, 0xFF);
}

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
    
    // Clear all entries
    for (int i = 0; i < IDT_SIZE; i++) {
        idt_set_entry(i, 0, 0, 0);
    }

    idt_set_entry(32, (uint32_t)irq0_stub, 0x08, 0x8E);
    idt_set_entry(33, (uint32_t)irq1_stub, 0x08, 0x8E);
    pic_remap();
    load_idt();
}

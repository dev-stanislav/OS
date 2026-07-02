#ifndef IDT_H
#define IDT_H

#include <stdint.h>

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attributes;
    uint16_t offset_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_pointer_t;

typedef struct {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t interrupt, error;
    uint32_t eip, cs, eflags, useresp, ss;
} __attribute__((packed)) interrupt_frame_t;

typedef void (*irq_handler_t)(void);

extern void load_idt(void);
void idt_init(void);
void idt_set_entry(int index, uint32_t offset, uint16_t selector, uint8_t type);
void irq_register_handler(uint8_t irq, irq_handler_t handler);
void interrupt_dispatcher(interrupt_frame_t *frame);
void panic(const char *message, const interrupt_frame_t *frame) __attribute__((noreturn));

#endif

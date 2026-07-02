#include "irq.h"
#include "io.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI 0x20
#define PIC_READ_ISR 0x0B

static irq_handler_t handlers[16];
static uint32_t counts[16];
static uint32_t spurious[16];
static uint16_t mask_bits;

static void io_wait(void) { outb(0x80, 0); }

static void pic_write_masks(void) {
    outb(PIC1_DATA, (uint8_t)(mask_bits & 0xFF));
    outb(PIC2_DATA, (uint8_t)(mask_bits >> 8));
}

static uint8_t pic_isr(uint8_t slave) {
    uint16_t command = slave ? PIC2_COMMAND : PIC1_COMMAND;
    outb(command, PIC_READ_ISR);
    return inb(command);
}

void irq_init(void) {
    outb(PIC1_COMMAND, 0x11); io_wait();
    outb(PIC2_COMMAND, 0x11); io_wait();
    outb(PIC1_DATA, 0x20); io_wait();
    outb(PIC2_DATA, 0x28); io_wait();
    outb(PIC1_DATA, 0x04); io_wait();
    outb(PIC2_DATA, 0x02); io_wait();
    outb(PIC1_DATA, 0x01); io_wait();
    outb(PIC2_DATA, 0x01); io_wait();
    mask_bits = 0xFFFFu;
    pic_write_masks();
    for (uint8_t i=0;i<16;i++) {
        handlers[i] = 0;
        counts[i] = 0;
        spurious[i] = 0;
    }
}

void irq_register_handler(uint8_t irq, irq_handler_t handler) {
    if (irq < 16) handlers[irq] = handler;
}

void irq_dispatch(uint8_t irq) {
    if (irq >= 16) return;
    if (irq == 7 && !(pic_isr(0) & 0x80)) {
        spurious[irq]++;
        return;
    }
    if (irq == 15 && !(pic_isr(1) & 0x80)) {
        spurious[irq]++;
        outb(PIC1_COMMAND, PIC_EOI);
        return;
    }
    counts[irq]++;
    if (handlers[irq]) handlers[irq]();
    irq_send_eoi(irq);
}

void irq_mask(uint8_t irq) {
    if (irq >= 16) return;
    mask_bits |= (uint16_t)(1u << irq);
    pic_write_masks();
}

void irq_unmask(uint8_t irq) {
    if (irq >= 16) return;
    mask_bits &= (uint16_t)~(1u << irq);
    pic_write_masks();
}

void irq_send_eoi(uint8_t irq) {
    if (irq >= 16) return;
    if (irq >= 8) outb(PIC2_COMMAND, PIC_EOI);
    outb(PIC1_COMMAND, PIC_EOI);
}

uint8_t irq_is_masked(uint8_t irq) {
    return irq < 16 ? (uint8_t)((mask_bits >> irq) & 1u) : 1;
}

uint32_t irq_count(uint8_t irq) {
    return irq < 16 ? counts[irq] : 0;
}

uint32_t irq_spurious_count(uint8_t irq) {
    return irq < 16 ? spurious[irq] : 0;
}

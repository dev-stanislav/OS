#include "idt.h"
#include "io.h"
#include "keyboard.h"
#include "libk.h"
#include "mouse.h"
#include "timer.h"
#include "vga.h"

#define IDT_SIZE 256
#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI 0x20
#define GDT_KERNEL_CODE 0x08
#define IDT_INTERRUPT_GATE 0x8E

static idt_entry_t idt_entries[IDT_SIZE];
// Accessed by load_idt in boot/idt_load.asm.
idt_pointer_t idt_pointer;

extern void (*interrupt_stub_table[IDT_SIZE])(void);

static irq_handler_t irq_handlers[16];

static const char *exception_names[32] = {
    "Divide Error",
    "Debug",
    "Non-maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved"
};

static void io_wait(void) {
    outb(0x80, 0);
}

static void pic_remap(void) {
    outb(PIC1_COMMAND, 0x11); io_wait();
    outb(PIC2_COMMAND, 0x11); io_wait();
    outb(PIC1_DATA, 0x20); io_wait();
    outb(PIC2_DATA, 0x28); io_wait();
    outb(PIC1_DATA, 0x04); io_wait();
    outb(PIC2_DATA, 0x02); io_wait();
    outb(PIC1_DATA, 0x01); io_wait();
    outb(PIC2_DATA, 0x01); io_wait();
    outb(PIC1_DATA, 0xF8); io_wait(); /* IRQ0 timer, IRQ1 keyboard, IRQ2 cascade */
    outb(PIC2_DATA, 0xEF); io_wait(); /* IRQ12 mouse */
}

static void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) outb(PIC2_COMMAND, PIC_EOI);
    outb(PIC1_COMMAND, PIC_EOI);
}

static void print_number(uint32_t value) {
    char text[16];
    kitoa(value, text, 10);
    vga_write(text, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

static void print_hex(uint32_t value) {
    char text[16];
    vga_write("0x", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    for (int shift=28;shift>=0;shift-=4) {
        uint8_t digit = (uint8_t)((value >> shift) & 0x0F);
        text[0] = (char)(digit < 10 ? '0' + digit : 'A' + digit - 10);
        text[1] = '\0';
        vga_write(text, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    }
}

static void dump_register(const char *name, uint32_t value) {
    vga_write(name, VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_write("=", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    print_hex(value);
    vga_write("  ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

static uint32_t read_cr2(void) {
    uint32_t value;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(value));
    return value;
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
    
    for (int i = 0; i < IDT_SIZE; i++) {
        idt_set_entry(i, (uint32_t)interrupt_stub_table[i], GDT_KERNEL_CODE, IDT_INTERRUPT_GATE);
    }

    for (uint8_t i=0;i<16;i++) irq_handlers[i] = 0;
    irq_register_handler(0, timer_irq_handler);
    irq_register_handler(1, keyboard_irq_handler);
    irq_register_handler(12, mouse_irq_handler);

    pic_remap();
    load_idt();
}

void irq_register_handler(uint8_t irq, irq_handler_t handler) {
    if (irq < 16) irq_handlers[irq] = handler;
}

void panic(const char *message, const interrupt_frame_t *frame) {
    disable_interrupts();
    vga_text_mode();
    vga_clear(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_write("KERNEL PANIC\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_write(message ? message : "unknown panic", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_write("\n\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    if (frame) {
        vga_write("Vector: ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        print_number(frame->interrupt);
        if (frame->interrupt < 32) {
            vga_write(" (", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            vga_write(exception_names[frame->interrupt], VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
            vga_write(")", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        }
        vga_write("  Error: ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        print_hex(frame->error);
        vga_write("\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        if (frame->interrupt == 14) {
            vga_write("CR2:    ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            print_hex(read_cr2());
            vga_write("\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        }

        dump_register("EIP", frame->eip); dump_register("CS", frame->cs); dump_register("EFLAGS", frame->eflags); vga_write("\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        dump_register("EAX", frame->eax); dump_register("EBX", frame->ebx); dump_register("ECX", frame->ecx); dump_register("EDX", frame->edx); vga_write("\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        dump_register("ESI", frame->esi); dump_register("EDI", frame->edi); dump_register("EBP", frame->ebp); dump_register("ESP", frame->esp); vga_write("\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        dump_register("DS", frame->ds); dump_register("ES", frame->es); dump_register("FS", frame->fs); dump_register("GS", frame->gs); vga_write("\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    }

    vga_write("\nSystem halted.\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    for (;;) cpu_halt();
}

void interrupt_dispatcher(interrupt_frame_t *frame) {
    if (frame->interrupt < 32) {
        panic("CPU exception", frame);
    }

    if (frame->interrupt >= 32 && frame->interrupt < 48) {
        uint8_t irq = (uint8_t)(frame->interrupt - 32);
        if (irq_handlers[irq]) irq_handlers[irq]();
        pic_send_eoi(irq);
        return;
    }

    panic("Unhandled interrupt", frame);
}

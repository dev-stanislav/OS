#include "panic.h"
#include "io.h"
#include "ksym.h"
#include "libk.h"
#include "serial.h"
#include "vga.h"

static void panic_write(const char *text, uint8_t color) {
    vga_write(text, color, VGA_COLOR_BLACK);
    serial_write(text);
}

static void print_number(uint32_t value) {
    char text[16];
    kitoa(value, text, 10);
    panic_write(text, VGA_COLOR_WHITE);
}

static void print_hex(uint32_t value) {
    char text[16];
    panic_write("0x", VGA_COLOR_LIGHT_CYAN);
    for (int shift=28;shift>=0;shift-=4) {
        uint8_t digit = (uint8_t)((value >> shift) & 0x0F);
        text[0] = (char)(digit < 10 ? '0' + digit : 'A' + digit - 10);
        text[1] = '\0';
        panic_write(text, VGA_COLOR_WHITE);
    }
}

static void dump_register(const char *name, uint32_t value) {
    panic_write(name, VGA_COLOR_LIGHT_GREY);
    panic_write("=", VGA_COLOR_LIGHT_GREY);
    print_hex(value);
    panic_write("  ", VGA_COLOR_WHITE);
}

static uint32_t read_cr2(void) {
    uint32_t value;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(value));
    return value;
}

static const char *exception_name(uint32_t vector) {
    static const char *names[32] = {
        "Divide Error", "Debug", "Non-maskable Interrupt", "Breakpoint",
        "Overflow", "Bound Range Exceeded", "Invalid Opcode", "Device Not Available",
        "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present",
        "Stack-Segment Fault", "General Protection Fault", "Page Fault", "Reserved",
        "x87 Floating-Point Exception", "Alignment Check", "Machine Check", "SIMD Floating-Point Exception",
        "Virtualization Exception", "Control Protection Exception", "Reserved", "Reserved",
        "Reserved", "Reserved", "Reserved", "Reserved",
        "Hypervisor Injection Exception", "VMM Communication Exception", "Security Exception", "Reserved"
    };
    return vector < 32 ? names[vector] : "Interrupt";
}

void panic_stack_trace(uint32_t ebp) {
    panic_write("\nStack trace:\n", VGA_COLOR_LIGHT_CYAN);
    for (uint8_t depth=0;depth<8 && ebp;depth++) {
        uint32_t *frame = (uint32_t*)ebp;
        uint32_t next = frame[0];
        uint32_t ret = frame[1];
        uint32_t base = 0;
        const char *name = ksym_lookup(ret, &base);
        panic_write("  ", VGA_COLOR_WHITE);
        print_hex(ret);
        if (name) {
            panic_write(" <", VGA_COLOR_LIGHT_GREY);
            panic_write(name, VGA_COLOR_LIGHT_BROWN);
            panic_write("+", VGA_COLOR_LIGHT_GREY);
            print_hex(ret - base);
            panic_write(">", VGA_COLOR_LIGHT_GREY);
        }
        panic_write("\n", VGA_COLOR_WHITE);
        if (next <= ebp || next - ebp > 0x10000u) break;
        ebp = next;
    }
}

void panic(const char *message, const interrupt_frame_t *frame) {
    disable_interrupts();
    serial_write("\n\n");
    vga_text_mode();
    vga_clear(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    panic_write("KERNEL PANIC\n", VGA_COLOR_LIGHT_RED);
    panic_write(message ? message : "unknown panic", VGA_COLOR_LIGHT_RED);
    panic_write("\n\n", VGA_COLOR_WHITE);

    if (frame) {
        panic_write("Vector: ", VGA_COLOR_WHITE);
        print_number(frame->interrupt);
        panic_write(" (", VGA_COLOR_WHITE);
        panic_write(exception_name(frame->interrupt), VGA_COLOR_LIGHT_BROWN);
        panic_write(")  Error: ", VGA_COLOR_WHITE);
        print_hex(frame->error);
        panic_write("\n", VGA_COLOR_WHITE);
        if (frame->interrupt == 14) {
            panic_write("CR2:    ", VGA_COLOR_WHITE);
            print_hex(read_cr2());
            panic_write("\n", VGA_COLOR_WHITE);
        }

        dump_register("EIP", frame->eip); dump_register("CS", frame->cs); dump_register("EFLAGS", frame->eflags); panic_write("\n", VGA_COLOR_WHITE);
        dump_register("EAX", frame->eax); dump_register("EBX", frame->ebx); dump_register("ECX", frame->ecx); dump_register("EDX", frame->edx); panic_write("\n", VGA_COLOR_WHITE);
        dump_register("ESI", frame->esi); dump_register("EDI", frame->edi); dump_register("EBP", frame->ebp); dump_register("ESP", frame->esp); panic_write("\n", VGA_COLOR_WHITE);
        dump_register("DS", frame->ds); dump_register("ES", frame->es); dump_register("FS", frame->fs); dump_register("GS", frame->gs); panic_write("\n", VGA_COLOR_WHITE);
        panic_stack_trace(frame->ebp);
    } else {
        uint32_t ebp;
        __asm__ volatile ("mov %%ebp, %0" : "=r"(ebp));
        panic_stack_trace(ebp);
    }

    panic_write("\nSystem halted.\n", VGA_COLOR_LIGHT_RED);
    for (;;) cpu_halt();
}

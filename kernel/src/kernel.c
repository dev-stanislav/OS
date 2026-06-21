#include "vga.h"
#include "gdt.h"
#include "idt.h"
#include "console.h"

void kernel_main(void) {
    vga_init();
    gdt_init();
    idt_init();
    
    vga_write("========================================\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write("Welcome to MiniOS 32-bit Kernel!\n", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write("========================================\n\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    
    vga_write("System Information:\n", VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    vga_write("  Architecture: i686\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_write("  VGA Mode: Text 80x25\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_write("  GDT: Loaded\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_write("  IDT: Loaded\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_write("\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    console_init();

    for (;;) {
        console_update();
    }
}

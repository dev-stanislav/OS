#include "vga.h"

static uint16_t *vga_buffer = (uint16_t *)0xb8000;
static size_t vga_row = 0;
static size_t vga_col = 0;
static uint8_t vga_fg = VGA_COLOR_WHITE;
static uint8_t vga_bg = VGA_COLOR_BLACK;

void vga_init(void) {
    vga_row = 0;
    vga_col = 0;
    vga_clear(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

static uint16_t vga_entry(char c, uint8_t fg, uint8_t bg) {
    return (uint16_t)c | ((uint16_t)fg | (uint16_t)bg << 4) << 8;
}

void vga_putchar(char c, uint8_t fg, uint8_t bg) {
    if (c == '\n') {
        vga_newline();
        return;
    }
    
    vga_buffer[vga_row * VGA_WIDTH + vga_col] = vga_entry(c, fg, bg);
    
    vga_col++;
    if (vga_col >= VGA_WIDTH) {
        vga_newline();
    }
}

void vga_write(const char *str, uint8_t fg, uint8_t bg) {
    if (!str) return;
    
    while (*str) {
        vga_putchar(*str, fg, bg);
        str++;
    }
}

void vga_clear(uint8_t fg, uint8_t bg) {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_entry(' ', fg, bg);
        }
    }
    vga_row = 0;
    vga_col = 0;
}

void vga_newline(void) {
    vga_col = 0;
    vga_row++;
    
    if (vga_row >= VGA_HEIGHT) {
        // Scroll up
        for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];
            }
        }
        
        // Clear last line
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', vga_fg, vga_bg);
        }
        
        vga_row = VGA_HEIGHT - 1;
    }
}

void vga_backspace(uint8_t fg, uint8_t bg) {
    if (vga_col > 0) {
        vga_col--;
    } else if (vga_row > 0) {
        vga_row--;
        vga_col = VGA_WIDTH - 1;
    } else {
        return;
    }

    vga_buffer[vga_row * VGA_WIDTH + vga_col] = vga_entry(' ', fg, bg);
}

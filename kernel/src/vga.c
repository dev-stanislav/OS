#include "vga.h"
#include "io.h"

static uint16_t *vga_buffer = (uint16_t *)0xb8000;
static size_t vga_row;
static size_t vga_col;
static uint8_t vga_fg = VGA_COLOR_WHITE;
static uint8_t vga_bg = VGA_COLOR_BLACK;

static void vga_write_registers(const uint8_t *regs) {
    outb(0x3C2,*regs++);
    for(uint8_t i=0;i<5;i++){outb(0x3C4,i);outb(0x3C5,*regs++);}
    outb(0x3D4,0x03);outb(0x3D5,inb(0x3D5)|0x80);outb(0x3D4,0x11);outb(0x3D5,inb(0x3D5)&~0x80);
    for(uint8_t i=0;i<25;i++){outb(0x3D4,i);outb(0x3D5,*regs++);}
    for(uint8_t i=0;i<9;i++){outb(0x3CE,i);outb(0x3CF,*regs++);}
    for(uint8_t i=0;i<21;i++){(void)inb(0x3DA);outb(0x3C0,i);outb(0x3C0,*regs++);} (void)inb(0x3DA);outb(0x3C0,0x20);
}

void vga_text_mode(void) {
    static const uint8_t mode_03[] = {0x67,0x03,0x00,0x03,0x00,0x02,0x5F,0x4F,0x50,0x82,0x55,0x81,0xBF,0x1F,0x00,0x4F,0x0D,0x0E,0x00,0x00,0x00,0x50,0x9C,0x8E,0x8F,0x28,0x1F,0x96,0xB9,0xA3,0xFF,0x00,0x00,0x00,0x00,0x00,0x10,0x0E,0x00,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x0F,0x00,0x00};
    vga_write_registers(mode_03);
}

static uint16_t vga_entry(char c, uint8_t fg, uint8_t bg) {
    return (uint16_t)c | (((uint16_t)fg | ((uint16_t)bg << 4)) << 8);
}

void vga_update_cursor(void) {
    uint16_t position = (uint16_t)(vga_row * VGA_WIDTH + vga_col);
    outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)position);
    outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)(position >> 8));
}

void vga_init(void) { vga_clear(VGA_COLOR_WHITE, VGA_COLOR_BLACK); }

void vga_put_at(size_t row, size_t col, char c, uint8_t fg, uint8_t bg) {
    if (row < VGA_HEIGHT && col < VGA_WIDTH) vga_buffer[row * VGA_WIDTH + col] = vga_entry(c, fg, bg);
}

void vga_clear(uint8_t fg, uint8_t bg) {
    vga_fg = fg; vga_bg = bg;
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++) vga_put_at(y, x, ' ', fg, bg);
    vga_row = vga_col = 0;
    vga_update_cursor();
}

void vga_newline(void) {
    vga_col = 0;
    if (++vga_row < VGA_HEIGHT) { vga_update_cursor(); return; }
    for (size_t y = 0; y + 1 < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++) vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; x++) vga_put_at(VGA_HEIGHT - 1, x, ' ', vga_fg, vga_bg);
    vga_row = VGA_HEIGHT - 1;
    vga_update_cursor();
}

void vga_putchar(char c, uint8_t fg, uint8_t bg) {
    vga_fg = fg; vga_bg = bg;
    if (c == '\n') { vga_newline(); return; }
    vga_put_at(vga_row, vga_col, c, fg, bg);
    if (++vga_col == VGA_WIDTH) vga_newline(); else vga_update_cursor();
}

void vga_write(const char *str, uint8_t fg, uint8_t bg) {
    while (str && *str) vga_putchar(*str++, fg, bg);
}

void vga_backspace(uint8_t fg, uint8_t bg) {
    if (vga_col > 0) vga_col--;
    else if (vga_row > 0) { vga_row--; vga_col = VGA_WIDTH - 1; }
    else return;
    vga_put_at(vga_row, vga_col, ' ', fg, bg);
    vga_update_cursor();
}

size_t vga_get_row(void) { return vga_row; }
size_t vga_get_col(void) { return vga_col; }

void vga_set_position(size_t row, size_t col) {
    vga_row = row < VGA_HEIGHT ? row : VGA_HEIGHT - 1;
    vga_col = col < VGA_WIDTH ? col : VGA_WIDTH - 1;
    vga_update_cursor();
}

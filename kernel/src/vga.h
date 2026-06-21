#ifndef VGA_H
#define VGA_H

#include <stdint.h>
#include <stddef.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

#define VGA_COLOR_BLACK 0
#define VGA_COLOR_BLUE 1
#define VGA_COLOR_GREEN 2
#define VGA_COLOR_CYAN 3
#define VGA_COLOR_RED 4
#define VGA_COLOR_MAGENTA 5
#define VGA_COLOR_BROWN 6
#define VGA_COLOR_LIGHT_GREY 7
#define VGA_COLOR_DARK_GREY 8
#define VGA_COLOR_LIGHT_BLUE 9
#define VGA_COLOR_LIGHT_GREEN 10
#define VGA_COLOR_LIGHT_CYAN 11
#define VGA_COLOR_LIGHT_RED 12
#define VGA_COLOR_LIGHT_MAGENTA 13
#define VGA_COLOR_LIGHT_BROWN 14
#define VGA_COLOR_WHITE 15

void vga_init(void);
void vga_text_mode(void);
void vga_putchar(char c, uint8_t fg, uint8_t bg);
void vga_write(const char *str, uint8_t fg, uint8_t bg);
void vga_clear(uint8_t fg, uint8_t bg);
void vga_newline(void);
void vga_backspace(uint8_t fg, uint8_t bg);
size_t vga_get_row(void);
size_t vga_get_col(void);
void vga_set_position(size_t row, size_t col);
void vga_put_at(size_t row, size_t col, char c, uint8_t fg, uint8_t bg);
void vga_update_cursor(void);

#endif

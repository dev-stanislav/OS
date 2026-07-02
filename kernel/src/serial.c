#include "serial.h"
#include "io.h"

#define COM1 0x3F8

void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

void serial_write_char(char value) {
    for (uint32_t wait=0;wait<100000;wait++) if (inb(COM1 + 5) & 0x20) break;
    outb(COM1, (uint8_t)value);
}

void serial_write(const char *text) {
    while (text && *text) {
        if (*text == '\n') serial_write_char('\r');
        serial_write_char(*text++);
    }
}

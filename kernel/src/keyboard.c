#include <stdint.h>
#include "keyboard.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64

static uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

char keyboard_read_char(void) {
    static const char keymap[128] = {
        [2] = '1', [3] = '2', [4] = '3', [5] = '4', [6] = '5',
        [7] = '6', [8] = '7', [9] = '8', [10] = '9', [11] = '0',
        [12] = '-', [14] = '\b', [15] = '\t', [28] = '\n', [57] = ' ',
        [16] = 'q', [17] = 'w', [18] = 'e', [19] = 'r', [20] = 't',
        [21] = 'y', [22] = 'u', [23] = 'i', [24] = 'o', [25] = 'p',
        [30] = 'a', [31] = 's', [32] = 'd', [33] = 'f', [34] = 'g',
        [35] = 'h', [36] = 'j', [37] = 'k', [38] = 'l',
        [44] = 'z', [45] = 'x', [46] = 'c', [47] = 'v', [48] = 'b',
        [49] = 'n', [50] = 'm'
    };

    if ((inb(KEYBOARD_STATUS_PORT) & 1) == 0) {
        return 0;
    }

    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    if (scancode & 0x80) {
        return 0;
    }

    return keymap[scancode];
}

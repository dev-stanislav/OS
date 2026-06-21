#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

enum key_code {
    KEY_NONE = 0,
    KEY_ESCAPE = 127,
    KEY_LEFT = 128, KEY_RIGHT, KEY_UP, KEY_DOWN,
    KEY_HOME, KEY_END, KEY_DELETE
};

void keyboard_init(void);
void keyboard_irq_handler(void);
uint16_t keyboard_pop_event(void);

#endif

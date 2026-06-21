#ifndef MOUSE_H
#define MOUSE_H
#include <stdint.h>
void mouse_init(void);
void mouse_irq_handler(void);
int mouse_x(void);
int mouse_y(void);
uint8_t mouse_left(void);
#endif

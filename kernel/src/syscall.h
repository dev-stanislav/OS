#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include "idt.h"

#define SYSCALL_VECTOR 0x80
#define SYSCALL_MAX 16

typedef uint32_t (*syscall_handler_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

void syscall_init(void);
uint8_t syscall_register(uint32_t number, syscall_handler_t handler);
void syscall_dispatch(interrupt_frame_t *frame);

#endif

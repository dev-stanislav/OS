#ifndef PANIC_H
#define PANIC_H

#include "idt.h"

void panic(const char *message, const interrupt_frame_t *frame) __attribute__((noreturn));
void panic_stack_trace(uint32_t ebp);

#endif

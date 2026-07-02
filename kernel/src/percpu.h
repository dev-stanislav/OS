#ifndef PERCPU_H
#define PERCPU_H

#include <stdint.h>

typedef struct {
    uint32_t id;
    uint32_t lapic_id;
    uint32_t kernel_stack;
} percpu_t;

void percpu_init_bsp(void);
uint32_t cpu_id(void);
percpu_t *percpu_get(void);

#endif

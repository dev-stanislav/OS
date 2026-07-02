#include "percpu.h"

static percpu_t bsp_cpu;

void percpu_init_bsp(void) {
    bsp_cpu.id = 0;
    bsp_cpu.lapic_id = 0;
    bsp_cpu.kernel_stack = 0;
}

uint32_t cpu_id(void) { return bsp_cpu.id; }
percpu_t *percpu_get(void) { return &bsp_cpu; }

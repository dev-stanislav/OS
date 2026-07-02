#ifndef CPU_H
#define CPU_H

#include <stdint.h>

typedef enum {
    CPU_FEATURE_FPU      = 1u << 0,
    CPU_FEATURE_TSC      = 1u << 1,
    CPU_FEATURE_MSR      = 1u << 2,
    CPU_FEATURE_PAE      = 1u << 3,
    CPU_FEATURE_APIC     = 1u << 4,
    CPU_FEATURE_SEP      = 1u << 5,
    CPU_FEATURE_SSE      = 1u << 6,
    CPU_FEATURE_FXSR     = 1u << 7,
    CPU_FEATURE_NX       = 1u << 8
} cpu_feature_t;

typedef struct {
    char vendor[13];
    uint32_t max_leaf;
    uint32_t max_extended_leaf;
    uint32_t family;
    uint32_t model;
    uint32_t stepping;
    uint32_t features;
} cpu_info_t;

void cpu_init(void);
const cpu_info_t *cpu_info(void);
uint8_t cpu_has(cpu_feature_t feature);
void cpu_fpu_init(void);
void cpu_fpu_save(void *area);
void cpu_fpu_restore(const void *area);

#endif

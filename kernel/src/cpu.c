#include "cpu.h"
#include "libk.h"

static cpu_info_t info;

static uint8_t cpuid_supported(void) {
    uint32_t before, after;
    __asm__ volatile (
        "pushfl\n"
        "popl %0\n"
        "movl %0, %1\n"
        "xorl $0x200000, %1\n"
        "pushl %1\n"
        "popfl\n"
        "pushfl\n"
        "popl %1\n"
        "pushl %0\n"
        "popfl\n"
        : "=&r"(before), "=&r"(after)
        :
        : "cc");
    return ((before ^ after) & 0x200000u) != 0;
}

static void cpuid(uint32_t leaf, uint32_t subleaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile (
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(subleaf));
}

void cpu_init(void) {
    kmemset(&info, 0, sizeof(info));
    kstrcpy(info.vendor, "unknown", sizeof(info.vendor));
    if (!cpuid_supported()) return;

    uint32_t eax, ebx, ecx, edx;
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    info.max_leaf = eax;
    ((uint32_t*)info.vendor)[0] = ebx;
    ((uint32_t*)info.vendor)[1] = edx;
    ((uint32_t*)info.vendor)[2] = ecx;
    info.vendor[12] = '\0';

    if (info.max_leaf >= 1) {
        cpuid(1, 0, &eax, &ebx, &ecx, &edx);
        info.stepping = eax & 0x0Fu;
        info.model = (eax >> 4) & 0x0Fu;
        info.family = (eax >> 8) & 0x0Fu;
        if (edx & (1u << 0)) info.features |= CPU_FEATURE_FPU;
        if (edx & (1u << 4)) info.features |= CPU_FEATURE_TSC;
        if (edx & (1u << 5)) info.features |= CPU_FEATURE_MSR;
        if (edx & (1u << 6)) info.features |= CPU_FEATURE_PAE;
        if (edx & (1u << 9)) info.features |= CPU_FEATURE_APIC;
        if (edx & (1u << 11)) info.features |= CPU_FEATURE_SEP;
        if (edx & (1u << 24)) info.features |= CPU_FEATURE_FXSR;
        if (edx & (1u << 25)) info.features |= CPU_FEATURE_SSE;
    }

    cpuid(0x80000000u, 0, &eax, &ebx, &ecx, &edx);
    info.max_extended_leaf = eax;
    if (info.max_extended_leaf >= 0x80000001u) {
        cpuid(0x80000001u, 0, &eax, &ebx, &ecx, &edx);
        if (edx & (1u << 20)) info.features |= CPU_FEATURE_NX;
    }
    cpu_fpu_init();
}

const cpu_info_t *cpu_info(void) { return &info; }
uint8_t cpu_has(cpu_feature_t feature) { return (info.features & (uint32_t)feature) != 0; }

void cpu_fpu_init(void) {
    if (!cpu_has(CPU_FEATURE_FPU)) return;
    uint32_t cr0, cr4;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~0x04u;
    cr0 |= 0x02u;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));
    if (cpu_has(CPU_FEATURE_SSE) && cpu_has(CPU_FEATURE_FXSR)) {
        __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
        cr4 |= (1u << 9) | (1u << 10);
        __asm__ volatile ("mov %0, %%cr4" : : "r"(cr4));
    }
    __asm__ volatile ("fninit");
}

void cpu_fpu_save(void *area) {
    if (!area || !cpu_has(CPU_FEATURE_FPU)) return;
    if (cpu_has(CPU_FEATURE_SSE) && cpu_has(CPU_FEATURE_FXSR)) __asm__ volatile ("fxsave (%0)" : : "r"(area) : "memory");
    else __asm__ volatile ("fnsave (%0)" : : "r"(area) : "memory");
}

void cpu_fpu_restore(const void *area) {
    if (!area || !cpu_has(CPU_FEATURE_FPU)) return;
    if (cpu_has(CPU_FEATURE_SSE) && cpu_has(CPU_FEATURE_FXSR)) __asm__ volatile ("fxrstor (%0)" : : "r"(area) : "memory");
    else __asm__ volatile ("frstor (%0)" : : "r"(area) : "memory");
}

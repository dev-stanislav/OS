#include "apic.h"
#include "cpu.h"

uint8_t apic_available(void) { return cpu_has(CPU_FEATURE_APIC); }
uint8_t lapic_init(void) { return 0; }
uint8_t ioapic_init(void) { return 0; }
uint8_t apic_timer_init(void) { return 0; }

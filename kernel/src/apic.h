#ifndef APIC_H
#define APIC_H

#include <stdint.h>

uint8_t apic_available(void);
uint8_t lapic_init(void);
uint8_t ioapic_init(void);
uint8_t apic_timer_init(void);

#endif

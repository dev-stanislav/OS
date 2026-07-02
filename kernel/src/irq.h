#ifndef IRQ_H
#define IRQ_H

#include <stdint.h>

typedef void (*irq_handler_t)(void);

void irq_init(void);
void irq_register_handler(uint8_t irq, irq_handler_t handler);
void irq_dispatch(uint8_t irq);
void irq_mask(uint8_t irq);
void irq_unmask(uint8_t irq);
void irq_send_eoi(uint8_t irq);
uint8_t irq_is_masked(uint8_t irq);
uint32_t irq_count(uint8_t irq);
uint32_t irq_spurious_count(uint8_t irq);

#endif

#include "timer.h"
#include "io.h"

#define PIT_COMMAND 0x43
#define PIT_CHANNEL0 0x40
#define PIT_FREQUENCY 1193182u

static volatile uint32_t ticks;

void timer_init(void) {
    uint16_t divisor = (uint16_t)(PIT_FREQUENCY / TIMER_HZ);
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)(divisor >> 8));
}

void timer_irq_handler(void) { ticks++; }

uint32_t timer_ticks(void) { return ticks; }

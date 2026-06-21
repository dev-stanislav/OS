#include "heap.h"

#define HEAP_SIZE (128 * 1024)

static uint8_t heap[HEAP_SIZE] __attribute__((aligned(16)));
static uint32_t offset;

void heap_init(void) { offset = 0; }

void *kmalloc(size_t size) {
    uint32_t aligned = ((uint32_t)size + 15u) & ~15u;
    if (aligned > HEAP_SIZE - offset) return 0;
    void *block = &heap[offset];
    offset += aligned;
    return block;
}

uint32_t heap_used(void) { return offset; }
uint32_t heap_capacity(void) { return HEAP_SIZE; }

#ifndef KSYM_H
#define KSYM_H

#include <stdint.h>

typedef struct {
    uint32_t address;
    const char *name;
} ksym_entry_t;

extern const ksym_entry_t ksym_table[];
extern const uint32_t ksym_table_count;

const char *ksym_lookup(uint32_t address, uint32_t *base);

#endif

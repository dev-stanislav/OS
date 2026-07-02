#include "ksym.h"

__attribute__((weak)) const ksym_entry_t ksym_table[] = {{0, 0}};
__attribute__((weak)) const uint32_t ksym_table_count = 0;

const char *ksym_lookup(uint32_t address, uint32_t *base) {
    const char *best = 0;
    uint32_t best_address = 0;
    for (uint32_t i=0;i<ksym_table_count;i++) {
        if (ksym_table[i].address <= address && ksym_table[i].address >= best_address) {
            best_address = ksym_table[i].address;
            best = ksym_table[i].name;
        }
    }
    if (base) *base = best_address;
    return best;
}

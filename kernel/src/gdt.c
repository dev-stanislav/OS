#include "gdt.h"

#define GDT_SIZE 3

static gdt_entry_t gdt_entries[GDT_SIZE];
// Accessed by load_gdt in boot/gdt_load.asm.
gdt_pointer_t gdt_pointer;

static void gdt_set_entry(int index, uint32_t base, uint32_t limit, 
                          uint8_t access, uint8_t gran) {
    gdt_entries[index].base_low = (base & 0xFFFF);
    gdt_entries[index].base_middle = (base >> 16) & 0xFF;
    gdt_entries[index].base_high = (base >> 24) & 0xFF;
    
    gdt_entries[index].limit_low = (limit & 0xFFFF);
    gdt_entries[index].granularity = (limit >> 16) & 0x0F;
    
    gdt_entries[index].granularity |= gran & 0xF0;
    gdt_entries[index].access = access;
}

void gdt_init(void) {
    gdt_pointer.limit = (sizeof(gdt_entry_t) * GDT_SIZE) - 1;
    gdt_pointer.base = (uint32_t)&gdt_entries;
    
    // NULL descriptor
    gdt_set_entry(0, 0, 0, 0, 0);
    
    // Code segment
    gdt_set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    
    // Data segment
    gdt_set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    
    load_gdt();
}

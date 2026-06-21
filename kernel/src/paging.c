#include <stdint.h>
#include "paging.h"

#define PAGE_PRESENT 0x001
#define PAGE_WRITABLE 0x002
#define PAGE_USER 0x004

static uint32_t page_directory[1024] __attribute__((aligned(4096)));
static uint32_t page_tables[4][1024] __attribute__((aligned(4096)));
static uint32_t framebuffer_table[1024] __attribute__((aligned(4096)));

void paging_init(void) {
    for (uint32_t table = 0; table < 4; table++) {
        page_directory[table] = (uint32_t)&page_tables[table] | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
        for (uint32_t page = 0; page < 1024; page++)
            page_tables[table][page] = ((table * 0x400000u) + (page * 0x1000u)) | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    }
    for (uint32_t table = 4; table < 1024; table++) page_directory[table] = 0;
    page_directory[896] = (uint32_t)framebuffer_table | PAGE_PRESENT | PAGE_WRITABLE;
    for(uint32_t page=0;page<1024;page++) framebuffer_table[page]=(0xE0000000u+page*0x1000u)|PAGE_PRESENT|PAGE_WRITABLE;
    __asm__ volatile ("mov %0, %%cr3" : : "r"(page_directory) : "memory");
    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");
}

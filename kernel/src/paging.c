#include <stdint.h>
#include "paging.h"
#include "io.h"

#define PAGE_PRESENT 0x001
#define PAGE_WRITABLE 0x002
#define PAGE_USER 0x004

static uint32_t page_directory[1024] __attribute__((aligned(4096)));
static uint32_t page_tables[4][1024] __attribute__((aligned(4096)));
static uint32_t framebuffer_table[1024] __attribute__((aligned(4096)));
static uint32_t framebuffer_address=0xE0000000u;
static uint32_t pci_read(uint8_t bus,uint8_t dev,uint8_t off){outl(0xCF8,0x80000000u|((uint32_t)bus<<16)|((uint32_t)dev<<11)|(off&0xFC));return inl(0xCFC);}

void paging_init(void) {
    for(uint16_t bus=0;bus<256;bus++)for(uint8_t dev=0;dev<32;dev++)if(pci_read((uint8_t)bus,dev,0)==0x11111234u){uint32_t bar=pci_read((uint8_t)bus,dev,0x10);if(bar&&!(bar&1))framebuffer_address=bar&0xFFFFF000u;}
    for (uint32_t table = 0; table < 4; table++) {
        page_directory[table] = (uint32_t)&page_tables[table] | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
        for (uint32_t page = 0; page < 1024; page++)
            page_tables[table][page] = ((table * 0x400000u) + (page * 0x1000u)) | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
    }
    for (uint32_t table = 4; table < 1024; table++) page_directory[table] = 0;
    page_directory[framebuffer_address>>22] = (uint32_t)framebuffer_table | PAGE_PRESENT | PAGE_WRITABLE;
    for(uint32_t page=0;page<1024;page++) framebuffer_table[page]=(framebuffer_address+page*0x1000u)|PAGE_PRESENT|PAGE_WRITABLE;
    __asm__ volatile ("mov %0, %%cr3" : : "r"(page_directory) : "memory");
    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000u;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");
}
uint32_t paging_framebuffer_address(void){return framebuffer_address;}

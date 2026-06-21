#include "disk.h"
#include "io.h"

#define ATA_DATA 0x1F0
#define ATA_SECTOR_COUNT 0x1F2
#define ATA_LBA_LOW 0x1F3
#define ATA_LBA_MID 0x1F4
#define ATA_LBA_HIGH 0x1F5
#define ATA_DRIVE 0x1F6
#define ATA_COMMAND 0x1F7
#define ATA_STATUS 0x1F7

static uint8_t ready;

static void outw(uint16_t port, uint16_t value) { __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port)); }
static uint16_t inw(uint16_t port) { uint16_t value; __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port)); return value; }

static uint8_t wait_not_busy(void) {
    for (uint32_t timeout=0; timeout<1000000; timeout++) if (!(inb(ATA_STATUS) & 0x80)) return 1;
    return 0;
}
static uint8_t wait_drq(void) {
    for (uint32_t timeout=0; timeout<1000000; timeout++) { uint8_t state=inb(ATA_STATUS); if(state&1)return 0; if(state&8)return 1; }
    return 0;
}
static void select_lba(uint32_t lba) {
    outb(ATA_DRIVE, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    outb(ATA_SECTOR_COUNT, 1); outb(ATA_LBA_LOW, (uint8_t)lba); outb(ATA_LBA_MID, (uint8_t)(lba>>8)); outb(ATA_LBA_HIGH, (uint8_t)(lba>>16));
}

void disk_init(void) { ready = wait_not_busy() && inb(ATA_STATUS) != 0xFF; }
uint8_t disk_ready(void) { return ready; }
uint8_t disk_read_sector(uint32_t lba, void *buffer) {
    if(!ready||!wait_not_busy())return 0; select_lba(lba); outb(ATA_COMMAND,0x20); if(!wait_drq())return 0;
    uint16_t *out=(uint16_t*)buffer; for(uint16_t i=0;i<256;i++)out[i]=inw(ATA_DATA); return 1;
}
uint8_t disk_write_sector(uint32_t lba, const void *buffer) {
    if(!ready||!wait_not_busy())return 0; select_lba(lba); outb(ATA_COMMAND,0x30); if(!wait_drq())return 0;
    const uint16_t *in=(const uint16_t*)buffer; for(uint16_t i=0;i<256;i++)outw(ATA_DATA,in[i]); outb(ATA_COMMAND,0xE7); return wait_not_busy();
}

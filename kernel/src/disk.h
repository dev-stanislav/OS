#ifndef DISK_H
#define DISK_H

#include <stdint.h>

void disk_init(void);
uint8_t disk_ready(void);
uint8_t disk_read_sector(uint32_t lba, void *buffer);
uint8_t disk_write_sector(uint32_t lba, const void *buffer);

#endif

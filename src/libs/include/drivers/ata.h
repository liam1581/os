#pragma once
#include <stdint.h>
#include "bool.h"

#define ATA_SECTOR_SIZE 512

bool ata_init();
bool ata_read_sector(uint32_t lba, uint8_t* buffer);
bool ata_write_sector(uint32_t lba, const uint8_t* buffer);
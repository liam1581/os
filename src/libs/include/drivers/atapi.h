#pragma once
#include <stdint.h>
#include "bool.h"

#define SECTOR_SIZE 2048

bool atapi_init();
bool atapi_read_sector(uint32_t lba, uint8_t* buffer);
#pragma once
#include "structs_and_enums/all.h"
#include <stdint.h>
#include "bool.h"

bool iso9660_init();
bool iso9660_list_dir(const char* path, struct ISO9660Dir* out);
bool iso9660_read_file(const char* path, uint8_t* buffer, uint32_t* out_size);
#pragma once

#include <stdint.h>
#include "bool.h"

#pragma pack(push, 1)

typedef struct {
    uint8_t start;
    uint32_t signature;
    uint8_t width;
    uint8_t height;
    uint8_t chars;
    uint8_t extAscii;
    uint8_t end;
} LFHFileHeader;

#pragma pack(pop)

typedef struct {
    LFHFileHeader file;
} LFHHeader;

bool validate_lfh_header(const uint8_t *data, uint32_t file_size, LFHHeader* header);
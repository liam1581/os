#pragma once
#include <stdint.h>
#include "bool.h"

#pragma pack(push, 1)

typedef struct {
    uint8_t start;
    uint32_t signature;
    uint8_t reserved1;
    uint8_t reserved2;
    uint8_t end;
} LEHFileHeader;

#pragma pack(pop)

typedef struct {
    LEHFileHeader file;
} LEHHeader;

bool validate_leh_header(const uint8_t* data, uint32_t file_size, LEHHeader* header);

int leh_exec(const char* path);
int leh_exec_from(const char* path, bool useFat);
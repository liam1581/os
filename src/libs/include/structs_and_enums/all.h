#pragma once
#include <stdint.h>
#include "bool.h"

enum {
	KEYBOARD_EVENT_TYPE_MAKE = 0,
	KEYBOARD_EVENT_TYPE_BREAK = 1,
};
struct KeyboardEvent {
	uint8_t type;
	uint16_t code;
};

#define FAT32_MAX_FILENAME  256
#define FAT32_MAX_CHILDREN  128
struct FAT32Entry {
    char     name[FAT32_MAX_FILENAME];
    bool     is_directory;
    uint32_t cluster;
    uint32_t size;
};
struct FAT32Dir {
    struct FAT32Entry entries[FAT32_MAX_CHILDREN];
    uint32_t count;
};

#define ISO9660_MAX_FILENAME  32
#define ISO9660_MAX_CHILDREN  64
struct ISO9660Entry {
    char     name[ISO9660_MAX_FILENAME];
    bool     is_directory;
    uint32_t lba;       // starting sector
    uint32_t size;      // file size in bytes
};
struct ISO9660Dir {
    struct ISO9660Entry entries[ISO9660_MAX_CHILDREN];
    uint32_t count;
};

enum {
    PRINT_COLOR_BLACK = 0,
	PRINT_COLOR_BLUE = 1,
	PRINT_COLOR_GREEN = 2,
	PRINT_COLOR_CYAN = 3,
	PRINT_COLOR_RED = 4,
	PRINT_COLOR_MAGENTA = 5,
	PRINT_COLOR_BROWN = 6,
	PRINT_COLOR_LIGHT_GRAY = 7,
	PRINT_COLOR_DARK_GRAY = 8,
	PRINT_COLOR_LIGHT_BLUE = 9,
	PRINT_COLOR_LIGHT_GREEN = 10,
	PRINT_COLOR_LIGHT_CYAN = 11,
	PRINT_COLOR_LIGHT_RED = 12,
	PRINT_COLOR_PINK = 13,
	PRINT_COLOR_YELLOW = 14,
	PRINT_COLOR_WHITE = 15,
};
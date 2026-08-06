#pragma once

// FAT32
#define FAT32_MAX_FILENAME  256
#define FAT32_MAX_CHILDREN  128

#define FAT32_ATTR_READ_ONLY 0x01
#define FAT32_ATTR_HIDDEN    0x02
#define FAT32_ATTR_SYSTEM    0x04
#define FAT32_ATTR_VOLUME_ID 0x08
#define FAT32_ATTR_DIRECTORY 0x10
#define FAT32_ATTR_ARCHIVE   0x20
#define FAT32_ATTR_LFN       0x0F

// ISO9660
#define ISO9660_MAX_FILENAME  32
#define ISO9660_MAX_CHILDREN  64

// Print
#define KAPI_PRINT_COLOR_BLACK 0
#define KAPI_PRINT_COLOR_BLUE 1
#define KAPI_PRINT_COLOR_GREEN 2
#define KAPI_PRINT_COLOR_CYAN 3
#define KAPI_PRINT_COLOR_RED 4
#define KAPI_PRINT_COLOR_MAGENTA 5
#define KAPI_PRINT_COLOR_BROWN 6
#define KAPI_PRINT_COLOR_LIGHT_GRAY 7
#define KAPI_PRINT_COLOR_DARK_GRAY 8
#define KAPI_PRINT_COLOR_LIGHT_BLUE 9
#define KAPI_PRINT_COLOR_LIGHT_GREEN 10
#define KAPI_PRINT_COLOR_LIGHT_CYAN 11
#define KAPI_PRINT_COLOR_LIGHT_RED 12
#define KAPI_PRINT_COLOR_PINK 13
#define KAPI_PRINT_COLOR_YELLOW 14
#define KAPI_PRINT_COLOR_WHITE 15





#define STR(name, value) char name[] = value
#define INT(name, value) int name = value
#define BOOL(name, value) bool name = value;
#define FLOAT(name, value) float name = value;
#define CHAR(name, value) char name = value;
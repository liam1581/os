#pragma once
#include "structs_and_enums/all.h"
#include <stdint.h>
#include "bool.h"

#define FAT32_MAX_FILENAME  256
#define FAT32_MAX_CHILDREN  128

#define FAT32_ATTR_READ_ONLY 0x01
#define FAT32_ATTR_HIDDEN    0x02
#define FAT32_ATTR_SYSTEM    0x04
#define FAT32_ATTR_VOLUME_ID 0x08
#define FAT32_ATTR_DIRECTORY 0x10
#define FAT32_ATTR_ARCHIVE   0x20
#define FAT32_ATTR_LFN       0x0F

// Init
bool fat32_init();

// Read
bool fat32_list_dir(const char* path, struct FAT32Dir* out);
bool fat32_read_file(const char* path, uint8_t* buffer, uint32_t* out_size);

// Write
bool fat32_create_file(const char* path);
bool fat32_create_dir(const char* path);
bool fat32_write_file(const char* path, const uint8_t* buffer, uint32_t size);
bool fat32_rename(const char* path, const char* new_name);
bool fat32_copy_file(const char* src, const char* dest);
bool fat32_move_file(const char* src, const char* dest);
bool fat32_delete_file(const char* path);
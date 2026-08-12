#pragma once

extern "C" {
    #include "stddef.h"
    #include "string.h"
}

#pragma pack(push, 1)

typedef struct {
    uint16_t signature;
    uint32_t file_size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t pixel_offset;
} BMPFileHeader;

typedef struct {
    uint32_t header_size;
    int32_t  width;
    int32_t  height;
    uint16_t planes;
    uint16_t bits_per_pixel;
    uint32_t compression;
    uint32_t image_size;
    int32_t  x_pixels_per_meter;
    int32_t  y_pixels_per_meter;
    uint32_t colors_used;
    uint32_t important_colors;
} BMPInfoHeader;

#pragma pack(pop)

typedef struct {
    BMPFileHeader file;
    BMPInfoHeader info;
} BMPHeader;


bool validate_bmp_header(const uint8_t *data, uint32_t file_size, BMPHeader *header);

void bmpTestingMain();
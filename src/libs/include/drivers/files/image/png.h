#pragma once

#include <stdint.h>
#include "bool.h"

#pragma pack(push, 1)

typedef struct {
    uint32_t width;
    uint32_t height;
    uint8_t  bit_depth;     // 1, 2, 4, 8, or 16 (which are valid depends on color_type)
    uint8_t  color_type;    // 0=grayscale, 2=RGB, 3=palette, 4=grayscale+alpha, 6=RGBA
    uint8_t  compression;   // must be 0
    uint8_t  filter_method; // must be 0
    uint8_t  interlace;     // must be 0 (no Adam7)

    uint8_t  palette[256][3];   // PLTE entries, RGB
    uint8_t  palette_alpha[256]; // tRNS alpha per entry; defaults to 255 (opaque) if tRNS absent
    uint16_t palette_count;      // number of valid entries in palette[]/palette_alpha[]
} PNGFileHeader;

#pragma pack(pop)

typedef struct {
    PNGFileHeader file;
} PNGHeader;

bool png_has_alpha(const PNGHeader* header);

static inline uint8_t png_output_bytes_per_pixel(const PNGHeader* header) {
    return png_has_alpha(header) ? 4 : 3;
}

bool validate_png_header(const uint8_t* data, uint32_t file_size, PNGHeader* header);

bool png_decode(const uint8_t* data, uint32_t file_size, const PNGHeader* header, uint8_t** out_pixels, uint32_t* out_size);
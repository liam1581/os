#pragma once

#include <stdint.h>
#include "bool.h"

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Initialize the framebuffer from the Multiboot2 information
 * structure supplied by GRUB.
 *
 * Returns true if an RGB framebuffer was found.
 */
bool framebuffer_init(uint64_t multiboot_info_addr);


/*
 * Returns whether a usable framebuffer was found.
 */
bool framebuffer_is_available(void);


/*
 * Get framebuffer information.
 */
uint64_t framebuffer_get_address(void);
uint32_t framebuffer_get_width(void);
uint32_t framebuffer_get_height(void);
uint32_t framebuffer_get_pitch(void);
uint8_t framebuffer_get_bpp(void);


/*
 * Draw one RGB pixel.
 *
 * x/y are screen coordinates.
 *
 * r/g/b are standard 0-255 RGB values.
 */
void framebuffer_put_pixel(
    uint32_t x,
    uint32_t y,
    uint8_t r,
    uint8_t g,
    uint8_t b
);


/*
 * Draw a BMP image.
 *
 * pixels:
 *     Pointer to the BMP pixel data returned/located by
 *     your BMP header validation code.
 *
 * start_x/start_y:
 *     Position on the screen where the top-left corner
 *     of the image should be drawn.
 *
 * width/height:
 *     Dimensions of the BMP image.
 *
 * This expects a validated 24-bit uncompressed BMP.
 *
 * BMP pixels are BGR rather than RGB, and rows are padded
 * to a 4-byte boundary. This function handles both.
 *
 * A positive height means a normal bottom-up BMP.
 * A negative height means a top-down BMP.
 */
bool framebuffer_draw_bmp(
    const uint8_t* pixels,
    uint32_t start_x,
    uint32_t start_y,
    uint32_t width,
    int32_t height
);


/*
 * Draw a decoded PNG image.
 *
 * pixels:
 *     Pointer to the buffer returned by png_decode() -- already
 *     top-down, tightly packed (no row padding), RGB or RGBA
 *     (per has_alpha) in that byte order (unlike BMP, no BGR swap
 *     needed).
 *
 * start_x/start_y:
 *     Position on the screen where the top-left corner
 *     of the image should be drawn.
 *
 * width/height:
 *     Dimensions of the decoded image (PNGHeader::width/height).
 *
 * has_alpha:
 *     true if pixels are 4 bytes/pixel RGBA (PNG color type 6),
 *     false if 3 bytes/pixel RGB (color type 2). Pass
 *     png_has_alpha(header) from png.h.
 *
 * Pixels with alpha == 0 are treated as fully transparent and left
 * untouched on screen; any other alpha value is drawn fully opaque
 * (this is a simple binary test, not real alpha blending).
 */
bool framebuffer_draw_png(
    const uint8_t* pixels,
    uint32_t start_x,
    uint32_t start_y,
    uint32_t width,
    uint32_t height,
    bool has_alpha
);


/*
 * Clear the entire framebuffer.
 */
void framebuffer_clear(
    uint8_t r,
    uint8_t g,
    uint8_t b
);


#ifdef __cplusplus
}
#endif
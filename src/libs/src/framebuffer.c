#include "video/framebuffer.h"

#include "print.h"

#include "multiboot2.h"
#include "debug.h"

#include <stddef.h>


static uint8_t* framebuffer = NULL;

static uint32_t framebuffer_width = 0;
static uint32_t framebuffer_height = 0;
static uint32_t framebuffer_pitch = 0;
static uint8_t framebuffer_bpp = 0;

static uint8_t red_position = 0;
static uint8_t red_mask_size = 0;

static uint8_t green_position = 0;
static uint8_t green_mask_size = 0;

static uint8_t blue_position = 0;
static uint8_t blue_mask_size = 0;

static bool framebuffer_available = false;


/*
 * Convert a normal 8-bit RGB component to the number
 * of bits available in the framebuffer.
 *
 * Example:
 *
 *     8-bit value -> 5-bit value
 *
 *     0   -> 0
 *     128 -> 16
 *     255 -> 31
 */
static uint32_t scale_color(
    uint8_t value,
    uint8_t mask_size
) {
    if (mask_size == 0)
        return 0;

    if (mask_size >= 8)
        return value;

    uint32_t max_value =
        (1u << mask_size) - 1u;

    return ((uint32_t)value * max_value + 127u) / 255u;
}


/*
 * Convert RGB values into the framebuffer's actual
 * pixel format.
 */
static uint32_t make_pixel(
    uint8_t r,
    uint8_t g,
    uint8_t b
) {
    uint32_t pixel = 0;

    uint32_t red =
        scale_color(r, red_mask_size);

    uint32_t green =
        scale_color(g, green_mask_size);

    uint32_t blue =
        scale_color(b, blue_mask_size);

    pixel |= red << red_position;
    pixel |= green << green_position;
    pixel |= blue << blue_position;

    return pixel;
}


/*
 * Initialize the framebuffer from GRUB's Multiboot2
 * framebuffer tag.
 */
bool framebuffer_init(uint64_t multiboot_info_addr)
{
    struct multiboot_tag_framebuffer_rgb* fb =
        multiboot2_find_framebuffer_rgb(
            multiboot_info_addr
        );

    if (fb == NULL) {
        DBG_PRINTLNS(
            "Framebuffer: no RGB framebuffer found"
        );

        framebuffer_available = false;
        return false;
    }


    framebuffer =
        (uint8_t*)(uintptr_t)fb->framebuffer_addr;

    framebuffer_width =
        fb->framebuffer_width;

    framebuffer_height =
        fb->framebuffer_height;

    framebuffer_pitch =
        fb->framebuffer_pitch;

    framebuffer_bpp =
        fb->framebuffer_bpp;


    /*
     * Save the RGB component layout supplied by GRUB.
     */
    red_position =
        (uint8_t)fb->framebuffer_red_field_position;

    red_mask_size =
        (uint8_t)fb->framebuffer_red_mask_size;

    green_position =
        (uint8_t)fb->framebuffer_green_field_position;

    green_mask_size =
        (uint8_t)fb->framebuffer_green_mask_size;

    blue_position =
        (uint8_t)fb->framebuffer_blue_field_position;

    blue_mask_size =
        (uint8_t)fb->framebuffer_blue_mask_size;


    /*
     * We currently support these common RGB formats:
     *
     *     16 bpp
     *     24 bpp
     *     32 bpp
     */
    if (framebuffer_bpp != 16 &&
        framebuffer_bpp != 24 &&
        framebuffer_bpp != 32) {

        DBG_PRINTLNS(
            "Framebuffer: unsupported BPP"
        );

        framebuffer_available = false;
        return false;
    }


    framebuffer_available = true;

    DBG_PRINTLNS(
        "Framebuffer initialized"
    );

    return true;
}


/*
 * Check whether the framebuffer is available.
 */
bool framebuffer_is_available(void)
{
    return framebuffer_available;
}


/*
 * Framebuffer information getters.
 */
uint64_t framebuffer_get_address(void)
{
    return (uint64_t)(uintptr_t)framebuffer;
}

uint32_t framebuffer_get_width(void)
{
    return framebuffer_width;
}

uint32_t framebuffer_get_height(void)
{
    return framebuffer_height;
}

uint32_t framebuffer_get_pitch(void)
{
    return framebuffer_pitch;
}

uint8_t framebuffer_get_bpp(void)
{
    return framebuffer_bpp;
}


/*
 * Draw one RGB pixel.
 */
void framebuffer_put_pixel(
    uint32_t x,
    uint32_t y,
    uint8_t r,
    uint8_t g,
    uint8_t b
) {
    if (!framebuffer_available)
        return;

    /*
     * Don't write outside the framebuffer.
     */
    if (x >= framebuffer_width ||
        y >= framebuffer_height) {

        return;
    }


    /*
     * Convert RGB into the framebuffer's format.
     */
    uint32_t pixel =
        make_pixel(r, g, b);


    /*
     * Calculate bytes per pixel.
     */
    uint32_t bytes_per_pixel =
        framebuffer_bpp / 8;


    /*
     * Calculate the address of the pixel.
     *
     * pitch is used instead of width because framebuffer
     * rows can contain padding.
     */
    uint8_t* address =
        framebuffer +
        (y * framebuffer_pitch) +
        (x * bytes_per_pixel);


    /*
     * Write the pixel byte-by-byte.
     *
     * x86 is little-endian.
     */
    for (uint32_t i = 0;
         i < bytes_per_pixel;
         i++) {

        address[i] =
            (uint8_t)((pixel >> (i * 8)) & 0xFF);
    }
}


/*
 * Draw a 24-bit uncompressed BMP.
 */
bool framebuffer_draw_bmp(
    const uint8_t* pixels,
    uint32_t start_x,
    uint32_t start_y,
    uint32_t width,
    int32_t height
) {
    if (!framebuffer_available) {
        println("FRAMEBUFFER UNAVAILABLE");
        return false;
    }

    if (pixels == NULL) {
        println("PIXEL DATA IS NULL");
        return false;
    }

    if (width == 0 || height == 0) {
        println("WIDTH OR HEIGHT IS ZERO");
        return false;
    }


    /*
     * BMP 24-bit pixels use 3 bytes:
     *
     *     B G R
     *
     * Each row is padded to a multiple of 4 bytes.
     */
    uint32_t row_size =
        (width * 3u + 3u) & ~3u;


    /*
     * BMP height has a special meaning:
     *
     *     positive = bottom-up
     *     negative = top-down
     */
    bool top_down = (height < 0);

    uint32_t image_height;

    if (height < 0) {
        image_height =
            (uint32_t)(-(int64_t)height);
    } else {
        image_height =
            (uint32_t)height;
    }


    /*
     * Go through every output row.
     */
    for (uint32_t y = 0;
         y < image_height;
         y++) {

        /*
         * Find the corresponding row in the BMP.
         *
         * Normal BMP:
         *
         *     first row in file = bottom row
         *
         * Top-down BMP:
         *
         *     first row in file = top row
         */
        uint32_t bmp_y;

        if (top_down) {
            bmp_y = y;
        } else {
            bmp_y =
                image_height - 1u - y;
        }


        /*
         * Get the beginning of this BMP row.
         */
        const uint8_t* row =
            pixels + (bmp_y * row_size);


        /*
         * Go through every pixel in the row.
         */
        for (uint32_t x = 0;
             x < width;
             x++) {

            /*
             * Each pixel is 3 bytes:
             *
             *     [B] [G] [R]
             */
            const uint8_t* pixel =
                row + (x * 3u);


            uint8_t b = pixel[0];
            uint8_t g = pixel[1];
            uint8_t r = pixel[2];


            /*
             * Let framebuffer_put_pixel() deal with
             * the actual framebuffer pixel format.
             */
            framebuffer_put_pixel(
                start_x + x,
                start_y + y,
                r,
                g,
                b
            );
        }
    }


    return true;
}


/*
 * Draw a decoded PNG image. See framebuffer.h for details.
 */
bool framebuffer_draw_png(
    const uint8_t* pixels,
    uint32_t start_x,
    uint32_t start_y,
    uint32_t width,
    uint32_t height,
    bool has_alpha
) {
    if (!framebuffer_available) {
        println("FRAMEBUFFER UNAVAILABLE");
        return false;
    }

    if (pixels == NULL) {
        println("PIXEL DATA IS NULL");
        return false;
    }

    if (width == 0 || height == 0) {
        println("WIDTH OR HEIGHT IS 0");
        return false;
    }

    uint32_t bpp = has_alpha ? 4u : 3u;

    /*
     * Unlike BMP, png_decode() already hands back tightly packed,
     * top-down RGB(A) rows -- no padding, no bottom-up flip, no BGR
     * swap needed.
     */
    for (uint32_t y = 0; y < height; y++) {
        const uint8_t* row = pixels + ((uint64_t)y * width * bpp);

        for (uint32_t x = 0; x < width; x++) {
            const uint8_t* pixel = row + (x * bpp);

            uint8_t r = pixel[0];
            uint8_t g = pixel[1];
            uint8_t b = pixel[2];

            /*
             * Simple binary transparency: skip fully-transparent
             * pixels, draw everything else fully opaque. This is not
             * real alpha blending.
             */
            if (has_alpha && pixel[3] == 0)
                continue;

            framebuffer_put_pixel(
                start_x + x,
                start_y + y,
                r,
                g,
                b
            );
        }
    }

    return true;
}


/*
 * Clear the framebuffer.
 */
void framebuffer_clear(
    uint8_t r,
    uint8_t g,
    uint8_t b
) {
    if (!framebuffer_available)
        return;

    for (uint32_t y = 0;
         y < framebuffer_height;
         y++) {

        for (uint32_t x = 0;
             x < framebuffer_width;
             x++) {

            framebuffer_put_pixel(
                x,
                y,
                r,
                g,
                b
            );
        }
    }
}
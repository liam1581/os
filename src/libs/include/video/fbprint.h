#pragma once

#include <stdint.h>

#include "structs_and_enums/all.h"

/*
 * Initialize the framebuffer console using the Multiboot2
 * information structure passed by GRUB.
 *
 * Returns 1 on success, 0 if no RGB framebuffer was found.
 */
int fbprint_init(uint64_t multiboot_info_addr);

/*
 * Draw one RGB pixel.
 */
void fb_put_pixel(
    uint32_t x,
    uint32_t y,
    uint8_t r,
    uint8_t g,
    uint8_t b
);

/*
 * Clear the framebuffer.
 */
void fbclear(void);

/*
 * Set foreground/background colors using the same
 * 0-15 color values as the normal VGA print system.
 */
void fbprint_set_color(
    uint8_t foreground,
    uint8_t background
);

/*
 * Character and string output.
 */
void fbprintc(char character);
void fbprint(const char* str);
void fbprint_constant(const char* str);
void fbprintln(const char* str);

/*
 * Formatted printing.
 *
 * Supported:
 *
 * %s  string
 * %c  character
 * %d  signed decimal
 * %u  unsigned decimal
 * %x  hexadecimal lowercase
 * %X  hexadecimal uppercase
 * %b  binary
 * %%  literal %
 */
void fbprintf(const char* fmt, ...);

/*
 * Integer output.
 */
void fbprint_uint64_dec(uint64_t value);
void fbprint_uint64_hex(uint64_t value);
void fbprint_uint64_bin(uint64_t value);

/*
 * Character deletion.
 */
void fbdelete_last_char(void);

/*
 * Cursor functions.
 *
 * row/col refer to CHARACTER cells, not pixels.
 */
void fbmove_cursor(int row, int col);
void fbmove_cursor_up(void);
void fbmove_cursor_down(void);
void fbmove_cursor_left(void);
void fbmove_cursor_right(void);
void fbmove_cursor_to_start(void);
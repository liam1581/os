#pragma once

#include <stdint.h>

#include "structs_and_enums/all.h"

int fbprint_init(uint64_t multiboot_info_addr);

void fb_put_pixel(uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b);

void fbclear();

void fbprint_set_color(uint8_t foreground, uint8_t background);

void fbprintc(char character);
void fbprint(const char* str);
void fbprint_constant(const char* str);
void fbprintln(const char* str);

void fbprint_eachChar();

void fbprintf(const char* fmt, ...);

void fbprint_uint64_dec(uint64_t value);
void fbprint_uint64_hex(uint64_t value);
void fbprint_uint64_bin(uint64_t value);

void fbdelete_last_char();

void fbmove_cursor(int row, int col);
void fbmove_cursor_up();
void fbmove_cursor_down();
void fbmove_cursor_left();
void fbmove_cursor_right();
void fbmove_cursor_to_start();
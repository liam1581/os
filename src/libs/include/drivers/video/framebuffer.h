#pragma once

#include <stdint.h>
#include "bool.h"

bool framebuffer_init(uint64_t multiboot_info_addr);

bool framebuffer_is_available();

uint64_t framebuffer_get_address();
uint32_t framebuffer_get_width();
uint32_t framebuffer_get_height();
uint32_t framebuffer_get_pitch();
uint8_t framebuffer_get_bpp();

void framebuffer_put_pixel(uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b);

bool framebuffer_draw_bmp(const uint8_t* pixels, uint32_t start_x, uint32_t start_y, uint32_t width, int32_t height);
bool framebuffer_draw_png(const uint8_t* pixels, uint32_t start_x, uint32_t start_y, uint32_t width, uint32_t height, bool has_alpha);

void framebuffer_clear(uint8_t r, uint8_t g, uint8_t b);
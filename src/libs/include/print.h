#pragma once
#include "structs_and_enums/all.h"
#include <stdint.h>
#include <stddef.h>

void clear_screen();
void printc(char character);
void print(const char* string);
void print_constant(const char* string);
void println(const char* string);
void printf(char* fmt, ...);
void print_set_color(uint8_t foreground, uint8_t background);
void print_uint64_dec(uint64_t value);
void print_uint64_hex(uint64_t value);
void print_uint64_bin(uint64_t value);
void delete_last_char();

void printf(char* fmt, ...);

void move_cursor(int row, int col);
void move_cursor_up();
void move_cursor_down();
void move_cursor_left();
void move_cursor_right();
void move_cursor_to_start();
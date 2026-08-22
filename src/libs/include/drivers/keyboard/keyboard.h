#pragma once
#include "structs_and_enums/all.h"
#include "drivers/keyboard/keycodes.h"
#include <stdint.h>

#include "bool.h"

void keyboard_init();
void keyboard_set_handler(void (*handler)(struct KeyboardEvent event));
bool keyboard_is_down(uint16_t code);
bool keyboard_is_up(uint16_t code);

uint8_t keycode_to_ascii_ext(uint16_t code, bool shift_pressed, bool altgr_pressed);
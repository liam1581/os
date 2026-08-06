#pragma once

extern "C" {
    #include "drivers/keyboard.h"
}

void keyboard_buffer_clear();
void handle_input(struct KeyboardEvent event);
void print_ascii(uint16_t code, bool shift, bool altgr);
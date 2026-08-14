#pragma once

extern "C" {
    #include "drivers/keyboard/keyboard.h"
}

void testing_keyboard_buffer_clear();
void testing_handle_input(struct KeyboardEvent event);
void testing_print_ascii(uint16_t code, bool shift, bool altgr);
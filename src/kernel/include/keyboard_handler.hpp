#pragma once

#include "commands.hpp"

extern "C" {
    #include "drivers/keyboard/keyboard.h"
}

extern Commands commands;

void keyboard_buffer_clear();
void handle_input(struct KeyboardEvent event);
void print_ascii(uint16_t code, bool shift, bool altgr);
#include "testing_keyboard_handler.hpp"

#include "command_functions.hpp"

extern "C" {
    #include "print.h"
    #include "string.h"

    #include "drivers/power.h"
}

bool testing_capsLockActive = false;
bool testing_cmdMode = true;

size_t testing_keyboard_buffer_length = 0;
size_t testing_keyboard_buffer_index = 0;
char testing_keyboard_buffer[1024];

void testing_keyboard_buffer_clear() {
    testing_keyboard_buffer_length = 0;
    testing_keyboard_buffer_index = 0;
    for (size_t i = 0; i < 1024; i++) testing_keyboard_buffer[i] = '\0';
}

void testing_handle_input(struct KeyboardEvent event) {
    if (event.type == KEYBOARD_EVENT_TYPE_MAKE) {
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);

        if (event.code == KEY_CODE_CAPS_LOCK) {
            testing_capsLockActive = !testing_capsLockActive;
            return;
        } else if (event.code == KEY_CODE_BACKSPACE) {
            if (testing_keyboard_buffer_index == 0) return;

            for (size_t i = testing_keyboard_buffer_index - 1; i < testing_keyboard_buffer_length - 1; i++) {
                testing_keyboard_buffer[i] = testing_keyboard_buffer[i + 1];
            }
            testing_keyboard_buffer[testing_keyboard_buffer_length - 1] = '\0';
            testing_keyboard_buffer_length--;
            testing_keyboard_buffer_index--;

            delete_last_char();
            return;
        } else if (!testing_cmdMode && event.code == KEY_CODE_ARROW_KEY_UP) {
            move_cursor_up();
            return;
        } else if (!testing_cmdMode && event.code == KEY_CODE_ARROW_KEY_DOWN) {
            move_cursor_down();
            return;
        } else if (event.code == KEY_CODE_ARROW_KEY_LEFT) {
            if (testing_keyboard_buffer_index == 0) return;
            testing_keyboard_buffer_index--;
            move_cursor_left();
            return;
        } else if (event.code == KEY_CODE_ARROW_KEY_RIGHT) {
            if (testing_keyboard_buffer_index >= testing_keyboard_buffer_length) return;
            testing_keyboard_buffer_index++;
            move_cursor_right();
            return;
        } else if (event.code == KEY_CODE_ENTER) {
            testing_keyboard_buffer[testing_keyboard_buffer_length] = '\0';
            printc('\n');

            if (testing_cmdMode) {                
                // print("D:");
                // print(get_current_dir());
                print("> ");
                testing_keyboard_buffer_clear();
            }
            return;
        }

        bool testing_shift = keyboard_is_down(KEY_CODE_SHIFT_LEFT) || keyboard_is_down(KEY_CODE_SHIFT_RIGHT) || testing_capsLockActive;
        bool testing_altgr = keyboard_is_down(KEY_CODE_ALT_RIGHT);
        bool testing_ctrl = keyboard_is_down(KEY_CODE_CTRL_LEFT) || keyboard_is_down(KEY_CODE_CTRL_RIGHT);
        bool testing_alt = keyboard_is_down(KEY_CODE_ALT_LEFT);
        bool testing_menukey = keyboard_is_down(KEY_CODE_MENU_KEY);

        if (testing_ctrl && testing_alt && event.code == KEY_CODE_E) {
            testing_cmdMode = !testing_cmdMode;
            print_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLACK);
            print("\nEntered ");
            print(testing_cmdMode ? "command mode, " : "text input mode, ");
            print("Press Ctrl + Alt + E again to return to ");
            println(testing_cmdMode ? "text input mode." : "command mode.");
            print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
            print(testing_cmdMode ? "D:" : "");
            print(testing_cmdMode ? get_current_dir() : "");
            print(testing_cmdMode ? "> " : "");
            testing_keyboard_buffer_clear();
            return;
        } else if (testing_ctrl && testing_alt && event.code == KEY_CODE_C) {
            clear_screen();
            print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
            move_cursor_to_start();
            print(testing_cmdMode ? "D:" : "");
            print(testing_cmdMode ? get_current_dir() : "");
            print(testing_cmdMode ? "> " : "");
            return;
        } else if (testing_ctrl && testing_alt && event.code == KEY_CODE_R) {
            arch_restart();
            return;
        } else if (testing_ctrl && testing_alt && event.code == KEY_CODE_S) {
            arch_shutdown();
            return;
        }

        testing_print_ascii(event.code, testing_shift, testing_altgr);

        
    } else if (event.type == KEYBOARD_EVENT_TYPE_BREAK) {
        // Handle key release if needed
    }
}

void testing_print_ascii(uint16_t code, bool shift, bool altgr) {
    char ascii = keycode_to_ascii_ext(code, shift, altgr);

    if (ascii == '\0') return;

    for (size_t i = testing_keyboard_buffer_length; i > testing_keyboard_buffer_index; i--) {
        testing_keyboard_buffer[i] = testing_keyboard_buffer[i - 1];
    }
    testing_keyboard_buffer[testing_keyboard_buffer_index] = ascii;
    testing_keyboard_buffer_length++;
    testing_keyboard_buffer_index++;

    for (size_t i = testing_keyboard_buffer_index - 1; i < testing_keyboard_buffer_length; i++) {
        printc(testing_keyboard_buffer[i]);
    }
    for (size_t i = testing_keyboard_buffer_index; i < testing_keyboard_buffer_length; i++) {
        move_cursor_left();
    }
}
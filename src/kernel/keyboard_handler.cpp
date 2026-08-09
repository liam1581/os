#include "keyboard_handler.hpp"

#include "commands.hpp"
#include "command_functions.hpp"

extern "C" {
    #include "print.h"
    #include "drivers/power.h"
}

bool capsLockActive = false;
bool cmdMode = true;

extern Commands commands;

void print_shell_prefix() {
    printc(get_current_drive());
    print(":");
    print(get_current_path());
}

size_t keyboard_buffer_length = 0;
size_t keyboard_buffer_index = 0;
char keyboard_buffer[1024];

void keyboard_buffer_clear() {
    keyboard_buffer_length = 0;
    keyboard_buffer_index = 0;
    for (size_t i = 0; i < 1024; i++) keyboard_buffer[i] = '\0';
}

void handle_input(struct KeyboardEvent event) {
    if (event.type == KEYBOARD_EVENT_TYPE_MAKE) {
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);

        if (event.code == KEY_CODE_CAPS_LOCK) {
            capsLockActive = !capsLockActive;
            return;
        } else if (event.code == KEY_CODE_BACKSPACE) {
            if (keyboard_buffer_index == 0) return;

            for (size_t i = keyboard_buffer_index - 1; i < keyboard_buffer_length - 1; i++) {
                keyboard_buffer[i] = keyboard_buffer[i + 1];
            }
            keyboard_buffer[keyboard_buffer_length - 1] = '\0';
            keyboard_buffer_length--;
            keyboard_buffer_index--;

            delete_last_char();
            return;
        } else if (!cmdMode && event.code == KEY_CODE_ARROW_KEY_UP) {
            move_cursor_up();
            return;
        } else if (!cmdMode && event.code == KEY_CODE_ARROW_KEY_DOWN) {
            move_cursor_down();
            return;
        } else if (event.code == KEY_CODE_ARROW_KEY_LEFT) {
            if (keyboard_buffer_index == 0) return;
            keyboard_buffer_index--;
            move_cursor_left();
            return;
        } else if (event.code == KEY_CODE_ARROW_KEY_RIGHT) {
            if (keyboard_buffer_index >= keyboard_buffer_length) return;
            keyboard_buffer_index++;
            move_cursor_right();
            return;
        } else if (event.code == KEY_CODE_ENTER) {
            keyboard_buffer[keyboard_buffer_length] = '\0';
            printc('\n');

            if (cmdMode) {
                commands.handleCommand(keyboard_buffer);

                print_shell_prefix();
                print("> ");
                keyboard_buffer_clear();
            }
            return;
        }

        bool shift = keyboard_is_down(KEY_CODE_SHIFT_LEFT) || keyboard_is_down(KEY_CODE_SHIFT_RIGHT) || capsLockActive;
        bool altgr = keyboard_is_down(KEY_CODE_ALT_RIGHT);
        bool ctrl = keyboard_is_down(KEY_CODE_CTRL_LEFT) || keyboard_is_down(KEY_CODE_CTRL_RIGHT);
        bool alt = keyboard_is_down(KEY_CODE_ALT_LEFT);
        bool menukey = keyboard_is_down(KEY_CODE_MENU_KEY);

        if (ctrl && alt && event.code == KEY_CODE_E) {
            cmdMode = !cmdMode;
            print_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLACK);
            print("\nEntered ");
            print(cmdMode ? "command mode, " : "text input mode, ");
            print("Press Ctrl + Alt + E again to return to ");
            println(cmdMode ? "text input mode." : "command mode.");
            print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
            print(cmdMode ? "D:" : "");
            print(cmdMode ? get_current_dir() : "");
            print(cmdMode ? "> " : "");
            keyboard_buffer_clear();
            return;
        } else if (ctrl && alt && event.code == KEY_CODE_C) {
            clear_screen();
            print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
            move_cursor_to_start();
            print(cmdMode ? "D:" : "");
            print(cmdMode ? get_current_dir() : "");
            print(cmdMode ? "> " : "");
            return;
        } else if (ctrl && alt && event.code == KEY_CODE_R) {
            arch_restart();
            return;
        } else if (ctrl && alt && event.code == KEY_CODE_S) {
            arch_shutdown();
            return;
        }

        print_ascii(event.code, shift, altgr);

        
    } else if (event.type == KEYBOARD_EVENT_TYPE_BREAK) {
        // Handle key release if needed
    }
}

void print_ascii(uint16_t code, bool shift, bool altgr) {
    char ascii = keycode_to_ascii_ext(code, shift, altgr);

    if (ascii == '\0') return;

    for (size_t i = keyboard_buffer_length; i > keyboard_buffer_index; i--) {
        keyboard_buffer[i] = keyboard_buffer[i - 1];
    }
    keyboard_buffer[keyboard_buffer_index] = ascii;
    keyboard_buffer_length++;
    keyboard_buffer_index++;

    for (size_t i = keyboard_buffer_index - 1; i < keyboard_buffer_length; i++) {
        printc(keyboard_buffer[i]);
    }
    for (size_t i = keyboard_buffer_index; i < keyboard_buffer_length; i++) {
        move_cursor_left();
    }
}
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

#define COMMAND_HISTORY_CAPACITY 32
#define COMMAND_MAX_LENGTH 1024

// Ring buffer of past commands, most-recent-overwrites-oldest once full.
char commandHistory[COMMAND_HISTORY_CAPACITY][COMMAND_MAX_LENGTH];
size_t commandHistoryLengths[COMMAND_HISTORY_CAPACITY];
size_t commandHistoryCount = 0; // total commands ever pushed (can exceed capacity)

// The line the user was actively typing before they started browsing
// history with the up arrow -- restored when they arrow back down past
// the most recent history entry.
char pendingCommand[COMMAND_MAX_LENGTH];
size_t pendingCommandLength = 0;

// How many commands back from "now" we're currently viewing: 0 means
// not browsing (editing the live/pending line), 1 means the most
// recently run command, 2 the one before that, etc.
size_t historyBrowseOffset = 0;

void keyboard_buffer_clear() {
    keyboard_buffer_length = 0;
    keyboard_buffer_index = 0;
    for (size_t i = 0; i < 1024; i++) keyboard_buffer[i] = '\0';
}

// Replaces the current line's contents, on screen and in the typing
// buffer, with the given text -- shared by every history-browsing
// path below.
void set_buffer_contents(const char* text, size_t length) {
    // Move the screen cursor to the end of the current line...
    for (size_t i = keyboard_buffer_index; i < keyboard_buffer_length; i++) {
        move_cursor_right();
    }
    // ...then erase it, character by character.
    for (size_t i = 0; i < keyboard_buffer_length; i++) {
        delete_last_char();
    }

    for (size_t i = 0; i < length; i++) {
        keyboard_buffer[i] = text[i];
        printc(text[i]);
    }
    keyboard_buffer[length] = '\0';
    keyboard_buffer_length = length;
    keyboard_buffer_index = length;
}

void history_browse_older() {
    size_t available = commandHistoryCount < COMMAND_HISTORY_CAPACITY ? commandHistoryCount : COMMAND_HISTORY_CAPACITY;
    if (historyBrowseOffset >= available) return; // already showing the oldest stored command

    if (historyBrowseOffset == 0) {
        // Starting to browse -- stash whatever was being typed so the
        // down arrow can bring it back later.
        for (size_t i = 0; i < keyboard_buffer_length; i++) pendingCommand[i] = keyboard_buffer[i];
        pendingCommand[keyboard_buffer_length] = '\0';
        pendingCommandLength = keyboard_buffer_length;
    }

    historyBrowseOffset++;

    size_t slot = (commandHistoryCount - historyBrowseOffset) % COMMAND_HISTORY_CAPACITY;
    set_buffer_contents(commandHistory[slot], commandHistoryLengths[slot]);
}

void history_browse_newer() {
    if (historyBrowseOffset == 0) return; // already on the live/pending line

    historyBrowseOffset--;

    if (historyBrowseOffset == 0) {
        set_buffer_contents(pendingCommand, pendingCommandLength);
        return;
    }

    size_t slot = (commandHistoryCount - historyBrowseOffset) % COMMAND_HISTORY_CAPACITY;
    set_buffer_contents(commandHistory[slot], commandHistoryLengths[slot]);
}

// Backspacing in the middle of the line only erases the single cell at
// the cursor (delete_last_char()) -- nothing redraws whatever comes
// after it, so it stays stuck on screen even though the logical buffer
// (already shifted left by the time this is called) no longer matches.
// Reprint everything from the cursor to the end of the (now-shorter)
// buffer, which visually shifts it all left by one as it's redrawn;
// blank the one now-stale trailing cell left over from the old,
// one-longer line; then walk the cursor back to where editing should
// continue, since printing moved it to the end of what was just drawn.
void redraw_tail_after_delete() {
    size_t tailLength = keyboard_buffer_length - keyboard_buffer_index;

    for (size_t i = 0; i < tailLength; i++) {
        printc(keyboard_buffer[keyboard_buffer_index + i]);
    }
    printc(' ');

    for (size_t i = 0; i < tailLength + 1; i++) {
        move_cursor_left();
    }
}

// Called on Enter (in command mode) to record what was just run.
void history_push(const char* text, size_t length) {
    if (length == 0) return;
    if (length >= COMMAND_MAX_LENGTH) length = COMMAND_MAX_LENGTH - 1;

    size_t slot = commandHistoryCount % COMMAND_HISTORY_CAPACITY;

    for (size_t i = 0; i < length; i++) commandHistory[slot][i] = text[i];
    commandHistory[slot][length] = '\0';
    commandHistoryLengths[slot] = length;

    commandHistoryCount++;
    historyBrowseOffset = 0; // back to a fresh/pending line
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

            if (keyboard_buffer_index < keyboard_buffer_length) {
                redraw_tail_after_delete();
            }

            return;
        }  else if (cmdMode && event.code == KEY_CODE_ARROW_KEY_UP) {
            history_browse_older();
            return;
        } else if (cmdMode && event.code == KEY_CODE_ARROW_KEY_DOWN) {
            history_browse_newer();
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
                history_push(keyboard_buffer, keyboard_buffer_length);

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
        //bool menukey = keyboard_is_down(KEY_CODE_MENU_KEY);

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
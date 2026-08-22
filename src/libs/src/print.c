#include "print.h"
#include "bool.h"
#include "drivers/video/framebuffer.h"
#include "drivers/video/fbprint.h"
#include "x86_64/port.h"

#include <stddef.h>

#define VGA_CTRL_PORT 0x3D4
#define VGA_DATA_PORT 0x3D5

const static size_t NUM_COLS = 80;
const static size_t NUM_ROWS = 25;

struct Char {
    uint8_t character;
    uint8_t color;
};

struct Char* buffer = (struct Char*) 0xb8000;
size_t col = 0;
size_t row = 0;
uint8_t color = PRINT_COLOR_WHITE | PRINT_COLOR_BLACK << 4;

void clear_row(size_t row) {  
    struct Char empty = (struct Char) {
        character: ' ',
        color: color,
    };

    for (size_t col = 0; col < NUM_COLS; col++) {
        buffer[col + NUM_COLS * row] = empty;
    }
}

void clear_screen() {
    if (framebuffer_is_available()) {
        fbclear();
        return;
    }

    for (size_t i = 0; i < NUM_ROWS; i++) {
        clear_row(i);
    }
}

void print_newline() {
    col = 0;

    if (row < NUM_ROWS - 1) {
        row++;
        return;
    }

    for (size_t row = 1; row < NUM_ROWS; row++) {
        for (size_t col = 0; col < NUM_COLS; col++) {
            struct Char character = buffer[col + NUM_COLS * row];
            buffer[col + NUM_COLS * (row - 1)] = character;
        }
    }

    clear_row(NUM_ROWS - 1);
}

static int cursor_row = 0;
static int cursor_col = 0;

void printc(char character) {
    if (framebuffer_is_available()) {
        fbprintc(character);
        return;
    }

    if (character == '\n') {
        print_newline();
        cursor_col = 0;
        cursor_row++;
        move_cursor(cursor_row, cursor_col);
        return;
    } else if (character == '\r') {
        cursor_col = 0;
        return;
    } else if (character == '\b') {
        if (cursor_col > 0) cursor_col--;
    } else if (character == '\0') {
        return;
    }

    if (col > NUM_COLS || cursor_col >= NUM_COLS) {
        print_newline();
        cursor_col = 0;
        cursor_row++;
    }

    buffer[col + NUM_COLS * row] = (struct Char) {
        character: (uint8_t) character,
        color: color,
    };

    col++;
    cursor_col++;

    move_cursor(cursor_row, cursor_col);
}

void print(const char* str) {
    if (framebuffer_is_available()) {
        fbprint(str);
        return;
    }

    for (size_t i = 0; 1; i++) {
        char character = (uint8_t) str[i];

        if (character == '\0') {
            return;
        }

        printc(character);
    }
}

void println(const char* str) {
    if (framebuffer_is_available()) {
        fbprintln(str);
        return;
    }

    print(str);
    printc('\n');
}

void print_set_color(uint8_t foreground, uint8_t background) {
    if (framebuffer_is_available()) {
        fbprint_set_color(foreground, background);
        return;
    }

    color = foreground + (background << 4);
}

void print_uint64_dec(uint64_t value) {
    if (framebuffer_is_available()) {
        fbprint_uint64_dec(value);
        return;
    }

    if (value == 0) {
        printc('0');
        return;
    }
    
    char buffer[20];
    int i = 0;
    
    while (value > 0) {
        buffer[i++] = (value % 10) + '0';
        value /= 10;
    }
    
    while (i-- > 0) {
        printc(buffer[i]);
    }
}

void print_uint64_hex(uint64_t value) {
    if (framebuffer_is_available()) {
        fbprint_uint64_hex(value);
        return;
    }

    if (value == 0) {
        printc('0');
        return;
    }
    
    char buffer[16];
    int i = 0;
    
    while (value > 0) {
        uint8_t digit = value & 0xF;
        
        if (digit < 10) {
            buffer[i++] = digit + '0';
        } else {
            buffer[i++] = digit - 10 + 'A';
        }
        
        value >>= 4;
    }
    
    while (i-- > 0) {
        printc(buffer[i]);
    }
}

void print_uint64_bin(uint64_t value) {
    if (framebuffer_is_available()) {
        fbprint_uint64_bin(value);
        return;
    }

    char buffer[64];
    
    for (size_t i = 0; i < 64; i++) {
        buffer[i] = (value & 1) + '0';
        value >>= 1;
    }
    
    for (size_t i = 64; i > 0; i--) {
        printc(buffer[i - 1]);
    }
}

void delete_last_char() {
    if (framebuffer_is_available()) {
        fbdelete_last_char();
        return;
    }

    if (cursor_row == 0 && cursor_col == 0) return;

    if (cursor_col > 0) {
        cursor_col--;
        col--;
    } else {
        cursor_row--;
        row--;
        
        int last_col = 0;
        for (int i = 0; i < NUM_COLS; i++) {
            if (buffer[i + NUM_COLS * row].character != ' ') {
                last_col = i + 1; // one past the last real character
            }
        }

        cursor_col = last_col;
        col = last_col;
    }

    buffer[col + NUM_COLS * row] = (struct Char) {
        character: (uint8_t) ' ',
        color: color,
    };

    move_cursor(cursor_row, cursor_col);
}

void move_cursor(int row, int col) {
    if (framebuffer_is_available()) {
        fbmove_cursor(row, col);
        return;
    }

    uint16_t pos = row * NUM_COLS + col;
    port_outb(VGA_CTRL_PORT, 0x0F);
    port_outb(VGA_DATA_PORT, (uint8_t) (pos & 0xFF));

    port_outb(VGA_CTRL_PORT, 0x0E);
    port_outb(VGA_DATA_PORT, (uint8_t) ((pos >> 8) & 0xFF));
}

void move_cursor_up() {
    if (framebuffer_is_available()) {
        move_cursor_up();
        return;
    }

    if (cursor_row == 0) return; // already at top
    cursor_row--;
    row--;

    // Clamp to end of line if new row is shorter
    int last_col = 0;
    for (int i = 0; i < NUM_COLS; i++) {
        if (buffer[i + NUM_COLS * row].character != ' ') {
            last_col = i + 1;
        }
    }
    if (cursor_col > last_col) {
        cursor_col = last_col;
        col = last_col;
    }

    move_cursor(cursor_row, cursor_col);
}

void move_cursor_down() {
    if (framebuffer_is_available()) {
        fbmove_cursor_down();
        return;
    }

    if (cursor_row >= NUM_ROWS - 1) return; // already at bottom
    cursor_row++;
    row++;

    // Clamp to end of line if new row is shorter
    int last_col = 0;
    for (int i = 0; i < NUM_COLS; i++) {
        if (buffer[i + NUM_COLS * row].character != ' ') {
            last_col = i + 1;
        }
    }
    if (cursor_col > last_col) {
        cursor_col = last_col;
        col = last_col;
    }

    move_cursor(cursor_row, cursor_col);
}

void move_cursor_left() {
    if (framebuffer_is_available()) {
        fbmove_cursor_left();
        return;
    }

    if (cursor_col == 0 && cursor_row == 0) return; // already at very start

    if (cursor_col > 0) {
        cursor_col--;
        col--;
    } else {
        // Wrap to end of previous row
        cursor_row--;
        row--;
        int last_col = 0;
        for (int i = 0; i < NUM_COLS; i++) {
            if (buffer[i + NUM_COLS * row].character != ' ') {
                last_col = i + 1;
            }
        }
        cursor_col = last_col;
        col = last_col;
    }

    move_cursor(cursor_row, cursor_col);
}

void move_cursor_right() {
    if (framebuffer_is_available()) {
        fbmove_cursor_right();
        return;
    }

    // Find end of current line
    int last_col = 0;
    for (int i = 0; i < NUM_COLS; i++) {
        if (buffer[i + NUM_COLS * row].character != ' ') {
            last_col = i + 1;
        }
    }

    if (cursor_col < last_col) {
        cursor_col++;
        col++;
    } else if (cursor_row < NUM_ROWS - 1) {
        // Wrap to start of next row
        cursor_row++;
        row++;
        cursor_col = 0;
        col = 0;
    }

    move_cursor(cursor_row, cursor_col);
}

void move_cursor_to_start() {
    if (framebuffer_is_available()) {
        fbmove_cursor_to_start();
        return;
    }

    cursor_row = 0;
    cursor_col = 0;
    row = 0;
    col = 0;
    move_cursor(cursor_row, cursor_col);
}
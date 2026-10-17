#include "print.h"
#include "bool.h"
#include "log.h"
#include "drivers/video/framebuffer.h"
#include "drivers/video/fbprint.h"
#include "x86_64/port.h"

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

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

static uint32_t parse_number(const char **str) {
    uint32_t value = 0;
    
    while (**str >= '0' && **str <= '9') {
        value = value * 10 + (**str - '0');
        (*str)++;
    }
    
    return value;
}

void printf(const char* fmt, ...) {
    va_list args;

    va_start(args, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            printc(*fmt);
            fmt++;
            continue;
        }

        fmt++;

        if (*fmt == '\0') {
            break;
        }

        switch (*fmt) {
            case '%': {
                printc('%');
                break;
            }

            case 'c': {
                printc((char)va_arg(args, int));
                break;
            }

            case 's': {
                print(va_arg(args, const char*));
                break;
            }

            case 'd':
            case 'i': {
                print_int64_dec(va_arg(args, int));
                break;
            }

            case 'u': {
                print_uint64_dec(va_arg(args, unsigned int));
                break;
            }

            case 'f': {
                print_double(va_arg(args, double));
                break;
            }

            case 'F': {
                print_full_double(va_arg(args, double));
                break;
            }

            case 'x': {
                print_uint64_hex(va_arg(args, unsigned int));
                break;
            }

            case 'p': {
                print("0x");
                print_uint64_hex((uint64_t)va_arg(args, void*));
                break;
            }

            case 'C': {
                fmt++;

                uint32_t foreground = parse_number(&fmt);

                if (*fmt != ',') {
                    print("%C");
                    break;
                }

                fmt++;

                uint32_t background = parse_number(&fmt);

                if (foreground > 15 || background >> 15) {
                    print("%C");
                    break;
                }

                print_set_color((uint8_t)foreground, (uint8_t)background);

                fmt--;
                break;
            }

            default: {
                printc('%');
                printc(*fmt);
                break;
            }

        }

        fmt++;
    }

    va_end(args);
}

void print_set_color(uint8_t foreground, uint8_t background) {
    if (background == PRINT_COLOR_BLUE || background == PRINT_COLOR_LIGHT_BLUE) {
        DBG_PRINT("Something calling set_color with blue/light blue bg");
        DBG_PRINT(background == PRINT_COLOR_BLUE ? "Background is blue" : background == PRINT_COLOR_LIGHT_BLUE ? "Background is light blue" : "huh?");
    }
    if (framebuffer_is_available()) {
        fbprint_set_color(foreground, background);
        return;
    }

    color = foreground + (background << 4);
}

void print_double(double value) {
    if (value < 0.0) {
        printc('-');
        value = -value;
    }

    uint64_t integer = (uint64_t)value;
    double fractional = value - (double)integer;

    print_int64_dec(integer);
    printc('.');

    for (int i = 0; i < 6; i++) {
        fractional *= 10.0;
        int digit = (int)fractional;
        printc('0' + digit);
        fractional -= digit;
    }
}

void print_full_double(double value) {
    union {
        double d;
        uint64_t u;
    } bits = { value };

    uint64_t sign = bits.u >> 63;
    uint64_t exponent = (bits.u >> 52) & 0x7FF;
    uint64_t fraction = bits.u & 0xFFFFFFFFFFFFFULL;

    if (exponent == 0x7FF) { 
        if (fraction != 0) {
            print("NaN");
        } else {
            if (sign) {
                print("-");
            }
            print("Inf");
        }

        return;
    }

    if (sign) {
        print("-");
    }

    if (value == 0.0) {
        print("0.0");
        return;
    }

    if (value < 0.0)
        value = -value;

    int decimal_exponent = 0;

    while (value >= 10.0) {
        value /= 10.0;
        decimal_exponent++;
    }

    while (value < 1.0) {
        value *= 10.0;
        decimal_exponent--;
    }

    char digits[18];
    for (int i = 0; i < 17; i++) {
        int digit = (int)value;
        digits[i] = (char)('0' + digit);
        value = (value - digit) * 10.0;
    }

    int round_digit = (int)value;

    if (round_digit >= 5) {
        int i = 16;

        while (i >= 0 && digits[i] == '9') {
            digits[i] = '0';
            i--;
        }

        if (i >= 0) {
            digits[i]++;
        } else {
            digits[0] = '1';

            for (int j = 1; j < 17; j++) {
                digits[j] = '0';
            }
            decimal_exponent++;
        }
    }

    if (decimal_exponent >= 17 || decimal_exponent <= -5) {
        printc(digits[0]);
        printc('.');

        for (int i = 1; i < 17; i++)
            printc(digits[i]);

        printc('e');

        if (decimal_exponent >= 0) {
            printc('+');
        } else {
            printc('-');
            decimal_exponent = -decimal_exponent;
        }

        if (decimal_exponent >= 100) {
            printc('0' + decimal_exponent / 100);
            decimal_exponent %= 100;

            printc('0' + decimal_exponent / 10);
            printc('0' + decimal_exponent % 10);
        } else if (decimal_exponent >= 10) {
            printc('0' + decimal_exponent / 10);
            printc('0' + decimal_exponent % 10);
        } else {
            printc('0' + decimal_exponent);
        }

        return;
    }

    if (decimal_exponent >= 0) {
        int integer_digits = decimal_exponent + 1;

        for (int i = 0; i < integer_digits; i++) {
            if (i < 17)
                printc(digits[i]);
            else
                printc('0');
        }

        printc('.');

        for (int i = integer_digits; i < 17; i++)
            printc(digits[i]);

    } else {
        printc('0');
        printc('.');

        for (int i = -1; i > decimal_exponent; i--)
            printc('0');

        for (int i = 0; i < 17; i++)
            printc(digits[i]);
    }
}

void print_int64_dec(int64_t value) {
    if (value < 0) {
        printc('-');
        print_uint64_dec((uint64_t)(-(value + 1)) + 1);
    } else {
        print_uint64_dec((uint64_t)value);
    }
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

uint64_t print_lock(void) {
    return fb_lock_acquire();
}

void print_unlock(uint64_t flags) {
    fb_lock_release(flags);
}
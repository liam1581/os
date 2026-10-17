#include "log.h"
#include "print.h"
#include "x86_64/rtc.h"
#include "drivers/power.h"

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

void debug_putc(char c) {
    port_outb(0xE9, c);
}
void debug_write(const char* str) {
    while (*str) {
        debug_putc(*str++);
    }
}
void debug_writeln(const char* str) {
    debug_write(str);
    debug_putc('\n');
}
static uint32_t parse_number(const char **str) {
    uint32_t value = 0;
    
    while (**str >= '0' && **str <= '9') {
        value = value * 10 + (**str - '0');
        (*str)++;
    }
    
    return value;
}
void debug_writef(const char* fmt, ...) {
    va_list args;

    va_start(args, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            debug_putc(*fmt);
            fmt++;
            continue;
        }

        fmt++;

        if (*fmt == '\0') {
            break;
        }

        switch (*fmt) {
            case '%': {
                debug_putc('%');
                break;
            }

            case 'c': {
                debug_putc((char)va_arg(args, uint64_t));
                break;
            }

            case 's': {
                debug_write(va_arg(args, const char*));
                break;
            }

            case 'd':
            case 'i': {
                debug_write_int64_dec(va_arg(args, int));
                break;
            }

            case 'u': {
                debug_write_uint64_dec(va_arg(args, unsigned int));
                break;
            }

            case 'x': {
                debug_write_uint64_hex(va_arg(args, unsigned int));
                break;
            }

            case 'p': {
                debug_write("0x");
                debug_write_uint64_hex((uint64_t)va_arg(args, void*));
                break;
            }

            case 'C': {
                int fg = 0;
                int bg = 0;

                fmt++;

                while (*fmt >= '0' && *fmt <= '9') {
                    fg = fg * 10 + (*fmt - '0');
                    fmt++;
                }

                if (*fmt == ',')
                    fmt++;

                while (*fmt >= '0' && *fmt <= '9') {
                    bg = bg * 10 + (*fmt - '0');
                    fmt++;
                }

                // VGA color -> ANSI color
                static const int ansi_fg[16] = {
                    30, // BLACK
                    34, // BLUE
                    32, // GREEN
                    36, // CYAN
                    31, // RED
                    35, // MAGENTA
                    33, // BROWN
                    37, // LIGHT_GRAY
                    90, // DARK_GRAY
                    94, // LIGHT_BLUE
                    92, // LIGHT_GREEN
                    96, // LIGHT_CYAN
                    91, // LIGHT_RED
                    95, // PINK
                    93, // YELLOW
                    97  // WHITE
                };

                static const int ansi_bg[16] = {
                    40,  // BLACK
                    44,  // BLUE
                    42,  // GREEN
                    46,  // CYAN
                    41,  // RED
                    45,  // MAGENTA
                    43,  // BROWN
                    47,  // LIGHT_GRAY
                    100, // DARK_GRAY
                    104, // LIGHT_BLUE
                    102, // LIGHT_GREEN
                    106, // LIGHT_CYAN
                    101, // LIGHT_RED
                    105, // PINK
                    103, // YELLOW
                    107  // WHITE
                };

                if (fg > 15)
                    fg = PRINT_COLOR_WHITE;

                if (bg > 15)
                    bg = PRINT_COLOR_BLACK;

                debug_write("\033[");
                debug_write_uint64_dec(ansi_fg[fg]);
                debug_putc(';');
                debug_write_uint64_dec(ansi_bg[bg]);
                debug_putc('m');

                fmt--; // outer loop will increment fmt again
                break;
            }

            default: {
                debug_putc('%');
                debug_putc(*fmt);
                break;
            }

        }

        fmt++;
    }

    va_end(args);
}
void debug_vwritef(const char* fmt, va_list args) {
    while (*fmt) {
        if (*fmt != '%') {
            debug_putc(*fmt);
            fmt++;
            continue;
        }

        fmt++;

        if (*fmt == '\0') {
            break;
        }

        switch (*fmt) {
            case '%': {
                debug_putc('%');
                break;
            }

            case 'c': {
                debug_putc((char)va_arg(args, int));
                break;
            }

            case 's': {
                debug_write(va_arg(args, const char*));
                break;
            }

            case 'd':
            case 'i': {
                debug_write_int64_dec(va_arg(args, int));
                break;
            }

            case 'u': {
                debug_write_uint64_dec(va_arg(args, unsigned int));
                break;
            }

            case 'x': {
                debug_write_uint64_hex(va_arg(args, unsigned int));
                break;
            }

            case 'p': {
                debug_write("0x");
                debug_write_uint64_hex((uint64_t)va_arg(args, void*));
                break;
            }

            case 'C': {
                int fg = 0;
                int bg = 0;

                fmt++;

                while (*fmt >= '0' && *fmt <= '9') {
                    fg = fg * 10 + (*fmt - '0');
                    fmt++;
                }

                if (*fmt == ',')
                    fmt++;

                while (*fmt >= '0' && *fmt <= '9') {
                    bg = bg * 10 + (*fmt - '0');
                    fmt++;
                }

                // VGA color -> ANSI color
                static const int ansi_fg[16] = {
                    30, // BLACK
                    34, // BLUE
                    32, // GREEN
                    36, // CYAN
                    31, // RED
                    35, // MAGENTA
                    33, // BROWN
                    37, // LIGHT_GRAY
                    90, // DARK_GRAY
                    94, // LIGHT_BLUE
                    92, // LIGHT_GREEN
                    96, // LIGHT_CYAN
                    91, // LIGHT_RED
                    95, // PINK
                    93, // YELLOW
                    97  // WHITE
                };

                static const int ansi_bg[16] = {
                    40,  // BLACK
                    44,  // BLUE
                    42,  // GREEN
                    46,  // CYAN
                    41,  // RED
                    45,  // MAGENTA
                    43,  // BROWN
                    47,  // LIGHT_GRAY
                    100, // DARK_GRAY
                    104, // LIGHT_BLUE
                    102, // LIGHT_GREEN
                    106, // LIGHT_CYAN
                    101, // LIGHT_RED
                    105, // PINK
                    103, // YELLOW
                    107  // WHITE
                };

                if (fg > 15)
                    fg = PRINT_COLOR_WHITE;

                if (bg > 15)
                    bg = PRINT_COLOR_BLACK;

                debug_write("\033[");
                debug_write_uint64_dec(ansi_fg[fg]);
                debug_putc(';');
                debug_write_uint64_dec(ansi_bg[bg]);
                debug_putc('m');

                fmt--; // outer loop will increment fmt again
                break;
            }

            default: {
                debug_putc('%');
                debug_putc(*fmt);
                break;
            }

        }

        fmt++;
    }

    va_end(args);
}
void debug_write_int64_dec(int64_t value) {
    if (value < 0) {
        debug_putc('-');
        debug_write_uint64_dec((uint64_t)(-(value + 1)) + 1);
    } else {
        debug_write_uint64_dec((uint64_t)value);
    }
}
void debug_write_uint64_dec(uint64_t value) {
    if (value == 0) {
        debug_putc('0');
        return;
    }
    
    char buffer[20];
    int i = 0;
    
    while (value > 0) {
        buffer[i++] = (value % 10) + '0';
        value /= 10;
    }
    
    while (i-- > 0) {
        debug_putc(buffer[i]);
    }
}
void debug_write_uint64_hex(uint64_t value) {
    if (value == 0) {
        debug_putc('0');
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
        debug_putc(buffer[i]);
    }
}
void debug_write_uint64_bin(uint64_t value) {
    char buffer[64];
    
    for (size_t i = 0; i < 64; i++) {
        buffer[i] = (value & 1) + '0';
        value >>= 1;
    }
    
    for (size_t i = 64; i > 0; i--) {
        debug_putc(buffer[i - 1]);
    }
}

void debug_printf(const char* file, const char* function, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    DBG_PRINTSF("%C14,0[%d:%d:%d] [DEBUG] %s::%s:%d: ", rtc_hours(), rtc_minutes(), rtc_seconds(), file, function, line);
    DBG_PRINTSVF(fmt, args);
}
void debug_print(const char* file, const char* function, int line, const char* message) {
    DBG_PRINTSF("%C14,0[%d:%d:%d] [DEBUG] %s::%s:%d: %s\n", rtc_hours(), rtc_minutes(), rtc_seconds(), file, function, line, message);
}
void info_printf(const char* file, const char* function, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    DBG_PRINTSF("%C8,0[%d:%d:%d] [INFO] %s::%s:%d: ", rtc_hours(), rtc_minutes(), rtc_seconds(), file, function, line);
    DBG_PRINTSVF(fmt, args);
}
void info_print(const char* file, const char* function, int line, const char* message) {
    DBG_PRINTSF("%C8,0[%d:%d:%d] [INFO] %s::%s:%d: %s\n", rtc_hours(), rtc_minutes(), rtc_seconds(), file, function, line, message);
}
void error_printf(const char* file, const char* function, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    DBG_PRINTSF("%C4,0[%d:%d:%d] [ERROR] %s::%s:%d: ", rtc_hours(), rtc_minutes(), rtc_seconds(), file, function, line);
    DBG_PRINTSVF(fmt, args);
}
void error_print(const char* file, const char* function, int line, const char* error) {
    DBG_PRINTSF("%C4,0[%d:%d:%d] [ERROR] %s::%s:%d: %s\n", rtc_hours(), rtc_minutes(), rtc_seconds(), file, function, line, error);
}
void fatal_printf(const char* file, const char* function, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    DBG_PRINTSF("%C4,0[%d:%d:%d] [FATAL] %s::%s:%d: ", rtc_hours(), rtc_minutes(), rtc_seconds(), file, function, line);
    DBG_PRINTSVF(fmt, args);
}
void fatal_print(const char* file, const char* function, int line, const char* error) {
    DBG_PRINTSF("%C4,0[%d:%d:%d] [FATAL] %s::%s:%d: %s\n", rtc_hours(), rtc_minutes(), rtc_seconds(), file, function, line, error);
}

[[noreturn]] void kernel_panic(const char* file, const char* function, int line, const char* error, int cls) {
    if (cls == 1) { clear_screen(); }

    printf("%C4,0KERNEL PANIC IN %s::%s:%d: %s\n", file, function, line, error);
    fatal_printf(file, function, line, "{KERNEL PANIC}: %s\n", error);

    while (1) { };
}
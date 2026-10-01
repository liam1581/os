#include "log.h"
#include "print.h"

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
    DBG_PRINTSF("[DEBUG] %s::%s:%d: ", file, function, line);
    DBG_PRINTSVF(fmt, args);
}
void debug_print(const char* file, const char* function, int line, const char* message) {
    DBG_PRINTSF("[DEBUG] %s::%s:%d: %s\n", file, function, line, message);
}
void info_printf(const char* file, const char* function, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    DBG_PRINTSF("[INFO] %s::%s:%d: ", file, function, line);
    DBG_PRINTSVF(fmt, args);
}
void info_print(const char* file, const char* function, int line, const char* message) {
    DBG_PRINTSF("[INFO] %s::%s:%d: %s\n", file, function, line, message);
}
void error_printf(const char* file, const char* function, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    DBG_PRINTSF("[ERROR] %s::%s:%d: ", file, function, line);
    DBG_PRINTSVF(fmt, args);
}
void error_print(const char* file, const char* function, int line, const char* error) {
    DBG_PRINTSF("[ERROR] %s::%s:%d: %s\n", file, function, line, error);
}
void fatal_printf(const char* file, const char* function, int line, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    DBG_PRINTSF("[FATAL] %s::%s:%d: ", file, function, line);
    DBG_PRINTSVF(fmt, args);
}
void fatal_print(const char* file, const char* function, int line, const char* error) {
    DBG_PRINTSF("[FATAL] %s::%s:%d: %s\n", file, function, line, error);
}
[[noreturn]] void kernel_panic(const char* file, const char* function, int line, const char* error, int cls) {
    if (cls == 1) { clear_screen(); }

    printf("%C4,0KERNEL PANIC IN %s::%s:%d: %s\n", file, function, line, error);
    fatal_print(file, function, line, error);

    while (1) { };
}
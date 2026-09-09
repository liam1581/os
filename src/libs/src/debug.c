#include "debug.h"

#include <stddef.h>

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

void debug_print(const char* file, const char* function, int line, const char* message) {
    DBG_PRINTS("[DEBUG] ");
    DBG_PRINTS(file);
    DBG_PRINTS("::");
    DBG_PRINTS(function);
    DBG_PRINTS(":");
    DBG_PRINT_DEC(line);
    DBG_PRINTLNS(message);
    DBG_PRINTS("\n");
}
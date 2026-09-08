#include "debug.h"

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
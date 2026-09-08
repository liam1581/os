#pragma once
#include "drivers/serial.h"
#include "x86_64/port.h"

#ifdef DEBUG_SERIAL
    #define DBG_PRINTS(str) serial_write(str)
    #define DBG_PRINTLNS(str) serial_writeln(str)
#elifdef DEBUG_QEMU
    #define DBG_PRINTS(str) debug_write(str)
    #define DBG_PRINTLNS(str) debug_writeln(str)
#else
    #define DBG_PRINTS(str)
    #define DBG_PRINTLNS(str)
#endif

void debug_putc(char c);
void debug_write(const char* str);
void debug_writeln(const char* str);
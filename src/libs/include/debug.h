#pragma once
#include "drivers/serial.h"
#include "x86_64/port.h"

#if defined(DEBUG_SERIAL)
    #define DBG_PRINTS(str) serial_write(str)
    #define DBG_PRINTLNS(str) serial_writeln(str)
    #define DBG_PRINT_HEX(value) serial_write_uint64_hex(value); serial_write("\n")
    #define DBG_PRINT_DEC(value) serial_write_uint64_dec(value); serial_write("\n")
    #define DBG_PRINT_BIN(value) serial_write_uint64_bin(value); serial_write("\n")

    #define DBG_PRINT(file, function, line, message) debug_print(file, function, line, message)
#elif defined(DEBUG_QEMU)
    #define DBG_PRINTS(str) debug_write(str)
    #define DBG_PRINTLNS(str) debug_writeln(str)
    #define DBG_PRINT_HEX(value) debug_write_uint64_hex(value); debug_putc('\n')
    #define DBG_PRINT_DEC(value) debug_write_uint64_dec(value); debug_putc('\n')
    #define DBG_PRINT_BIN(value) debug_write_uint64_bin(value); debug_putc('\n')

    #define DBG_PRINT(file, function, line, message) debug_print(file, function, line, message)
#else
    #define DBG_PRINTS(str)
    #define DBG_PRINTLNS(str)
    #define DBG_PRINT_HEX(value)
    #define DBG_PRINT_DEC(value)
    #define DBG_PRINT_BIN(value)

    #define DBG_PRINT(file, function, line, message)
#endif

void debug_putc(char c);
void debug_write(const char* str);
void debug_writeln(const char* str);
void debug_write_uint64_dec(uint64_t value);
void debug_write_uint64_hex(uint64_t value);
void debug_write_uint64_bin(uint64_t value);

void debug_print(const char* file, const char* function, int line, const char* message);
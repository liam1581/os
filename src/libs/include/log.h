#pragma once
#include <stdarg.h>
#include <stdint.h>
#include "drivers/serial.h"
#include "x86_64/port.h"

#if defined(DEBUG_SERIAL)
    #define DBG_PRINTS(str) serial_write(str)
    #define DBG_PRINTLNS(str) serial_writeln(str)
    #define DBG_PRINTSF(fmt, ...) serial_writef(fmt, ##__VA_ARGS__)
    #define DBG_PRINTSVF(fmt, args) serial_vwritef(fmt, args)
    #define DBG_PRINT_INT(value) serial_write_int64_dec(value)
    #define DBG_PRINT_HEX(value) serial_write_uint64_hex(value)
    #define DBG_PRINT_DEC(value) serial_write_uint64_dec(value)
    #define DBG_PRINT_BIN(value) serial_write_uint64_bin(value)

    #define DBG_PRINT(message) debug_print(__FILE_NAME__, __FUNCTION__, __LINE__, message)
    #define INF_PRINT(message) info_print(__FILE_NAME__, __FUNCTION__, __LINE__, message)
    #define ERR_PRINT(error) error_print(__FILE_NAME__, __FUNCTION__, __LINE__, error)
    #define FATAL_PRINT(error) fatal_print(__FILE_NAME__, __FUNCTION__, __LINE__, error)
    #define KERNEL_PANIC(error, cls) kernel_panic(__FILE_NAME__, __FUNCTION__, __LINE__, error, cls)

    #define DBG_PRINTF(fmt, ...) debug_printf(__FILE_NAME__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
    #define INF_PRINTF(fmt, ...) info_printf(__FILE_NAME__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
    #define ERR_PRINTF(fmt, ...) error_printf(__FILE_NAME__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
    #define FATAL_PRINTF(fmt, ...) fatal_printf(__FILE_NAME__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)

    #define DBG_PRINT_(filename, func, line, message) debug_print(filename, func, line, message)
    #define INF_PRINT_(filename, func, line, message) info_print(filename, func, line, message)
    #define ERR_PRINT_(filename, func, line, error) error_print(filename, func, line, error)
    #define FATAL_PRINT_(filename, func, line, error) fatal_print(filename, func, line, error)
    #define KERNEL_PANIC_(filename, func, line, error, cls) kernel_panic(filename, func, line, error, cls)

    #define DBG_PRINTF_(filename, func, line, fmt, ...) debug_printf(filename, func, line fmt, ##__VA_ARGS__)
    #define INF_PRINTF_(filename, func, line, fmt, ...) info_printf(filename, func, line fmt, ##__VA_ARGS__)
    #define ERR_PRINTF_(filename, func, line, fmt, ...) error_printf(filename, func, line fmt, ##__VA_ARGS__)
    #define FATAL_PRINTF_(filename, func, line, fmt, ...) fatal_printf(filename, func, line fmt, ##__VA_ARGS__)
#elif defined(DEBUG_QEMU)
    #define DBG_PRINTS(str) debug_write(str)
    #define DBG_PRINTLNS(str) debug_writeln(str)
    #define DBG_PRINTSF(fmt, ...) debug_writef(fmt, ##__VA_ARGS__)
    #define DBG_PRINTSVF(fmt, args) debug_vwritef(fmt, args)
    #define DBG_PRINT_INT(value) debug_write_int64_dec(value)
    #define DBG_PRINT_HEX(value) debug_write_uint64_hex(value)
    #define DBG_PRINT_DEC(value) debug_write_uint64_dec(value)
    #define DBG_PRINT_BIN(value) debug_write_uint64_bin(value)

    #define DBG_PRINT(message) debug_print(__FILE_NAME__, __FUNCTION__, __LINE__, message)
    #define INF_PRINT(message) info_print(__FILE_NAME__, __FUNCTION__, __LINE__, message)
    #define ERR_PRINT(error) error_print(__FILE_NAME__, __FUNCTION__, __LINE__, error)
    #define FATAL_PRINT(error) fatal_print(__FILE_NAME__, __FUNCTION__, __LINE__, error)
    #define KERNEL_PANIC(error, cls) kernel_panic(__FILE_NAME__, __FUNCTION__, __LINE__, error, cls)

    #define DBG_PRINTF(fmt, ...) debug_printf(__FILE_NAME__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
    #define INF_PRINTF(fmt, ...) info_printf(__FILE_NAME__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
    #define ERR_PRINTF(fmt, ...) error_printf(__FILE_NAME__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
    #define FATAL_PRINTF(fmt, ...) fatal_printf(__FILE_NAME__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)

    #define DBG_PRINT_(filename, func, line, message) debug_print(filename, func, line, message)
    #define INF_PRINT_(filename, func, line, message) info_print(filename, func, line, message)
    #define ERR_PRINT_(filename, func, line, error) error_print(filename, func, line, error)
    #define FATAL_PRINT_(filename, func, line, error) fatal_print(filename, func, line, error)
    #define KERNEL_PANIC_(filename, func, line, error, cls) kernel_panic(filename, func, line, error, cls)

    #define DBG_PRINTF_(filename, func, line, fmt, ...) debug_printf(filename, func, line fmt, ##__VA_ARGS__)
    #define INF_PRINTF_(filename, func, line, fmt, ...) info_printf(filename, func, line fmt, ##__VA_ARGS__)
    #define ERR_PRINTF_(filename, func, line, fmt, ...) error_printf(filename, func, line fmt, ##__VA_ARGS__)
    #define FATAL_PRINTF_(filename, func, line, fmt, ...) fatal_printf(filename, func, line fmt, ##__VA_ARGS__)
#else
    #define DBG_PRINTS(str)
    #define DBG_PRINTLNS(str)
    #define DBG_PRINTSF(fmt, ...)
    #define DBG_PRINTSVF(fmt, args)
    #define DBG_PRINT_INT(value)
    #define DBG_PRINT_HEX(value)
    #define DBG_PRINT_DEC(value)
    #define DBG_PRINT_BIN(value)

    #define DBG_PRINT(message)
    #define INF_PRINT(message)
    #define ERR_PRINT(error)
    #define FATAL_PRINT(error)
    #define KERNEL_PANIC(error, cls)

    #define DBG_PRINTF(fmt, ...)
    #define INF_PRINTF(fmt, ...)
    #define ERR_PRINTF(fmt, ...)
    #define FATAL_PRINTF(fmt, ...)

    #define DBG_PRINT_(filename, func, line, message)
    #define INF_PRINT_(filename, func, line, message)
    #define ERR_PRINT_(filename, func, line, error)
    #define FATAL_PRINT_(filename, func, line, error)
    #define KERNEL_PANIC_(filename, func, line, error, cls)

    #define DBG_PRINTF_(filename, func, line, fmt, ...)
    #define INF_PRINTF_(filename, func, line, fmt, ...)
    #define ERR_PRINTF_(filename, func, line, fmt, ...)
    #define FATAL_PRINTF_(filename, func, line, fmt, ...)
#endif

void debug_putc(char c);
void debug_write(const char* str);
void debug_writeln(const char* str);
void debug_writef(const char* fmt, ...);
void debug_vwritef(const char* fmt, va_list args);
void debug_write_int64_dec(int64_t value);
void debug_write_uint64_dec(uint64_t value);
void debug_write_uint64_hex(uint64_t value);
void debug_write_uint64_bin(uint64_t value);

void debug_print(const char* file, const char* function, int line, const char* message);
void info_print(const char* file, const char* function, int line, const char* message);
void error_print(const char* file, const char* function, int line, const char* error);
void fatal_print(const char* file, const char* function, int line, const char* error);

void debug_printf(const char* file, const char* function, int line, const char* fmt, ...);
void info_printf(const char* file, const char* function, int line, const char* fmt, ...);
void fatal_printf(const char* file, const char* function, int line, const char* fmt, ...);
void error_printf(const char* file, const char* function, int line, const char* fmt, ...);

[[noreturn]] void kernel_panic(const char* file, const char* function, int line, const char* error, int cls);
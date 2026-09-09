#include "krnl.h"

#include "print.h"
#include "debug.h"
#include "drivers/video/framebuffer.h"

[[noreturn]] void KERNEL_PANIC(const char* filename, const char* function, int line, const char* error, int cls) {
    if (cls == 1) { clear_screen(); }
    print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
    print("KERNEL PANIC IN ");
    print(filename);
    print("::");
    print(function);
    print(":");
    print_uint64_dec(line);

    println(error);

    DBG_PRINT(filename, function, line, error);
    
    while (1);
}
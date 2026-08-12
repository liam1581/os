#include "krnl.h"

#include "print.h"
#include "debug.h"
#include "framebuffer.h"

void KERNEL_PANIC(const char* filename, const char* error, int cls) {
    if (cls == 1) { clear_screen(); }
    print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
    print("KERNEL PANIC IN \"");
    print(filename);
    println("\"!");
    println(error);

    DBG_PRINTS("KERNEL PANIC IN\"");
    DBG_PRINTS(filename);
    DBG_PRINTLNS("\"!");
    DBG_PRINTLNS(error);
    
    while (1);
}
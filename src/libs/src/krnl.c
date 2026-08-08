#include "krnl.h"
#include "print.h"

void KERNEL_PANIC(const char* filename, const char* error) {
    clear_screen();
    print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
    print("KERNEL PANIC IN \"");
    print(filename);
    println("\"!");
    println(error);
    while (1);
}
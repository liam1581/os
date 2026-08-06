#include "testing.h"

extern "C" {
    // includes
    #include "print.h"
}


extern "C" void kernel_testing() {
    clear_screen();
    print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
    println("KERNEL TESTING");

    while (1);
}
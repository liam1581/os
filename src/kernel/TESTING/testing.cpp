#include "testing.h"
#include "testing_keyboard_handler.hpp"
#include "commands.hpp"

extern "C" {
    // includes
    #include "print.h"
}


extern "C" void kernel_testing() {
    clear_screen();
    print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
    println("KERNEL IS CURRENTLY IN TESTING MODE");
    println("ALL FEATURES THAT ARE AVAILABLE HERE ARE UNDER CONSTRUCTION AND NOT FINISHED!");
    println("PLEASE SELECT THE PRODUCTION VERSION OF THE KERNEL IN GRUB\n");

    // Commands commands;
    // commands.registerCommands();

    keyboard_init();
    keyboard_set_handler(testing_handle_input);

    while (1);
}
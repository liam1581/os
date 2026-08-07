#include "testing.h"
#include "testing_keyboard_handler.hpp"
#include "commands.hpp"

extern "C" {
    // includes
    #include "print.h"
    #include "pmm.h"
}


extern "C" void kernel_testing() {
    clear_screen();
    print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
    println("KERNEL IS CURRENTLY IN TESTING MODE");
    println("ALL FEATURES THAT ARE AVAILABLE HERE ARE UNDER CONSTRUCTION AND NOT FINISHED!");
    println("PLEASE SELECT THE PRODUCTION VERSION OF THE KERNEL IN GRUB\n");

    print_set_color(PRINT_COLOR_LIGHT_GRAY, PRINT_COLOR_BLACK);
    print("Memory: ");
    print_uint64_dec(pmm_free_memory_bytes() / 1024 / 1024);
    print(" / ");
    print_uint64_dec(pmm_total_memory_bytes() / 1024 / 1024);
    println(" MiB free\n");
    print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);

    // Commands commands;
    // commands.registerCommands();

    keyboard_init();
    keyboard_set_handler(testing_handle_input);

    while (1);
}
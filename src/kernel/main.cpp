#include "main.h"

#include "csh/csh.h"
#include "commands.hpp"
#include "keyboard_handler.hpp"

extern "C" {
    #include "print.h"
    #include "command_functions.h"
    #include "pmm.h"
}

void cpp_main() {
    clear_screen();
    print_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLACK);
    println("Welcome to our 64-bit kernel!\n");

    print_set_color(PRINT_COLOR_LIGHT_GRAY, PRINT_COLOR_BLACK);
    print("Memory: ");
    print_uint64_dec(pmm_free_memory_bytes() / 1024 / 1024);
    print(" / ");
    print_uint64_dec(pmm_total_memory_bytes() / 1024 / 1024);
    println(" MiB free\n");
    print_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLACK);

    cmd_atapi_init();
    cmd_fat_init();

    // Commands commands;
    // commands.registerCommands();

    //testingCsh();

    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    print("D:");
    print(get_current_dir());
    print("> ");

    keyboard_init();
    keyboard_set_handler(handle_input);
    
    while (1);
}
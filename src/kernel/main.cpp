#include "main.h"

#include "csh/csh.h"
#include "commands.hpp"
#include "keyboard_handler.hpp"
#include "command_functions.hpp"

extern "C" {
    #include "print.h"
    #include "pmm.h"
}

Commands commands;

void cpp_main() {
    clear_screen();
    print_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLACK);
    println("Welcome to CustomOS\n");

    print_set_color(PRINT_COLOR_LIGHT_GRAY, PRINT_COLOR_BLACK);
    print("Memory: ");
    print_uint64_dec(pmm_free_memory_bytes() / 1024 / 1024);
    print(" / ");
    print_uint64_dec(pmm_total_memory_bytes() / 1024 / 1024);
    println(" MiB free");
    print("        ");
    print_uint64_dec((pmm_total_memory_bytes() - pmm_free_memory_bytes()) / 1024 / 1024);
    println(" MiB used\n");
    print_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLACK);

    cmd_atapi_init(NullArgument);
    cmd_fat_init(NullArgument);

    commands.registerCommands();

    //testingCsh();

    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    printc(get_current_drive());
    print(":");
    print(get_current_path());
    print("> ");

    keyboard_init();
    keyboard_set_handler(handle_input);
    
    while (1);
}
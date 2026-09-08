#include "testing.h"
#include "testing_keyboard_handler.hpp"
#include "command_functions.hpp"

#include "bmpTesting.hpp"

extern "C" {
    #include "print.h"

    #include "x86_64/port.h"
    #include "mem/mem.h"

    #include "krnl.h"
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
    println(" MiB free");
    print("        ");
    print_uint64_dec((pmm_total_memory_bytes() - pmm_free_memory_bytes()) / 1024 / 1024);
    println(" MiB used\n");
    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);

    cmd_atapi_init(NullArgument);
    cmd_fat_init(NullArgument);
    
    bmpTestingMain();

    print("> ");
    
    keyboard_init();
    keyboard_set_handler(testing_handle_input);

    while (1);
}
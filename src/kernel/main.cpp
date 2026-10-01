#include "main.h"

#include "commands.hpp"
#include "keyboard_handler.hpp"
#include "command_functions.hpp"

extern "C" {
    #include "print.h"
    #include "mem/mem.h"
}

Commands commands;

void cpp_main() {
    cmd_fat_init(NullArgument);
    
    uint8_t versionBuffer[4096];
    uint32_t versionLen;
    get_kernel_version(versionBuffer, &versionLen);
    
    clear_screen();
    print_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLACK);
    print("Welcome to CustomOS version ");
    for (uint32_t i = 0; i < versionLen; i++) {
        printc((char)versionBuffer[i]);
    }
    print("\n\n");

    printf("%C7,0Memory: %u / %u MiB free\n", pmm_free_memory_bytes() / 1024 / 1024, pmm_total_memory_bytes() / 1024 / 1024);
    printf("        %u MiB used\n\n", (pmm_total_memory_bytes() - pmm_free_memory_bytes()) / 1024 / 1024);

    commands.registerCommands();

    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    printc(get_current_drive());
    print(":");
    print(get_current_path());
    print("> ");

    keyboard_init();
    keyboard_set_handler(handle_input);

    while (true) {
        fbcursor_poll();
    }
}
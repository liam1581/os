#include "main.h"

#include "commands.hpp"
#include "keyboard_handler.hpp"
#include "command_functions.hpp"

extern "C" {
    #include "print.h"
    #include "mem/mem.h"
}

Commands commands;

void kernel_main() {
    uint8_t versionBuffer[4096];
    uint32_t versionLen;
    get_kernel_version(versionBuffer, &versionLen);
    
    clear_screen();
    printf("%C14,0Welcome to CustomOS version ");
    for (uint32_t i = 0; i < versionLen; i++) {
        printf("%c", versionBuffer[i]);
    }
    printf("\n\n");

    printf("%C7,0Memory: %u / %u MiB free\n", pmm_free_memory_bytes() / 1024 / 1024, pmm_total_memory_bytes() / 1024 / 1024);
    printf("        %u MiB used\n\n", (pmm_total_memory_bytes() - pmm_free_memory_bytes()) / 1024 / 1024);

    commands.registerCommands();

    printf("%C15,0Type \"help commands\" for a list of commands!\n");
    printf("%C15,0%c:%s> ", get_current_drive(), get_current_path());

    keyboard_init();
    keyboard_set_handler(handle_input);

    while (true) {
        fbcursor_poll();
    }
}
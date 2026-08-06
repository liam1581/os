#include "keyboard_handler.h"
#include "print.h"
#include "commands.h"
#include "cpp/cpp_support.h"

#include "csh/csh.h"


void kernel_main() {
    call_global_constructors();

    clear_screen();
    print_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLACK);
    println("Welcome to our 64-bit kernel!\n");

    cmd_atapi_init();
    cmd_fat_init();

    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    print("D:");
    print(get_current_dir());
    print("> ");

    keyboard_init();
    keyboard_set_handler(handle_input);

    //testingCsh();
    //cpp_hello();

    while (1);
}
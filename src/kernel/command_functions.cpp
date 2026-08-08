#include "command_functions.hpp"

extern "C" {
    #include "print.h"
    #include "debug.h"

    #include "C_command_functions.h"
    
    #include "drivers/atapi.h"
    #include "drivers/ata.h"
    #include "drivers/iso9660.h"
    #include "drivers/fat32.h"
    
    #include "drivers/power.h"
    #include "timer.h"
}

char current_dir[1024] = "/data/";
char FATcurrent_dir[1024] = "/";
bool cdInitialized = false;
bool fatInitialized = false;

int cmd_echo(ArgumentObject args) {
    const char* message = args.getArgument("message");

    if (!args.getArgument("message").isValid()) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("echo: missing message");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }

    println(message);
    return 0;
}

int cmd_help(ArgumentObject) {
    println("Available commands:");
    println("  help - Show this message");
    println("  echo \"message\" - Print a message");
    println("  cls - Clear the screen");
    println("  reboot - Restart the computer");
    println("  shutdown - Shut down the computer (QEMU & VIRTUALBOX ONLY, NO ACPI)");
    println("  serial.init \"baudrate\" - Init serial port with the baudrate");
    println("  serial.write \"message\" - Write a message to the serial port");
    println("  serial.kill - Close the serial port connection");
    println("  cd.init - Initializes the CD Driver (automatically on boot)");
    println("  ls - Does a directory listing");
    println("  cat \"file\" - Prints the file's content");
    println("  run \"file\" - Runs a LHE file");
    println("  cd \"directory\" - Changes directorys");

    
    println("Available keyboard shortcuts:");
    println("  Ctrl + Alt + E - Toggle command/text mode");
    println("  Ctrl + Alt + C - Clear the screen");
    println("  Ctrl + Alt + R - Restart the computer");
    println("  Ctrl + Alt + S - Shut down the computer (QEMU & VIRTUALBOX ONLY, NO ACPI)");

    return 0;
}

int cmd_cls(ArgumentObject) {
    clear_screen();
    move_cursor_to_start();

    return 0;
}

int cmd_restart(ArgumentObject) {
    clear_screen();
    move_cursor_to_start();
    println("Restarting...");
    delay_s(2);
    arch_restart();

    return 0;
}

int cmd_shutdown(ArgumentObject) {
    clear_screen();
    move_cursor_to_start();
    println("Shutting down...");
    delay_s(2);
    arch_shutdown();

    return 0;
}

int cmd_serial_init(ArgumentObject args) {
    int baudrate = args.getArgument("baud");

    if (!args.getArgument("baudrate").isValid() || baudrate <= 0) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("serial.init: invalid baudrate");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);

        return 1;
    }

    serial_init((uint32_t)baudrate);

    print_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLACK);
    print("Serial port initialized at ");
    print_uint64_dec((uint64_t)baudrate);
    println(" baud.");
    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    serial_write("OS CONNECTED");

    return 0;
}

int cmd_serial_write(ArgumentObject args) {
    const char* message = args.getArgument("message");

    if (!args.getArgument("message").isValid()) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("serial.write: missing message");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);

        return 1;
    }

    serial_write(message);
    return 0;
}

int cmd_atapi_init(ArgumentObject) {
    if (!atapi_init()) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("No CD drive found!");
        DBG_PRINTLNS("NO CD DRIVE FOUND!");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    } else {
        print_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLACK);
        DBG_PRINTLNS("ATAPI CD drive initialized successfully.");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    }
    if (!iso9660_init()) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("Failed to read ISO filesystem!");
        DBG_PRINTLNS("FAILED TO READ ISO FILESYSTEM!");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    } else {
        print_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLACK);
        DBG_PRINTLNS("ISO9660 filesystem initialized successfully.");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    }
    cdInitialized = true;

    return 0;
}

int cmd_ls(ArgumentObject) {
    if (!cdInitialized) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("ls: CD drive not initialized. Run 'cd.init' first.");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }

    struct ISO9660Dir dir;
    if (iso9660_list_dir(current_dir, &dir)) {
        for (uint32_t i = 0; i < dir.count; i++) {
            if (dir.entries[i].is_directory) {
                print_set_color(PRINT_COLOR_CYAN, PRINT_COLOR_BLACK);
                print("[DIR] ");
                println(dir.entries[i].name);
                print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
            } else {
                println(dir.entries[i].name);
            }
        }
    } else {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("ls: directory not found!");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        
        return 1;
    }

    return 0;
}

int cmd_cat(ArgumentObject args) {
    if (!args.getArgument("file").isValid()) return 1;

    char input[MAX_ARGUMENT_BUFFER];
    const char* argss = args.raw();
    input[0] = 'c';
    input[1] = 'a';
    input[2] = 't';
    input[3] = ' ';
    size_t i = 0;
    while (argss[i] != '\0' && i < MAX_ARGUMENT_BUFFER - 5) {
        input[i + 4] = argss[i];
        i++;
    }
    input[i + 4] = '\0';

    C_cmd_cat(input);
    return 0;
}

int cmd_run(ArgumentObject args) {
    if (!args.getArgument("file").isValid()) return 1;

    char input[MAX_ARGUMENT_BUFFER];
    const char* argss = args.raw();
    input[0] = 'r';
    input[1] = 'u';
    input[2] = 'n';
    input[3] = ' ';
    size_t i = 0;
    while (argss[i] != '\0' && i < MAX_ARGUMENT_BUFFER - 5) {
        input[i + 4] = argss[i];
        i++;
    }
    input[i + 4] = '\0';

    C_cmd_run(input);
    return 0;
}

int cmd_cd(ArgumentObject args) {
    if (!args.getArgument("directory").isValid()) return 1;

    char input[MAX_ARGUMENT_BUFFER];
    const char* argss = args.raw();
    input[0] = 'c';
    input[1] = 'd';
    input[2] = ' ';
    size_t i = 0;
    while (argss[i] != '\0' && i < MAX_ARGUMENT_BUFFER - 4) {
        input[i + 3] = argss[i];
        i++;
    }
    input[i + 3] = '\0';

    C_cmd_cd(input);
    return 0;
}

int cmd_fat_cat(ArgumentObject args) {
    if (!args.getArgument("file").isValid()) return 1;

    char input[MAX_ARGUMENT_BUFFER];
    const char* argss = args.raw();
    input[0] = 'f';
    input[1] = 'a';
    input[2] = 't';
    input[3] = '.';
    input[4] = 'c';
    input[5] = 'a';
    input[6] = 't';
    input[7] = ' ';
    size_t i = 0;
    while (argss[i] != '\0' && i < MAX_ARGUMENT_BUFFER - 9) {
        input[i + 8] = argss[i];
        i++;
    }
    input[i + 8] = '\0';

    C_cmd_fat_cat(input);
    return 0;
}

int cmd_fat_init(ArgumentObject) {
    if (!ata_init()) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("No ATA disk found!");
        DBG_PRINTLNS("NO ATA DISK FOUND!");
        return 1;
    } else {
        print_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLACK);
        DBG_PRINTLNS("ATA disk found!");
    }
    if (!fat32_init()) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("FAT32 init failed!");
        DBG_PRINTLNS("FAT32 INIT FAILED!");
        return 1;
    } else {
        print_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLACK);
        DBG_PRINTLNS("FAT32 disk ready!");
    }
    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    fatInitialized = true;

    return 0;
}

int cmd_fat_ls(ArgumentObject) {
    if (!fatInitialized) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("fat.ls: Drive not initialized. Run 'fat.init' first.");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }

    struct FAT32Dir dir;
    if (fat32_list_dir(FATcurrent_dir, &dir)) {
        for (uint32_t i = 0; i < dir.count; i++) {
            if (dir.entries[i].is_directory) {
                print_set_color(PRINT_COLOR_CYAN, PRINT_COLOR_BLACK);
                print("[DIR] ");
                println(dir.entries[i].name);
                print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
            } else {
                println(dir.entries[i].name);
            }
        }
    } else {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("fat.ls: directory not found!");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);

        return 1;
    }

    return 0;
}

char* get_current_dir() {
    return current_dir;
}
char* get_fat_current_dir() {
    return FATcurrent_dir;
}
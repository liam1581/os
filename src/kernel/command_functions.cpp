#include "command_functions.hpp"

extern "C" {
    #include "print.h"
    #include "string.h"
    #include "debug.h"
    
    #include "drivers/atapi.h"
    #include "drivers/ata.h"
    #include "drivers/iso9660.h"
    #include "drivers/fat32.h"
    #include "drivers/power.h"
    
    #include "timer.h"
    #include "lhe.h"
}

#undef bool
#undef true
#undef false

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

    if (!args.getArgument("baud").isValid() || baudrate <= 0) {
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
    if (!cdInitialized) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("cat: CD drive not initialized. Run 'cd.init' first.");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }

    ArgumentValue fileArg = args.getArgument("file");
    if (!fileArg.isValid()) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("cat: missing file argument");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }
    const char* relative_filename = fileArg;

    char filename[2048];
    strcat_s(filename, current_dir, relative_filename);

    // Check if it's a directory by looking it up in the current dir listing
    struct ISO9660Dir dir;
    if (iso9660_list_dir(current_dir, &dir)) {
        for (uint32_t i = 0; i < dir.count; i++) {
            // compare name ignoring case
            bool match = true;
            for (size_t j = 0; ; j++) {
                char a = dir.entries[i].name[j];
                char b = relative_filename[j];
                if (a >= 'A' && a <= 'Z') a += 32;
                if (b >= 'A' && b <= 'Z') b += 32;
                if (a != b) { match = false; break; }
                if (a == '\0') break;
            }
            if (match && dir.entries[i].is_directory) {
                print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
                println("cat: cannot cat a directory");
                print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
                return 1;
            }
        }
    }

    uint8_t file_buf[4096];
    uint32_t file_size;
    if (iso9660_read_file(filename, file_buf, &file_size)) {
        for (uint32_t i = 0; i < file_size; i++) {
            printc((char)file_buf[i]);
        }
        printc('\n');
    } else {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("cat: file not found!");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);

        return 1;
    }

    return 0;
}

int cmd_run(ArgumentObject args) {
    if (!cdInitialized) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("run: CD drive not initialized. Run 'cd.init' first.");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }

    ArgumentValue fileArg = args.getArgument("file");
    if (!fileArg.isValid()) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("run: missing file argument");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }
    const char* relative_filename = fileArg;

    char path[2048];
    strcat_s(path, current_dir, relative_filename);

    int exec = lhe_exec(path);

    if (exec != 1) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("run: failed to load or execute program:");
    }

    if (exec == -1) {
        println("     File not found!");
        return 1;
    } else if (exec == -2) {
        println("     Invalid header size!");
        return 1;
    } else if (exec == -3) {
        println("     Invalid header!");
        return 1;
    } else if (exec <= -4) {
        println("     Unknown Error!");
        return 1;
    }
    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    return 0;
}

int cmd_cd(ArgumentObject args) {
    if (!cdInitialized) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("cd: CD drive not initialized. Run 'cd.init' first.");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }

    ArgumentValue dirArg = args.getArgument("directory");
    const char* target = dirArg.isValid() ? (const char*)dirArg : "";

    // No directory given (or explicitly "") goes to root
    if (target[0] == '\0') {
        current_dir[0] = '/';
        current_dir[1] = '\0';
        return 0;
    }

    char new_dir[1024];

    if (target[0] == '/') {
        // Absolute path -- use as-is
        size_t i = 0;
        while (target[i] != '\0' && i < sizeof(new_dir) - 2) {
            new_dir[i] = target[i];
            i++;
        }
        // Ensure trailing slash
        if (new_dir[i - 1] != '/') {
            new_dir[i++] = '/';
        }
        new_dir[i] = '\0';
    } else if (target[0] == '.' && target[1] == '.' && (target[2] == '\0' || target[2] == '/')) {
        // Go up one directory
        if (current_dir[0] == '/' && current_dir[1] == '\0') {
            // Already at root, do nothing
            return 0;
        }

        // Copy current_dir and strip trailing slash
        size_t len = 0;
        while (current_dir[len]) len++;
        if (len > 1 && current_dir[len - 1] == '/') len--;

        // Find the previous slash
        size_t i = len;
        while (i > 0 && current_dir[i - 1] != '/') i--;

        // Copy everything up to and including that slash
        for (size_t j = 0; j < i; j++) new_dir[j] = current_dir[j];
        if (i == 0) { new_dir[0] = '/'; new_dir[1] = '\0'; }
        else new_dir[i] = '\0';
    } else {
        // Relative path -- append to current_dir
        strcat_s(new_dir, current_dir, target);
        size_t len = 0;
        while (new_dir[len]) len++;
        // Ensure trailing slash
        if (new_dir[len - 1] != '/') {
            new_dir[len]     = '/';
            new_dir[len + 1] = '\0';
        }
    }

    // Verify the directory actually exists on the ISO
    struct ISO9660Dir dir;
    if (!iso9660_list_dir(new_dir, &dir)) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        print("cd: directory not found: ");
        println(new_dir);
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }

    // Commit
    size_t i = 0;
    while (new_dir[i]) { current_dir[i] = new_dir[i]; i++; }
    current_dir[i] = '\0';

    return 0;
}

int cmd_fat_cat(ArgumentObject args) {
    if (!fatInitialized) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("fat.cat: Drive not initialized. Run 'fat.init' first.");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }

    ArgumentValue fileArg = args.getArgument("file");
    if (!fileArg.isValid()) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("fat.cat: missing file argument");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }
    const char* relative_filename = fileArg;

    char filename[2048];
    strcat_s(filename, FATcurrent_dir, relative_filename);

    // Check if it's a directory by looking it up in the current dir listing
    struct FAT32Dir dir;
    if (fat32_list_dir(FATcurrent_dir, &dir)) {
        for (uint32_t i = 0; i < dir.count; i++) {
            // compare name ignoring case
            bool match = true;
            for (size_t j = 0; ; j++) {
                char a = dir.entries[i].name[j];
                char b = relative_filename[j];
                if (a >= 'A' && a <= 'Z') a += 32;
                if (b >= 'A' && b <= 'Z') b += 32;
                if (a != b) { match = false; break; }
                if (a == '\0') break;
            }
            if (match && dir.entries[i].is_directory) {
                print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
                println("fat.cat: cannot cat a directory");
                print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
                return 1;
            }
        }
    }

    uint8_t file_buf[4096];
    uint32_t file_size;
    if (fat32_read_file(filename, file_buf, &file_size)) {
        for (uint32_t i = 0; i < file_size; i++) {
            printc((char)file_buf[i]);
        }
        printc('\n');
    } else {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("fat.cat: file not found!");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }

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
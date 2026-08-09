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

char currentDrive = 'D';

void print_not_initialized(const char* command, char drive) {
    print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
    print(command);
    if (drive == 'C') {
        println(": FAT drive (C:) not initialized. Run 'fat.init' first.");
    } else {
        println(": CD drive (D:) not initialized. Run 'cd.init' first.");
    }
    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
}

static bool names_match(const char* entryName, const char* filename) {
    for (size_t j = 0; ; j++) {
        char a = entryName[j];
        char b = filename[j];
        if (a >= 'A' && a <= 'Z') a += 32;
        if (b >= 'A' && b <= 'Z') b += 32;
        if (a != b) return false;
        if (a == '\0') return true;
    }
}

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
    if (currentDrive == 'C') {
        if (!fatInitialized) {
            print_not_initialized("ls", 'C');
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
            println("ls: directory not found!");
            print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
            return 1;
        }

        return 0;
    }

    if (!cdInitialized) {
        print_not_initialized("ls", 'D');
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
    ArgumentValue fileArg = args.getArgument("file");
    if (!fileArg.isValid()) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("cat: missing file argument");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }
    const char* relative_filename = fileArg;

    if (currentDrive == 'C') {
        if (!fatInitialized) {
            print_not_initialized("cat", 'C');
            return 1;
        }

        char filename[2048];
        strcat_s(filename, FATcurrent_dir, relative_filename);

        struct FAT32Dir dir;
        if (fat32_list_dir(FATcurrent_dir, &dir)) {
            for (uint32_t i = 0; i < dir.count; i++) {
                if (names_match(dir.entries[i].name, relative_filename) && dir.entries[i].is_directory) {
                    print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
                    println("cat: cannot cat a directory");
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
            println("cat: file not found!");
            print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
            return 1;
        }

        return 0;
    }

    if (!cdInitialized) {
        print_not_initialized("cat", 'D');
        return 1;
    }

    char filename[2048];
    strcat_s(filename, current_dir, relative_filename);

    // Check if it's a directory by looking it up in the current dir listing
    struct ISO9660Dir dir;
    if (iso9660_list_dir(current_dir, &dir)) {
        for (uint32_t i = 0; i < dir.count; i++) {
            if (names_match(dir.entries[i].name, relative_filename) && dir.entries[i].is_directory) {
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
    ArgumentValue fileArg = args.getArgument("file");
    if (!fileArg.isValid()) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("run: missing file argument");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }
    const char* relative_filename = fileArg;

    bool useFat = (currentDrive == 'C');

    if (useFat && !fatInitialized) {
        print_not_initialized("run", 'C');
        return 1;
    }
    if (!useFat && !cdInitialized) {
        print_not_initialized("run", 'D');
        return 1;
    }

    char path[2048];
    strcat_s(path, useFat ? FATcurrent_dir : current_dir, relative_filename);

    int exec = lhe_exec_from(path, useFat);

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
    ArgumentValue dirArg = args.getArgument("directory");
    const char* target = dirArg.isValid() ? (const char*)dirArg : "";

    // Detect an explicit drive prefix, e.g. "D:/data/someDirectory" or
    // "C:/someDir1/someDir2". Anything after the prefix is treated as an
    // absolute path on that drive.
    char requestedDrive = currentDrive;
    const char* path = target;
    bool explicitDrive = false;

    if ((target[0] == 'D' || target[0] == 'd' || target[0] == 'C' || target[0] == 'c') && target[1] == ':') {
        requestedDrive = (target[0] == 'D' || target[0] == 'd') ? 'D' : 'C';
        path = target + 2;
        explicitDrive = true;
    }

    if (requestedDrive == 'C' && !fatInitialized) {
        print_not_initialized("cd", 'C');
        return 1;
    }
    if (requestedDrive == 'D' && !cdInitialized) {
        print_not_initialized("cd", 'D');
        return 1;
    }

    char* baseDir = (requestedDrive == 'C') ? FATcurrent_dir : current_dir;
    char new_dir[1024];

    if (path[0] == '\0') {
        // Nothing after the drive letter (or no argument at all) -> root
        new_dir[0] = '/';
        new_dir[1] = '\0';
    } else if (explicitDrive || path[0] == '/') {
        // Absolute path. An explicit drive prefix is always absolute on
        // that drive, matching the "D:/data/foo" style.
        size_t i = 0;
        size_t o = 0;
        if (path[0] != '/') { new_dir[0] = '/'; o = 1; }
        while (path[i] != '\0' && i + o < sizeof(new_dir) - 2) {
            new_dir[i + o] = path[i];
            i++;
        }
        size_t total = i + o;
        if (total == 0 || new_dir[total - 1] != '/') new_dir[total++] = '/';
        new_dir[total] = '\0';
    } else if (path[0] == '.' && path[1] == '.' && (path[2] == '\0' || path[2] == '/')) {
        // Go up one directory within the target drive's current directory
        if (baseDir[0] == '/' && baseDir[1] == '\0') {
            return 0; // already at root, nothing to do
        }

        size_t len = 0;
        while (baseDir[len]) len++;
        if (len > 1 && baseDir[len - 1] == '/') len--;

        size_t i = len;
        while (i > 0 && baseDir[i - 1] != '/') i--;

        for (size_t j = 0; j < i; j++) new_dir[j] = baseDir[j];
        if (i == 0) { new_dir[0] = '/'; new_dir[1] = '\0'; }
        else new_dir[i] = '\0';
    } else {
        // Relative path -- append to the target drive's current directory
        strcat_s(new_dir, baseDir, path);
        size_t len = 0;
        while (new_dir[len]) len++;
        if (new_dir[len - 1] != '/') {
            new_dir[len]     = '/';
            new_dir[len + 1] = '\0';
        }
    }

    // Verify the directory actually exists on the target drive
    bool exists;
    if (requestedDrive == 'C') {
        struct FAT32Dir dir;
        exists = fat32_list_dir(new_dir, &dir);
    } else {
        struct ISO9660Dir dir;
        exists = iso9660_list_dir(new_dir, &dir);
    }

    if (!exists) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        print("cd: directory not found: ");
        print(requestedDrive == 'C' ? "C:" : "D:");
        println(new_dir);
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }

    // Commit
    size_t i = 0;
    while (new_dir[i]) { baseDir[i] = new_dir[i]; i++; }
    baseDir[i] = '\0';

    currentDrive = requestedDrive;

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

char* get_current_dir() {
    return current_dir;
}
char* get_fat_current_dir() {
    return FATcurrent_dir;
}
char get_current_drive() {
    return currentDrive;
}
char* get_current_path() {
    return currentDrive == 'C' ? FATcurrent_dir : current_dir;
}
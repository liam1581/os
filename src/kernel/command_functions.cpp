#include "command_functions.hpp"

extern "C" {
    #include "print.h"
    #include "string.h"
    #include "debug.h"
    
    #include "drivers/storage/atapi.h"
    #include "drivers/storage/ata.h"
    #include "drivers/storage/iso9660.h"
    #include "drivers/storage/fat32.h"
    
    #include "drivers/files/os/leh.h"
    
    #include "drivers/power.h"

    #include "mem/mem.h"

    #include "timer.h"
}

char current_dir[1024] = "/data/";
char FATcurrent_dir[1024] = "/";
bool cdInitialized = false;
bool fatInitialized = false;

// Which drive is currently active: 'D' = CD/ISO9660, 'C' = FAT32 disk.
// ls/cat/run operate on whichever drive this is; cd can switch it via an
// explicit "D:..." / "C:..." prefix.
char currentDrive = 'D';

extern Commands commands;

static void print_not_initialized(const char* command, char drive) {
    print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
    print(command);
    if (drive == 'C') {
        println(": FAT drive (C:) not initialized. Run 'fat.init' first.");
    } else {
        println(": CD drive (D:) not initialized. Run 'cd.init' first.");
    }
    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
}

// Case-insensitive comparison of a dir-entry name against a filename.
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

int cmd_help(ArgumentObject args) {
    ArgumentValue type = args.getArgument("type");

    if (!type.isValid()) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("help: missing argument!");
        println("Valid arguments are:");
        println("    commands - for help about commands");
        println("    keyboard - for help about keyboard shortcuts");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }

    if (strcmp(type, "commands") == 0) {
        println("Available commands:");
        for (int i = 0; i < commands.getCommandsCount(); i++) {
            if (commands.getCommands()[i].helpMessage == nullptr) {
                char* msg = (char*)kmalloc(256);
                strcat_s(msg, "Command ", commands.getCommands()[i].name);
                strcat_s(msg, msg, " has no help message!");
                DBG_PRINT(__FILE_NAME__, __FUNCTION__, __LINE__, msg);
                kfree(msg);
            } else {
                print("  ");
                print(commands.getCommands()[i].name);
                println(commands.getCommands()[i].helpMessage);
            }
        }
    } else if (strcmp(type, "keyboard") == 0) {
        println("Available keyboard shortcuts:");
        println("  Ctrl + Alt + E - Toggle command/text mode");
        println("  Ctrl + Alt + C - Clear the screen");
        println("  Ctrl + Alt + R - Restart the computer");
        println("  Ctrl + Alt + S - Shut down the VM (NO ACPI)");
    } else {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        print("help: unknown option \"");
        print(type);
        println("\"");
        return 1;
    }

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

        static struct FAT32Dir dir;
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

    static struct ISO9660Dir dir;
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

        static struct FAT32Dir dir;
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
    static struct ISO9660Dir dir;
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

    int exec = leh_exec_from(path, useFat);

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
        static struct FAT32Dir dir;
        exists = fat32_list_dir(new_dir, &dir);
    } else {
        static struct ISO9660Dir dir;
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

int cmd_cre_file(ArgumentObject args) {
    if (currentDrive != 'C') {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("cre.file: only supported on the FAT drive (C:). Switch with cd \"C:/\".");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }
    if (!fatInitialized) {
        print_not_initialized("cre.file", 'C');
        return 1;
    }

    ArgumentValue fileArg = args.getArgument("file");
    if (!fileArg.isValid()) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("cre.file: missing file argument");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }
    const char* relative_filename = fileArg;

    char path[2048];
    strcat_s(path, FATcurrent_dir, relative_filename);

    if (!fat32_create_file(path)) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("cre.file: failed to create file (already exists or invalid path)");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }

    return 0;
}

int cmd_cre_dir(ArgumentObject args) {
    if (currentDrive != 'C') {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("cre.dir: only supported on the FAT drive (C:). Switch with cd \"C:/\".");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }
    if (!fatInitialized) {
        print_not_initialized("cre.dir", 'C');
        return 1;
    }

    ArgumentValue dirArg = args.getArgument("dir");
    if (!dirArg.isValid()) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("cre.dir: missing dir argument");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }
    const char* relative_dirname = dirArg;

    char path[2048];
    strcat_s(path, FATcurrent_dir, relative_dirname);

    if (!fat32_create_dir(path)) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("cre.dir: failed to create directory (already exists or invalid path)");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }

    return 0;
}

int cmd_cp(ArgumentObject args) {
    if (currentDrive != 'C') {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("cp: only supported on the FAT drive (C:). Switch with cd \"C:/\".");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }
    if (!fatInitialized) {
        print_not_initialized("cp", 'C');
        return 1;
    }

    ArgumentValue srcArg = args.getArgument("src");
    ArgumentValue destArg = args.getArgument("dest");
    if (!srcArg.isValid() || !destArg.isValid()) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("cp: missing src/dest argument");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }
    const char* relative_src = srcArg;
    const char* relative_dest = destArg;

    char srcPath[2048];
    char destPath[2048];
    strcat_s(srcPath, FATcurrent_dir, relative_src);
    strcat_s(destPath, FATcurrent_dir, relative_dest);

    // fat32_copy_file refuses directories internally, but check ourselves
    // first so we can give a clearer error than a generic failure.
    static struct FAT32Dir srcDir;
    if (fat32_list_dir(srcPath, &srcDir)) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("cp: copying directories is not supported");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }

    if (!fat32_copy_file(srcPath, destPath)) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("cp: failed to copy file (source not found?)");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }

    return 0;
}

int cmd_mv(ArgumentObject args) {
    if (currentDrive != 'C') {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("mv: only supported on the FAT drive (C:). Switch with cd \"C:/\".");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }
    if (!fatInitialized) {
        print_not_initialized("mv", 'C');
        return 1;
    }

    ArgumentValue srcArg = args.getArgument("src");
    ArgumentValue destArg = args.getArgument("dest");
    if (!srcArg.isValid() || !destArg.isValid()) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("mv: missing src/dest argument");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }
    const char* relative_src = srcArg;
    const char* relative_dest = destArg;

    char srcPath[2048];
    char destPath[2048];
    strcat_s(srcPath, FATcurrent_dir, relative_src);
    strcat_s(destPath, FATcurrent_dir, relative_dest);

    // fat32_move_file (copy+delete) refuses directories internally too;
    // check ourselves first for a clearer error message.
    static struct FAT32Dir srcDir;
    if (fat32_list_dir(srcPath, &srcDir)) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("mv: moving directories is not supported");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }

    if (!fat32_move_file(srcPath, destPath)) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("mv: failed to move file (source not found?)");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }

    return 0;
}

int cmd_rm(ArgumentObject args) {
    if (currentDrive != 'C') {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("rm: only supported on the FAT drive (C:). Switch with cd \"C:/\".");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }
    if (!fatInitialized) {
        print_not_initialized("rm", 'C');
        return 1;
    }

    ArgumentValue fileArg = args.getArgument("file");
    if (!fileArg.isValid()) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("rm: missing file argument");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }
    const char* relative_filename = fileArg;

    char path[2048];
    strcat_s(path, FATcurrent_dir, relative_filename);

    // fat32_delete_file refuses directories internally; check ourselves
    // first for a clearer error message.
    static struct FAT32Dir dir;
    if (fat32_list_dir(path, &dir)) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("rm: deleting directories is not supported");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }

    if (!fat32_delete_file(path)) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("rm: failed to delete file (not found?)");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    }

    return 0;
}

int cmd_display_mem(ArgumentObject) {
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

    return 0;
}

void testing(uint8_t* kv, uint32_t kl) {
    print("Kernel compiled at (DD/MM/YYYY HH/MM): ");
    printc(kv[0]);
    printc(kv[1]);
    printc('/');
    printc(kv[2]);
    printc(kv[3]);
    printc('/');
    printc(kv[4]);
    printc(kv[5]);
    printc(kv[6]);
    printc(kv[7]);
    printc(' ');
    printc(kv[9]);
    printc(kv[10]);
    printc(':');
    printc(kv[11]);
    printc(kv[12]);
    printc('\n');

    print("Kernel version: ");
    printc(kv[14]);
    printc(kv[15]);
    printc(kv[16]);
    printc('\n');

    print("Kernel build number: ");
    uint32_t currentIndex = kl - (kl - 18);
    for (uint32_t i = currentIndex; i < kl; i++) {
        printc(kv[i]);
    }
    printc('\n');
}

int cmd_ver(ArgumentObject) {
    uint8_t versionBuffer[4096];
    uint32_t versionLen;

    if (!iso9660_read_file("/data/ver.txt", versionBuffer, &versionLen)) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("\nFailed to read version file!");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return 1;
    } else {
        print_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLACK);
        print("\nKernel version: ");
        for (uint32_t i = 0; i < versionLen; i++) {
            printc((char)versionBuffer[i]);
        }
        printc('\n');
        print("Compiled at (DD/MM/YYYY HH/MM): ");
        printc(versionBuffer[0]);
        printc(versionBuffer[1]);
        printc('/');
        printc(versionBuffer[2]);
        printc(versionBuffer[3]);
        printc('/');
        printc(versionBuffer[4]);
        printc(versionBuffer[5]);
        printc(versionBuffer[6]);
        printc(versionBuffer[7]);
        printc(' ');
        printc(versionBuffer[9]);
        printc(versionBuffer[10]);
        printc(':');
        printc(versionBuffer[11]);
        printc(versionBuffer[12]);
        printc('\n');

        print("Version: ");
        printc(versionBuffer[14]);
        printc(versionBuffer[15]);
        printc(versionBuffer[16]);
        printc('\n');

        print("Build number: ");
        uint32_t currentIndex = versionLen - (versionLen - 18);
        for (uint32_t i = currentIndex; i < versionLen; i++) {
            printc(versionBuffer[i]);
        }
        
        printc('\n');
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    }

    return 0;
}

void get_kernel_version(uint8_t *vb, uint32_t* vs) {
    iso9660_read_file("/data/ver.txt", vb, vs);
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
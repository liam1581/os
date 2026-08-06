#include "command_functions.h"

#include <stdint.h>
#include <stddef.h>

#include "string.h"
#include "print.h"
#include "timer.h"
#include "lhe.h"
#include "debug.h"
#include "drivers/power.h"
#include "drivers/serial.h"
#include "drivers/atapi.h"
#include "drivers/ata.h"
#include "drivers/iso9660.h"
#include "drivers/fat32.h"

char current_dir[1024] = "/data/";
char FATcurrent_dir[1024] = "/";
bool cdInitialized = false;
bool fatInitialized = false;


void cmd_echo(const char* input) {
    const char* args = input + 5;

    if (args[0] != '"') {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("echo: expected a quoted string, e.g. echo \"hello\"");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return;
    }

    size_t start = 1;
    size_t end = start;
    while (args[end] != '"' && args[end] != '\0') {
        end++;
    }

    if (args[end] == '\0') {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("echo: missing closing quote");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return;
    }

    for (size_t i = start; i < end; i++) {
        printc(args[i]);
    }
    printc('\n');
}

int cmd_help() {
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

int cmd_cls() {
    clear_screen();
    move_cursor_to_start();

    return 0;
}

int cmd_restart() {
    clear_screen();
    move_cursor_to_start();
    println("Restarting...");
    delay_s(2);
    arch_restart();

    return 0;
}

int cmd_shutdown() {
    clear_screen();
    move_cursor_to_start();
    println("Shutting down...");
    delay_s(2);
    arch_shutdown();

    return 0;
}

void cmd_serial_init(const char* input) {
    const char* args = input + 12;

    if (args[0] != '"') {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("serial.init: expected a quoted string, e.g. serial.init \"115200\"");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return;
    }

    size_t start = 1;
    size_t end = start;
    while (args[end] != '"' && args[end] != '\0') {
        end++;
    }

    if (args[end] == '\0') {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("serial.init: missing closing quote");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return;
    }

    char baud_str[16];
    size_t baud_len = end - start;
    if (baud_len >= sizeof(baud_str)) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("serial.init: baud rate string too long");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return;
    }

    for (size_t i = 0; i < baud_len; i++) {
        baud_str[i] = args[start + i];
    }
    baud_str[baud_len] = '\0';

    uint32_t baud = (uint32_t)strtoul(baud_str, NULL, 10);
    if (baud == 0) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("serial.init: invalid baud rate");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return;
    }

    serial_init(baud);
    print_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLACK);
    print("Serial port initialized at ");
    print_uint64_dec(baud);
    println(" baud.");
    print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    serial_write("OS CONNECTED");
}

void cmd_serial_write(const char* input) {
    const char* args = input + 13;

    if (args[0] != '"') {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("serial.write: expected a quoted string, e.g. serial.write \"hello\"");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return;
    }

    size_t start = 1;
    size_t end = start;
    while (args[end] != '"' && args[end] != '\0') {
        end++;
    }

    if (args[end] == '\0') {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("serial.write: missing closing quote");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return;
    }

    for (size_t i = start; i < end; i++) {
        serial_write_byte(args[i]);
    }
    serial_writeln("");
}

int cmd_atapi_init() {
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

int cmd_ls() {
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
    }

    return 0;
}

void cmd_cat(const char* input) {
    if (!cdInitialized) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("cat: CD drive not initialized. Run 'cd.init' first.");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return;
    }

    const char* args = input + 4;

    if (args[0] != '"') {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("cat: expected a quoted string, e.g. cat \"filename.txt\"");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return;
    }

    size_t start = 1;
    size_t end = start;
    while (args[end] != '"' && args[end] != '\0') {
        end++;
    }

    if (args[end] == '\0') {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("cat: missing closing quote");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return;
    }

    char relative_filename[1024];
    size_t filename_len = end - start;
    if (filename_len >= sizeof(relative_filename)) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("cat: filename too long");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return;
    }

    for (size_t i = 0; i < filename_len; i++) {
        relative_filename[i] = args[start + i];
    }
    relative_filename[filename_len] = '\0';

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
                return;
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
    }
}

void cmd_run(const char* input) {
    if (!cdInitialized) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("run: CD drive not initialized. Run 'cd.init' first.");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return;
    }

    const char* args = input + 4;
    if (args[0] != '"') {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("run: expected a quoted path, e.g. run \"/data/example.lse\"");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
    } else {
        size_t start = 1, end = 1;
        while (args[end] != '"' && args[end] != '\0') end++;

        if (args[end] == '\0') {
            print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
            println("run: missing closing quote");
            print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        } else {
            char relative_filename[1024];
            size_t filename_len = end - start;
            if (filename_len >= sizeof(relative_filename)) {
                print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
                println("run: filename too long");
                print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
                return;
            }

            for (size_t i = 0; i < filename_len; i++) {
                relative_filename[i] = args[start + i];
            }
            relative_filename[filename_len] = '\0';

            char path[2048];
            strcat_s(path, current_dir, relative_filename);

            int exec = lhe_exec(path);

            if (exec != 1) {
                print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
                println("run: failed to load or execute program:");
            }

            if (exec == -1) {
                println("     File not found!");
            } else if (exec == -2) {
                println("     Invalid header size!");
            } else if (exec == -3) {
                println("     Invalid header!");
            }
            print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        }
    }
}

void cmd_cd(const char* input) {
    if (!cdInitialized) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("cd: CD drive not initialized. Run 'cd.init' first.");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return;
    }

    const char* args = input + 3; // skip "cd "

    // cd with no args goes to root
    if (args[0] == '\0') {
        current_dir[0] = '/';
        current_dir[1] = '\0';
        return;
    }

    char new_dir[1024];

    if (args[0] == '/') {
        // Absolute path — use as-is
        size_t i = 0;
        while (args[i] != '\0' && i < sizeof(new_dir) - 2) {
            new_dir[i] = args[i];
            i++;
        }
        // Ensure trailing slash
        if (new_dir[i - 1] != '/') {
            new_dir[i++] = '/';
        }
        new_dir[i] = '\0';
    } else if (args[0] == '.' && args[1] == '.' && (args[2] == '\0' || args[2] == '/')) {
        // Go up one directory
        if (current_dir[0] == '/' && current_dir[1] == '\0') {
            // Already at root, do nothing
            return;
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
        // Relative path — append to current_dir
        strcat_s(new_dir, current_dir, args);
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
        return;
    }

    // Commit
    size_t i = 0;
    while (new_dir[i]) { current_dir[i] = new_dir[i]; i++; }
    current_dir[i] = '\0';
}

int cmd_fat_init() {
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

int cmd_fat_ls() {
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
    }

    return 0;
}

void cmd_fat_cat(const char* input) {
    if (!fatInitialized) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("fat.cat: Drive not initialized. Run 'fat.init' first.");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return;
    }

    const char* args = input + 8;

    if (args[0] != '"') {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("fat.cat: expected a quoted string, e.g. fat.cat \"filename.txt\"");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return;
    }

    size_t start = 1;
    size_t end = start;
    while (args[end] != '"' && args[end] != '\0') {
        end++;
    }

    if (args[end] == '\0') {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("fat.cat: missing closing quote");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return;
    }

    char relative_filename[1024];
    size_t filename_len = end - start;
    if (filename_len >= sizeof(relative_filename)) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("fat.cat: filename too long");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return;
    }

    for (size_t i = 0; i < filename_len; i++) {
        relative_filename[i] = args[start + i];
    }
    relative_filename[filename_len] = '\0';

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
                return;
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
    }
}

char* get_current_dir() {
    return current_dir;
}
char* get_fat_current_dir() {
    return FATcurrent_dir;
}
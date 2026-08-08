#include <stdint.h>
#include <stddef.h>

#include "string.h"
#include "print.h"
#include "drivers/atapi.h"
#include "drivers/ata.h"
#include "drivers/iso9660.h"
#include "drivers/fat32.h"

char current_dirOLD[1024] = "/data/";
char FATcurrent_dirOLD[1024] = "/";
bool cdInitializedOLD = true;
bool fatInitializedOLD = true;


void C_cmd_cat(const char* input) {
    if (!cdInitializedOLD) {
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
    strcat_s(filename, current_dirOLD, relative_filename);

    // Check if it's a directory by looking it up in the current dir listing
    struct ISO9660Dir dir;
    if (iso9660_list_dir(current_dirOLD, &dir)) {
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

void C_cmd_run(const char* input) {
    if (!cdInitializedOLD) {
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
            strcat_s(path, current_dirOLD, relative_filename);

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

void C_cmd_cd(const char* input) {
    if (!cdInitializedOLD) {
        print_set_color(PRINT_COLOR_RED, PRINT_COLOR_BLACK);
        println("cd: CD drive not initialized. Run 'cd.init' first.");
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_BLACK);
        return;
    }

    const char* args = input + 3; // skip "cd "

    // cd with no args goes to root
    if (args[0] == '\0') {
        current_dirOLD[0] = '/';
        current_dirOLD[1] = '\0';
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
        if (current_dirOLD[0] == '/' && current_dirOLD[1] == '\0') {
            // Already at root, do nothing
            return;
        }

        // Copy current_dirOLD and strip trailing slash
        size_t len = 0;
        while (current_dirOLD[len]) len++;
        if (len > 1 && current_dirOLD[len - 1] == '/') len--;

        // Find the previous slash
        size_t i = len;
        while (i > 0 && current_dirOLD[i - 1] != '/') i--;

        // Copy everything up to and including that slash
        for (size_t j = 0; j < i; j++) new_dir[j] = current_dirOLD[j];
        if (i == 0) { new_dir[0] = '/'; new_dir[1] = '\0'; }
        else new_dir[i] = '\0';
    } else {
        // Relative path — append to current_dirOLD
        strcat_s(new_dir, current_dirOLD, args);
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
    while (new_dir[i]) { current_dirOLD[i] = new_dir[i]; i++; }
    current_dirOLD[i] = '\0';
}


void C_cmd_fat_cat(const char* input) {
    if (!fatInitializedOLD) {
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
    strcat_s(filename, FATcurrent_dirOLD, relative_filename);

    // Check if it's a directory by looking it up in the current dir listing
    struct FAT32Dir dir;
    if (fat32_list_dir(FATcurrent_dirOLD, &dir)) {
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
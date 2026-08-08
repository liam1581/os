#pragma once
#include "structs_and_enums/all.h"
#include "kapi_def.h"
#include "drivers/keycodes.h"
#include <stdint.h>
#include <stddef.h>
#include "bool.h"

typedef struct {
    // Print functions
    void (*clear_screen)();
    void (*printc)(char character);
    void (*print)(const char* string);
    void (*println)(const char* string);
    void (*printf)(char* fmt, ...);
    void (*print_set_color)(uint8_t foreground, uint8_t background);
    void (*print_uint64_dec)(uint64_t value);
    void (*print_uint64_hex)(uint64_t value);
    void (*print_uint64_bin)(uint64_t value);
    void (*delete_last_char)();

    void (*move_cursor)(int row, int col);
    void (*move_cursor_up)();
    void (*move_cursor_down)();
    void (*move_cursor_left)();
    void (*move_cursor_right)();
    void (*move_cursor_to_start)();

    // String functions
    int (*strcmp)(const char* a, const char* b);
    bool (*starts_with)(const char* str, const char* prefix);
    void (*strcat_s)(char* dest, const char* a, const char* b);
    uint64_t (*strtoul)(const char* str, char** endptr, int base);

    // Serial
    void (*serial_init)(uint32_t baud);
    void (*serial_write_byte)(uint8_t byte);
    void (*serial_write)(const char* str);
    void (*serial_writeln)(const char* str);
    void (*serial_close)();

    uint8_t (*serial_read_byte)();

    // Timer
    void (*delay_s)(uint32_t s);
    void (*delay_min)(uint32_t min);

    // LHE
    int (*lhe_exec)(const char* path);

    // PS2
    uint8_t (*ps2_read_scan_code)();

    // Keyboard
    void (*keyboard_init)();
    void (*keyboard_set_handler)(void (*handler)(struct KeyboardEvent event));
    bool (*keyboard_is_down)(uint16_t code);
    bool (*keyboard_is_up)(uint16_t code);

    char (*keycode_to_ascii)(uint16_t code);
    char (*keycode_to_ascii_ext)(uint16_t code, bool shift_pressed, bool altgr_pressed);

    // RTC
    uint8_t (*rtc_seconds)();

    // Port
    uint8_t (*port_inb)(uint16_t port);
    void (*port_outb)(uint16_t port, uint8_t value);
    uint16_t (*port_inw)(uint16_t port);
    void (*port_outw)(uint16_t port, uint16_t value);
    void (*port_wait)();

    // PIC
    void (*pic_remap)();
    void (*pic_eoi_master)();
    void (*pic_eoi_slave)();

    // IDT
    void (*idt_init)();
    void (*idt_set_handler_keyboard)(void (*handler)());

    // Power
    void (*arch_restart)(void);
    void (*arch_shutdown)(void);

    // ISO9660
    bool (*iso9660_init)();
    bool (*iso9660_list_dir)(const char* path, struct ISO9660Dir* out);
    bool (*iso9660_read_file)(const char* path, uint8_t* buffer, uint32_t* out_size);

    // FAT32
    bool (*fat32_init)();

    bool (*fat32_list_dir)(const char* path, struct FAT32Dir* out);
    bool (*fat32_read_file)(const char* path, uint8_t* buffer, uint32_t* out_size);

    bool (*fat32_create_file)(const char* path);
    bool (*fat32_create_dir)(const char* path);
    bool (*fat32_write_file)(const char* path, const uint8_t* buffer, uint32_t size);
    bool (*fat32_rename)(const char* path, const char* new_name);
    bool (*fat32_copy_file)(const char* src, const char* dest);
    bool (*fat32_move_file)(const char* src, const char* dest);
    bool (*fat32_delete_file)(const char* path);

    // ATA
    bool (*ata_init)();
    bool (*ata_read_sector)(uint32_t lba, uint8_t* buffer);
    bool (*ata_write_sector)(uint32_t lba, const uint8_t* buffer);

    // ATAPI
    bool (*atapi_init)();
    bool (*atapi_read_sector)(uint32_t lba, uint8_t* buffer);

} KernelAPI;

#define LHE_ENTRY __attribute__((section(".text.entry")))
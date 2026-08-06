#include "lhe.h"

#include "kapi/kapi.h"

#include "print.h"
#include "string.h"
#include "timer.h"

#include "x86_64/idt.h"
#include "x86_64/pic.h"
#include "x86_64/port.h"
#include "x86_64/rtc.h"

#include "drivers/serial.h"
#include "drivers/iso9660.h"
#include "drivers/ata.h"
#include "drivers/atapi.h"
#include "drivers/fat32.h"
#include "drivers/keyboard.h"
#include "drivers/power.h"
#include "drivers/ps2.h"

#define LHE_LOAD_ADDRESS 0x400000

static void* kapi_memset(void* ptr, uint8_t value, uint64_t size) {
    uint8_t* p = (uint8_t*)ptr;
    for (uint64_t i = 0; i < size; i++) p[i] = value;
    return ptr;
}

static void* kapi_memcpy(void* dest, const void* src, uint64_t size) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (uint64_t i = 0; i < size; i++) d[i] = s[i];
    return dest;
}

static bool lhe_check_header(const uint8_t* header) {
    return header[0] == LHE_MAGIC_0 &&
           header[1] == LHE_MAGIC_1 &&
           header[2] == LHE_MAGIC_2 &&
           header[3] == LHE_MAGIC_3 &&
           header[4] == LHE_MAGIC_4 &&
           header[5] == LHE_MAGIC_5 &&
           header[6] == LHE_MAGIC_6 &&
           header[7] == LHE_MAGIC_7 &&
           header[8] == LHE_MAGIC_8 &&
           header[9] == LHE_MAGIC_9 &&
           header[10] == LHE_MAGIC_A &&
           header[11] == LHE_MAGIC_B &&
           header[12] == LHE_MAGIC_C &&
           header[13] == LHE_MAGIC_D &&
           header[14] == LHE_MAGIC_E &&
           header[15] == LHE_MAGIC_F;
}

int lhe_exec(const char* path) {
    uint8_t* load_addr = (uint8_t*)LHE_LOAD_ADDRESS;

    uint32_t file_size;
    if (!iso9660_read_file(path, load_addr, &file_size)) return -1;

    // Validate header
    if (file_size <= LHE_HEADER_SIZE)  return -2;
    if (!lhe_check_header(load_addr))  return -3;

    KernelAPI kapi;
    kapi.clear_screen = clear_screen;
    kapi.printc = printc;
    kapi.print = print;
    kapi.println = println;
    kapi.print_set_color = print_set_color;
    kapi.print_uint64_dec = print_uint64_dec;
    kapi.print_uint64_hex = print_uint64_hex;
    kapi.print_uint64_bin = print_uint64_bin;
    kapi.delete_last_char = delete_last_char;

    kapi.move_cursor = move_cursor;
    kapi.move_cursor_up = move_cursor_up;
    kapi.move_cursor_down = move_cursor_down;
    kapi.move_cursor_left = move_cursor_left;
    kapi.move_cursor_right = move_cursor_right;
    kapi.move_cursor_to_start = move_cursor_to_start;

    kapi.strcmp = strcmp;
    kapi.starts_with = starts_with;
    kapi.strcat_s = strcat_s;
    kapi.strtoul = strtoul;

    kapi.serial_init = serial_init;
    kapi.serial_write_byte = serial_write_byte;
    kapi.serial_write = serial_write;
    kapi.serial_writeln = serial_writeln;
    kapi.serial_close = serial_close;
    kapi.serial_read_byte = serial_read_byte;
    kapi.delay_s = delay_s;
    kapi.delay_min = delay_min;
    kapi.lhe_exec = lhe_exec;
    kapi.ps2_read_scan_code = ps2_read_scan_code;
    kapi.keyboard_init = keyboard_init;
    kapi.keyboard_set_handler = keyboard_set_handler;
    kapi.keyboard_is_down = keyboard_is_down;
    kapi.keyboard_is_up = keyboard_is_up;
    kapi.keycode_to_ascii = keycode_to_ascii;
    kapi.keycode_to_ascii_ext = keycode_to_ascii_ext;
    kapi.rtc_seconds = rtc_seconds;
    kapi.port_inb = port_inb;
    kapi.port_outb = port_outb;
    kapi.port_inw = port_inw;
    kapi.port_outw = port_outw;
    kapi.port_wait = port_wait;
    kapi.pic_remap = pic_remap;
    kapi.pic_eoi_master = pic_eoi_master;
    kapi.pic_eoi_slave = pic_eoi_slave;
    kapi.idt_init = idt_init;
    kapi.idt_set_handler_keyboard = idt_set_handler_keyboard;
    kapi.arch_restart = arch_restart;
    kapi.arch_shutdown = arch_shutdown;
    kapi.iso9660_init = iso9660_init;
    kapi.iso9660_list_dir = iso9660_list_dir;
    kapi.iso9660_read_file = iso9660_read_file;
    kapi.fat32_init = fat32_init;
    kapi.fat32_list_dir = fat32_list_dir;
    kapi.fat32_read_file = fat32_read_file;
    kapi.fat32_create_file = fat32_create_file;
    kapi.fat32_create_dir = fat32_create_dir;
    kapi.fat32_write_file = fat32_write_file;
    kapi.fat32_rename = fat32_rename;
    kapi.fat32_copy_file = fat32_copy_file;
    kapi.fat32_move_file = fat32_move_file;
    kapi.fat32_delete_file = fat32_delete_file;
    kapi.ata_init = ata_init;
    kapi.ata_read_sector = ata_read_sector;
    kapi.ata_write_sector = ata_write_sector;
    kapi.atapi_init = atapi_init;
    kapi.atapi_read_sector = atapi_read_sector;

    // Jump past the header to the first byte of code and call it as a function
    void (*program_main)(KernelAPI*) = (void(*)(KernelAPI*))(load_addr + LHE_HEADER_SIZE);    
    program_main(&kapi);

    return 1;
}
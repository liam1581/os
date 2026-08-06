#pragma once

void cmd_echo(const char* input);
void cmd_help();
void cmd_cls();
void cmd_restart();
void cmd_shutdown();
void cmd_serial_init(const char* input);
void cmd_serial_write(const char* input);
void cmd_atapi_init();
void cmd_ls();
void cmd_cat(const char* input);
void cmd_run(const char* input);
void cmd_cd(const char* input);
void cmd_fat_init();
void cmd_fat_ls();
void cmd_fat_cat(const char* input);

char* get_current_dir();
char* get_fat_current_dir();
#pragma once

void cmd_echo(const char* input);
int cmd_help();
int cmd_cls();
int cmd_restart();
int cmd_shutdown();
void cmd_serial_init(const char* input);
void cmd_serial_write(const char* input);
int cmd_atapi_init();
int cmd_ls();
void cmd_cat(const char* input);
void cmd_run(const char* input);
void cmd_cd(const char* input);
int cmd_fat_init();
int cmd_fat_ls();
void cmd_fat_cat(const char* input);

char* get_current_dir();
char* get_fat_current_dir();
#pragma once

#include "commands.hpp"

int cmd_echo(ArgumentObject args);
int cmd_help(ArgumentObject args);
int cmd_cls(ArgumentObject);
int cmd_restart(ArgumentObject);
int cmd_shutdown(ArgumentObject);
int cmd_serial_init(ArgumentObject args);
int cmd_serial_write(ArgumentObject args);
int cmd_atapi_init(ArgumentObject);
int cmd_ls(ArgumentObject);
int cmd_cat(ArgumentObject args);
int cmd_run(ArgumentObject args);
int cmd_cd(ArgumentObject args);
int cmd_fat_init(ArgumentObject);
int cmd_cre_file(ArgumentObject args);
int cmd_cre_dir(ArgumentObject args);
int cmd_cp(ArgumentObject args);
int cmd_mv(ArgumentObject args);
int cmd_rm(ArgumentObject args);

char* get_current_dir();
char* get_fat_current_dir();
char get_current_drive();
char* get_current_path();
#pragma once
#include <stdint.h>

#include "bool.h"

int strcmp(const char* a, const char* b);
bool starts_with(const char* str, const char* prefix);
void strcat_s(char* dest, const char* a, const char* b);
uint64_t strtoul(const char* str, char** endptr, int base);
#pragma once
#include <stdint.h>

#include "bool.h"

void* memset(void* dest, int value, size_t count);
void* memcpy(void* dest, const void* src, size_t count);

int strcmp(const char* a, const char* b);
bool starts_with(const char* str, const char* prefix);
void strcat_s(char* dest, const char* a, const char* b);
uint64_t strtoul(const char* str, char** endptr, int base);
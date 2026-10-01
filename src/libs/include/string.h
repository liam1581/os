#pragma once
#include <stdint.h>
#include <stddef.h>

#include "bool.h"

void* memset(void* dest, int value, size_t count);
void* memcpy(void* dest, const void* src, size_t count);

int strcmp(const char* a, const char* b);
bool starts_with(const char* str, const char* prefix);
void strcat_s(char* dest, const char* a, const char* b);
uint64_t strtoul(const char* str, char** endptr, int base);

static inline char *uint64_to_str(uint64_t value) {
    if (value == 0) {
        return (char*)'0';
    }
    
    static char buffer[20];
    int i = 0;
    
    while (value > 0) {
        buffer[i++] = (value % 10) + '0';
        value /= 10;
    }
    return buffer;
}
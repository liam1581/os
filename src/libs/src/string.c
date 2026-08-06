#include <stddef.h>
#include <stdint.h>

#include "bool.h"
#include "string.h"

int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a - *b;
}

bool starts_with(const char* str, const char* prefix) {
    for (size_t i = 0; prefix[i] != '\0'; i++) {
        if (str[i] != prefix[i]) return false;
    }
    return true;
}

void strcat_s(char* dest, const char* a, const char* b) {
    uint32_t i = 0;

    // Copy first string
    while (a[i] != '\0') {
        dest[i] = a[i];
        i++;
    }

    // Copy second string starting right after first
    uint32_t j = 0;
    while (b[j] != '\0') {
        dest[i + j] = b[j];
        j++;
    }

    dest[i + j] = '\0';
}

static int char_to_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

uint64_t strtoul(const char* str, char** endptr, int base) {
    const char* p = str;

    // Skip leading whitespace
    while (*p == ' ' || *p == '\t') p++;

    // Handle optional 0x / 0X prefix for hex, or 0 prefix for octal
    if (base == 0) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            base = 16;
            p += 2;
        } else if (p[0] == '0') {
            base = 8;
            p++;
        } else {
            base = 10;
        }
    } else if (base == 16) {
        // Consume optional 0x prefix even when base is explicitly 16
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            p += 2;
        }
    }

    uint64_t result = 0;
    int got_digit = 0;

    while (*p) {
        int digit = char_to_digit(*p);

        // Stop if character isn't valid for this base
        if (digit < 0 || digit >= base) break;

        result = result * (uint64_t)base + (uint64_t)digit;
        got_digit = 1;
        p++;
    }

    // If endptr is provided, point it at the first unused character
    if (endptr != NULL) {
        *endptr = (char*)(got_digit ? p : str);
    }

    return result;
}
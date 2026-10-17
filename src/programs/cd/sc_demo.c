#include "kapi.h"
#include "x86_64/syscall.h"
#include <stdint.h>

static inline void syscall(uint64_t number, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    (void)arg3;

    asm volatile (
        "syscall"
        :
        : "D" (number),
          "S" (arg1),
          "d" (arg2)
        : "rcx", "r11", "memory"
    );
}

static inline void int0x80(uint64_t number, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    asm volatile (
        "int $0x80"
        :
        : "D" (number),
          "S" (arg1),
          "d" (arg2),
          "c" (arg3)
        : "memory"
    );
}

static uint64_t str_length(const char* str) {
    uint64_t length = 0;

    while (str[length] != '\0')
        length++;

    return length;
}

LEH_ENTRY int main(int argc, char* argv[], KernelAPI* api) {
    (void)argc;
    (void)argv;
    (void)api;
    
    const char* via_int80 = "Hello from print_string via int 0x80!";
    const char* via_fast = "Hello from print_string via syscall!";

    syscall(SYSCALL_PRINTLN, (uint64_t) via_int80, str_length(via_int80), 0);

    syscall(SYSCALL_PRINTLN, (uint64_t) via_fast, str_length(via_fast), 0);

    syscall(SYSCALL_PRINT_CHAR, (uint64_t) 'A', 0, 0);
    syscall(SYSCALL_PRINT_CHAR, (uint64_t) 'B', 0, 0);
    syscall(SYSCALL_PRINTLN, (uint64_t) " ", str_length(" "), 0);

    return 1;
}

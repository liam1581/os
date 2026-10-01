#include "kapi.h"
#include "x86_64/syscall.h"
#include <stdint.h>

/*
 * Demonstrates the print_character / print_string syscalls added in
 * x86_64/syscall.h, via both entry paths: `int 0x80` and the faster
 * `syscall` instruction. This talks to the kernel directly through
 * those syscalls rather than through the KernelAPI table (`api`) the
 * loader hands us -- kapi is a separate, older mechanism (a plain
 * function-pointer table), unrelated to these.
 */

static inline void syscall_print_character_int80(char character) {
    asm volatile (
        "int $0x80"
        :
        : "D" ((uint64_t) SYSCALL_PRINT_CHAR),
          "S" ((uint64_t) character)
        : "memory"
    );
}

static inline void syscall_print_string_int80(const char* str, uint64_t length) {
    asm volatile (
        "int $0x80"
        :
        : "D" ((uint64_t) SYSCALL_PRINT_STRING),
          "S" ((uint64_t) str),
          "d" (length)
        : "memory"
    );
}

// The `syscall` instruction clobbers rcx (return RIP) and r11 (saved
// RFLAGS) -- same as the real x86-64 Linux ABI -- so both are listed
// here even though our two syscalls don't use them as arguments.
static inline void syscall_print_character_fast(char character) {
    asm volatile (
        "syscall"
        :
        : "D" ((uint64_t) SYSCALL_PRINT_CHAR),
          "S" ((uint64_t) character)
        : "rcx", "r11", "memory"
    );
}

static inline void syscall_print_string_fast(const char* str, uint64_t length) {
    asm volatile (
        "syscall"
        :
        : "D" ((uint64_t) SYSCALL_PRINT_STRING),
          "S" ((uint64_t) str),
          "d" (length)
        : "rcx", "r11", "memory"
    );
}

static uint64_t str_length(const char* str) {
    uint64_t length = 0;

    while (str[length] != '\0')
        length++;

    return length;
}

LHE_ENTRY void main(KernelAPI* api) {
    (void) api; // unused -- see the comment above

    const char* via_int80 = "Hello from print_string via int 0x80!";
    const char* via_fast = "Hello from print_string via syscall!";

    syscall_print_string_int80(via_int80, str_length(via_int80));
    syscall_print_character_int80('\n');

    syscall_print_string_fast(via_fast, str_length(via_fast));
    syscall_print_character_fast('\n');

    syscall_print_character_int80('A');
    syscall_print_character_fast('B');
    syscall_print_character_int80('\n');
}

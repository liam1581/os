#include "x86_64/syscall.h"

#include <stddef.h>

#include "x86_64/msr.h"
#include "x86_64/gdt.h"
#include "print.h"

#define MSR_EFER    0xC0000080
#define MSR_STAR    0xC0000081
#define MSR_LSTAR   0xC0000083
#define MSR_FMASK   0xC0000084

#define EFER_SCE (1ULL << 0)

extern void syscall_entry();

void syscall_fast_init(void) {
    uint64_t efer = rdmsr(MSR_EFER);
    efer |= EFER_SCE;
    wrmsr(MSR_EFER, efer);

    uint64_t star = ((uint64_t) GDT_SELECTOR_CS_KERNEL) << 32;
    wrmsr(MSR_STAR, star);

    wrmsr(MSR_LSTAR, (uint64_t) syscall_entry);

    wrmsr(MSR_FMASK, (1ULL << 9));
}

void idt_handler_syscall(uint64_t number, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    (void) arg3; // currently unused, remove line once used!

    switch (number) {
        case SYSCALL_PRINT_CHAR: {
            printc((char) arg1);
            break;
        }

        case SYSCALL_PRINT_STRING: {
            const char* str = (const char*) arg1;
            uint64_t length = arg2;

            if (str == NULL)
                break;
            
            for (uint64_t i = 0; i < length; i++) {
                printc(str[i]);
            }

            break;
        }

        default: {
            // Unknown syscall -- ignored
            break;
        }
    }
}
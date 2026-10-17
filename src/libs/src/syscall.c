#include <stddef.h>

#include "x86_64/syscall.h"
#include "x86_64/msr.h"
#include "x86_64/gdt.h"
#include "drivers/power.h"
#include "drivers/serial.h"
#include "timer.h"
#include "print.h"

#include "log.h"

#define MSR_EFER  0xC0000080
#define MSR_STAR  0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_FMASK 0xC0000084

#define EFER_SCE (1ULL << 0) // "SYSCALL Enable"

// Defined in syscall_.asm.
extern void syscall_entry();

void syscall_fast_init(void) {
    uint64_t efer = rdmsr(MSR_EFER);
    efer |= EFER_SCE;
    wrmsr(MSR_EFER, efer);

    // STAR[47:32] is the base the CPU uses for `syscall`'s CS/SS:
    // CS = that value, SS = that value + 8. That must land on
    // GDT_SELECTOR_CS_KERNEL / GDT_SELECTOR_SS_KERNEL respectively --
    // see gdt.h and the kernel data segment added in main.asm.
    //
    // STAR[63:48] (the equivalent base for `sysret`) is left 0:
    // syscall_entry always returns via iretq, never sysret (see its
    // comment in syscall_.asm), so it's unused.
    uint64_t star = ((uint64_t) GDT_SELECTOR_CS_KERNEL) << 32;
    wrmsr(MSR_STAR, star);

    wrmsr(MSR_LSTAR, (uint64_t) syscall_entry);

    // RFLAGS bits to clear on entry. Just IF (bit 9), so a `syscall`
    // can't be interrupted mid-dispatch -- matching the int 0x80 path,
    // whose interrupt gate does the same thing implicitly.
    wrmsr(MSR_FMASK, (1ULL << 9));
}

void idt_handler_syscall(uint64_t number, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
#if defined(DEBUG_INTERRUPTS)
    INF_PRINTF("[INT] vector=0x80 (syscall) number=%u arg1=0x%x arg2=0x%x arg3=0x%x\n", number, arg1, arg2, arg3);
#endif

    switch (number) {

        case SYSCALL_PRINT_CHAR: {
            INF_PRINTF("Handling syscall: %u, arg1: %c\n", SYSCALL_PRINT_CHAR, (char) arg1);
            printc((char) arg1);
            break;
        }

        case SYSCALL_PRINT_STRING: {
            INF_PRINTF("Handling syscall: %u, arg1: %s, arg2: %u\n", SYSCALL_PRINT_STRING, (const char*) arg1, arg2);
            const char* str = (const char*) arg1;
            uint64_t length = arg2;

            if (str == NULL)
                break;

            for (uint64_t i = 0; i < length; i++) {
                printc(str[i]);
            }

            break;
        }
        case SYSCALL_PRINTLN: {
            INF_PRINTF("Handling syscall: %u, arg1: %s, arg2: %u\n", SYSCALL_PRINTLN, (const char*) arg1, arg2);
            const char* str = (const char*) arg1;
            uint64_t length = arg2;

            if (str == NULL)
                break;

            for (uint64_t i = 0; i < length; i++) {
                printc(str[i]);
            }

            printc('\n');
            break;
        }

        case SYSCALL_PRINT_INT: {
            INF_PRINTF("Handling syscall: %u, arg1: %d\n", SYSCALL_PRINT_INT, (int64_t) arg1);
            print_int64_dec((int64_t) arg1);
            break;
        }
        case SYSCALL_PRINT_UINT_HEX: {
            INF_PRINTF("Handling syscall: %u, arg1: %u\n", SYSCALL_PRINT_UINT_HEX, (uint64_t) arg1);
            print_uint64_hex((uint64_t) arg1);
            break;
        }
        case SYSCALL_PRINT_UINT_BIN: {
            INF_PRINTF("Handling syscall: %u, arg1: %u\n", SYSCALL_PRINT_UINT_BIN, (uint64_t) arg1);
            print_uint64_bin((uint64_t) arg1);
            break;
        }
        case SYSCALL_PRINT_UINT_DEC: {
            INF_PRINTF("Handling syscall: %u, arg1: %u\n", SYSCALL_PRINT_UINT_DEC, (uint64_t) arg1);
            print_uint64_dec((uint64_t) arg1);
            break;
        }
        case SYSCALL_PRINT_SET_COLOR: {
            INF_PRINTF("Handling syscall: %u, arg1: %u, arg2: %u\n", SYSCALL_PRINT_SET_COLOR, (uint8_t) arg1, (uint8_t) arg2);
            uint8_t foreground = (uint8_t) arg1;
            uint8_t background = (uint8_t) arg2;
            print_set_color(foreground, background);
            break;
        }
        case SYSCALL_CLEAR_SCREEN: {
            INF_PRINTF("Handling syscall: %u\n", SYSCALL_CLEAR_SCREEN);
            clear_screen();
            break;
        }

        case SYSCALL_ARCH_RESTART: {
            INF_PRINTF("Handling syscall: %u\n", SYSCALL_ARCH_RESTART);
            arch_restart();
            break;
        }
        case SYSCALL_ARCH_SHUTDOWN: {
            INF_PRINTF("Handling syscall: %u\n", SYSCALL_ARCH_SHUTDOWN);
            arch_shutdown();
            break;
        }

        case SYSCALL_SERIAL_INIT: {
            INF_PRINTF("Handling syscall: %u, arg1: %u\n", SYSCALL_SERIAL_INIT, (uint32_t) arg1);
            serial_init((uint32_t) arg1);
            break;
        }
        case SYSCALL_SERIAL_WRITE_BYTE: {
            INF_PRINTF("Handling syscall: %u, arg1: %u\n", SYSCALL_SERIAL_WRITE_BYTE, (uint8_t) arg1);
            serial_write_byte((uint8_t) arg1);
            break;
        }
        case SYSCALL_SERIAL_WRITE_STRING: {
            INF_PRINTF("Handling syscall: %u, arg1: %s, arg2: %u\n", SYSCALL_SERIAL_WRITE_STRING, (const char*) arg1, arg2);
            const char* str = (const char*) arg1;
            uint64_t length = arg2;

            if (str == NULL)
                break;

            for (uint64_t i = 0; i < length; i++) {
                serial_write_byte((uint8_t) str[i]);
            }

            break;
        }
        case SYSCALL_SERIAL_WRITELN: {
            INF_PRINTF("Handling syscall: %u, arg1: %s, arg2: %u\n", SYSCALL_SERIAL_WRITELN, (const char*) arg1, arg2);
            const char* str = (const char*) arg1;
            uint64_t length = arg2;

            if (str == NULL)
                break;

            for (uint64_t i = 0; i < length; i++) {
                serial_write_byte((uint8_t) str[i]);
            }

            serial_write_byte('\n');
            break;
        }
        case SYSCALL_SERIAL_WRITE_INT: {
            INF_PRINTF("Handling syscall: %u, arg1: %d\n", SYSCALL_SERIAL_WRITE_INT, (int64_t) arg1);
            serial_write_int64_dec((int64_t) arg1);
            break;
        }
        case SYSCALL_SERIAL_WRITE_UINT_HEX: {
            INF_PRINTF("Handling syscall: %u, arg1: %u\n", SYSCALL_SERIAL_WRITE_UINT_HEX, (uint64_t) arg1);
            serial_write_uint64_hex((uint64_t) arg1);
            break;
        }
        case SYSCALL_SERIAL_WRITE_UINT_BIN: {
            INF_PRINTF("Handling syscall: %u, arg1: %u\n", SYSCALL_SERIAL_WRITE_UINT_BIN, (uint64_t) arg1);
            serial_write_uint64_bin((uint64_t) arg1);
            break;
        }
        case SYSCALL_SERIAL_WRITE_UINT_DEC: {
            INF_PRINTF("Handling syscall: %u, arg1: %u\n", SYSCALL_SERIAL_WRITE_UINT_DEC, (uint64_t) arg1);
            serial_write_uint64_dec((uint64_t) arg1);
            break;
        }
        case SYSCALL_SERIAL_CLOSE: {
            INF_PRINTF("Handling syscall: %u\n", SYSCALL_SERIAL_CLOSE);
            serial_close();
            break;
        }

        case SYSCALL_DELAY_SECONDS: {
            INF_PRINTF("Handling syscall: %u, arg1: %u\n", SYSCALL_DELAY_SECONDS, (uint64_t) arg1);
            delay_s((uint64_t) arg1);
            break;
        }
        case SYSCALL_DELAY_MINUTES: {
            INF_PRINTF("Handling syscall: %u, arg1: %u\n", SYSCALL_DELAY_MINUTES, (uint64_t) arg1);
            delay_min((uint64_t) arg1);
            break;
        }

        default: {
            ERR_PRINTF("Unknown syscall number: %u\n", number);
            break;
        }
    }
}

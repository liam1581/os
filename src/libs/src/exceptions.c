#include "x86_64/exceptions.h"
#include "print.h"
#include "log.h"

static const char* exception_name(uint64_t vector) {
    switch (vector) {
        case 0:  return "#DE Devide Error";
        case 6:  return "#UD Invalid Opcode";
        case 8:  return "#DF Double Fault";
        case 13: return "#GP General Protection Fault";
        case 14: return "#PF Page Fault";
        default: return "Unhandled Exception";
    }
}

[[noreturn]] void idt_handler_exception(uint64_t vector, uint64_t error_code, uint64_t rip) {
    FATAL_PRINTF("CPU EXCEPTION %s (vector %u) error_code=0x%x rip=0x%x\n", exception_name(vector), vector, error_code, rip);
    KERNEL_PANIC("Unhandled CPU exception", 1);
}
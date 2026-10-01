#include "x86_64/msr.h"

uint64_t rdmsr(uint32_t msr) {
    uint32_t low;
    uint32_t high;

    asm volatile (
        "rdmsr"
        : "=a" (low), "=d" (high)
        : "c" (msr)
    );

    return ((uint64_t) high << 32) | low;
}

void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = (uint32_t) value;
    uint32_t high = (uint32_t) (value >> 32);

    asm volatile (
        "wrmsr"
        :
        : "a" (low), "d" (high), "c" (msr)
    );
}

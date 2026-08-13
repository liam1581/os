#include "mem/ram_watchdog.h"

#include "krnl.h"
#include "mem/pmm.h"
#include "x86_64/idt.h"
#include "x86_64/pit.h"

#define RAM_WATCHDOG_FREQUENCY_HZ 10

static void ram_watchdog_tick(void) {
    if (pmm_free_memory_bytes() == 0) {
        KERNEL_PANIC("ram_watchdog.c", "OUT OF MEMORY: PHYSICAL RAM EXHAUSTED", 1);
    }
}

void ram_watchdog_init(void) {
    idt_set_handler_timer(ram_watchdog_tick);
    pit_init(RAM_WATCHDOG_FREQUENCY_HZ);
}
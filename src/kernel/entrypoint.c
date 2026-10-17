#include <stdint.h>

#include "c_commands.h"
#include "cpp/cpp_support.h"
#include "drivers/video/framebuffer.h"
#include "mem/ram_watchdog.h"
#include "mem/mem.h"
#include "x86_64/idt.h"
#include "x86_64/syscall.h"

#include "smp.h"
#include "log.h"
#include "print.h"

void entrypoint(uint64_t multiboot_info_addr) {
    init_iso();
    init_disk();

    pmm_init(multiboot_info_addr);
    kheap_init();

    call_global_constructors();

    idt_init();
    syscall_fast_init();
    ram_watchdog_init();
    
    if (!framebuffer_init(multiboot_info_addr))
        KERNEL_PANIC("FAILED TO INITIALIZE FRAMEBUFFER", 1);
    if (!fbprint_init(multiboot_info_addr))
        KERNEL_PANIC("FAILED TO INITIALIZE FB_PRINT", 1);

    smp_init(multiboot_info_addr);
    INF_PRINTF("{SMP}: %i of %i CPUs online\n", smp_cpu_count(), smp_cpu_detected_count());
    
    
#if defined(PRODUCTION)
#include "main.h"
    kernel_main();
#elif defined(TESTING)
#include "testing.h"
    kernel_testing();
#elif defined(KERNELPANIC)
    KERNEL_PANIC("KERNEL PANIC CAUSED BY USER\nSELECT PRODUCTION OR TESTING KERNEL IN GRUB", 1);
#else
    KERNEL_PANIC("NEITHER TESTING NOR PRODUCTION DEFINED", 1);
#endif
}
#include <stdint.h>

#include "c_commands.h"

#include "cpp/cpp_support.h"
#include "drivers/video/framebuffer.h"
#include "mem/ram_watchdog.h"
#include "mem/mem.h"
#include "x86_64/idt.h"

#include "print.h"

#include "krnl.h"

void kernel_main(uint64_t multiboot_info_addr) {
    init_iso();

    pmm_init(multiboot_info_addr);
    kheap_init();

    call_global_constructors();

    idt_init();
    ram_watchdog_init();
    
    if (!framebuffer_init(multiboot_info_addr))
        KERNEL_PANIC(__FILE_NAME__, __FUNCTION__, __LINE__, "FAILED TO INITIALIZE FRAMEBUFFER", 1);
    if (!fbprint_init(multiboot_info_addr))
        KERNEL_PANIC(__FILE_NAME__, __FUNCTION__, __LINE__, "FAILED TO INITIALIZE FB_PRINT", 1);
    
    
#if defined(PRODUCTION)
#include "main.h"
    cpp_main();
#elif defined(TESTING)
#include "testing.h"
    kernel_testing();
#elif defined(KERNELPANIC)
    KERNEL_PANIC(__FILE_NAME__, __FUNCTION__, __LINE__, "KERNEL PANIC CAUSED BY USER\nSELECT PRODUCTION OR TESTING KERNEL IN GRUB", 1);
#else
    KERNEL_PANIC(__FILE_NAME__, __FUNCTION__, __LINE__, "NEITHER TESTING NOR PRODUCTION DEFINED", 1);
#endif
}
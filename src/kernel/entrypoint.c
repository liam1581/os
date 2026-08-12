#include "cpp/cpp_support.h"

#include <stdint.h>

#include "framebuffer.h"
#include "fbprint.h"
#include "pmm.h"
#include "kheap.h"

#include "krnl.h"

void kernel_main(uint64_t multiboot_info_addr) {
    if (!framebuffer_init(multiboot_info_addr))
        KERNEL_PANIC("entrypoint.c", "FAILED TO INITIALIZE FRAMEBUFFER", 1);
    if (!fbprint_init(multiboot_info_addr))
        KERNEL_PANIC("entrypoint.c", "FAILED TO INITIALIZE FB_PRINT", 1);
    pmm_init(multiboot_info_addr);
    kheap_init();
    
    call_global_constructors();
    
#ifdef PRODUCTION
#include "main.h"
    cpp_main();
#endif
#ifdef TESTING
#include "testing.h"
    kernel_testing();
#endif
#ifdef KERNELPANIC
    KERNEL_PANIC("entrypoint.c", "KERNEL PANIC CAUSED BY USER\nSELECT PRODUCTION OR TESTING KERNEL IN GRUB", 1);
#endif

    KERNEL_PANIC("entrypoint.c", "NEITHER TESTING NOR PRODUCTION DEFINED", 1);
}
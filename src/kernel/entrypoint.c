#include "cpp/cpp_support.h"

#include "KERNEL.h"

void kernel_main() {
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
    KERNEL_PANIC("entrypoint.c", "KERNEL PANIC CAUSED BY USER\nSELECT PRODUCTION OR TESTING KERNEL IN GRUB");
#endif

    KERNEL_PANIC("entrypoint.c", "NEITHER TESTING NOR PRODUCTION DEFINED");
}
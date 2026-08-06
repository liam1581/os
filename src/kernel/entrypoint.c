#include "cpp/cpp_support.h"
#include "testing.h"

#include "main.h"

void kernel_main() {
    call_global_constructors();

    cpp_main();
    //kernel_testing();
}
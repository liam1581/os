#include "cpp/cpp_support.h"

#include "main.h"

void kernel_main() {
    call_global_constructors();
    cpp_main();
}
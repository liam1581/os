#include "kapi.h"

LEH_ENTRY int main(int argc, char* argv[], KernelAPI* api) {
    api->println("Hello, World from CD!");

    for (int i = 0; i < argc; i++) {
        api->print("Arg ");
        api->print_uint64_dec((uint64_t)i);
        api->print(": ");
        api->println(argv[i]);
    }

    return 0;
}
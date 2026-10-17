#include "kapi.h"

KernelAPI* API;

LEH_ENTRY int main(int argc, char* argv[], KernelAPI* api) {
    api->println("Hello, World from FAT32!");

    for (int i = 0; i < argc; i++) {
        api->print("Arg ");
        api->print_uint64_dec((uint64_t)i);
        api->print(": ");
        api->println(argv[i]);
    }

    return 0;
}
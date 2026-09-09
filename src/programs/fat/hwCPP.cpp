extern "C" {
    #include "kapi.h"
}

KernelAPI* API;

LHE_ENTRY void main(KernelAPI* api) {
    API = api;

    api->println("Hello, World from CPP FAT!");
}
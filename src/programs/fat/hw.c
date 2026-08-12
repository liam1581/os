#include "kapi.h"

KernelAPI* API;

LHE_ENTRY void main(KernelAPI* api) {  
    API = api;

    STR(text, "Hello, World from FAT32!");
    api->println(text);
}
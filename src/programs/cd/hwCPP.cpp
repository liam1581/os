extern "C" {
    #include "kapi.h"
}

KernelAPI* API;

LHE_ENTRY void main(KernelAPI* api) {  
    API = api;

    STR(text, "Hello, World from CPP CD!");
    api->println(text);
}
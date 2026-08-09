#include <stddef.h>
#include <stdint.h>
#include "cpp/cpp_support.h"

extern "C" {
    #include "kheap.h"
}

void* operator new(size_t size) {
    void* ptr = kmalloc(size);
    if (!ptr) {
        // Out of memory
        while (1) { asm volatile("hlt"); }
    }
    return ptr;
}

void* operator new[](size_t size) {
    return operator new(size);
}

void operator delete(void* ptr) noexcept { kfree(ptr); }
void operator delete(void* ptr, size_t) noexcept { kfree(ptr); }
void operator delete[](void* ptr) noexcept { kfree(ptr); }
void operator delete[](void* ptr, size_t) noexcept { kfree(ptr); }

extern "C" void __cxa_pure_virtual() {
    while (1) { asm volatile("hlt"); }
}

extern "C" {
    void* __dso_handle = nullptr;
};
extern "C" int __cxa_atexit(void (*)(void*), void*, void*) {
    return 0;
}

typedef void (*ctor_func)();
extern "C" ctor_func __init_array_start[];
extern "C" ctor_func __init_array_end[];

extern "C" void call_global_constructors() {
    for (ctor_func* ctor = __init_array_start; ctor < __init_array_end; ++ctor) {
        (*ctor)();
    }
}
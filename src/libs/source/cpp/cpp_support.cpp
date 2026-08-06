#include <stddef.h>
#include <stdint.h>
#include "cpp/cpp_support.h"

namespace {
    constexpr size_t HEAP_SIZE = 64 * 1024;
    alignas(16) uint8_t heap[HEAP_SIZE];
    size_t heap_offset = 0;
}

void* operator new(size_t size) {
    if (heap_offset + size > HEAP_SIZE) {
        // Out of memory
        while (1) { asm volatile("hlt"); }
    }
    void* ptr = &heap[heap_offset];
    heap_offset += size;
    return ptr;
}

void* operator new[](size_t size) {
    return operator new(size);
}

void operator delete(void*) noexcept {}
void operator delete(void*, size_t) noexcept {}
void operator delete[](void*) noexcept {}
void operator delete[](void*, size_t) noexcept {}

extern "C" void __cxa_pure_virtual() {
    while (1) { asm volatile("hlt"); }
}

extern "C" void* __dso_handle = nullptr;
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
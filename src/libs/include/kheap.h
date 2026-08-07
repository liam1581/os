#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Resets the heap. Must be called once, after pmm_init(), before the first
// kmalloc()/operator new call.
void kheap_init(void);

// Allocates `size` bytes from the kernel heap, growing it via the PMM as
// needed. Returns NULL on failure (out of physical memory).
void* kmalloc(size_t size);

// Frees a pointer previously returned by kmalloc(). Freed memory is
// reused by later kmalloc() calls (adjacent free blocks are coalesced),
// unlike the old static bump allocator this replaces.
void kfree(void* ptr);

// Bytes currently handed out to callers.
uint64_t kheap_used_bytes(void);

// Total bytes claimed from the PMM so far (used + free, heap-internal
// headers included).
uint64_t kheap_capacity_bytes(void);

#ifdef __cplusplus
}
#endif

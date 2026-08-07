#pragma once
#include <stdint.h>
#include <stddef.h>

#define PMM_FRAME_SIZE 4096

#ifdef __cplusplus
extern "C" {
#endif

// Parses the Multiboot2 memory map and sets up the frame bitmap. Must be
// called before any other pmm_* function, and before the heap is used.
void pmm_init(uint64_t multiboot_info_addr);

// Returns the physical address of a free 4KiB frame and marks it used, or
// 0 on out-of-memory. Frame 0 is always reserved (it falls inside the
// low-1MiB reservation), so 0 is safe to use as a failure sentinel.
uint64_t pmm_alloc_frame(void);

// Marks a previously allocated frame as free again.
void pmm_free_frame(uint64_t phys_addr);

// Total usable RAM detected via the Multiboot2 memory map, in bytes.
uint64_t pmm_total_memory_bytes(void);

// Currently unallocated RAM, in bytes.
uint64_t pmm_free_memory_bytes(void);

#ifdef __cplusplus
}
#endif

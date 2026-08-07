#include "multiboot2.h"
#include <stddef.h>

struct multiboot_tag_mmap* multiboot2_find_mmap(uint64_t multiboot_info_addr) {
    uint8_t* base = (uint8_t*)(uintptr_t)multiboot_info_addr;

    // Layout: [ total_size:u32 | reserved:u32 | tag... | tag... | end tag ]
    uint32_t total_size = *(uint32_t*)base;

    uint8_t* tag_ptr = base + 8;
    uint8_t* end = base + total_size;

    while (tag_ptr < end) {
        struct multiboot_tag* tag = (struct multiboot_tag*)tag_ptr;

        if (tag->type == MULTIBOOT_TAG_TYPE_END) {
            break;
        }

        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            return (struct multiboot_tag_mmap*)tag_ptr;
        }

        // Tags are 8-byte aligned.
        tag_ptr += (tag->size + 7) & ~7u;
    }

    return NULL;
}

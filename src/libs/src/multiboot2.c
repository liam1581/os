#include "multiboot2.h"
#include "debug.h"
#include "print.h"

#include <stddef.h>

static uint32_t multiboot2_align8(uint32_t value) {
    return (value + 7) & ~7u;
}

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
        tag_ptr += multiboot2_align8(tag->size);
    }

    return NULL;
}

struct multiboot_tag_framebuffer_common* multiboot2_find_framebuffer(uint64_t multiboot_info_addr) {
    uint8_t* base = (uint8_t*)(uintptr_t)multiboot_info_addr;
    
    uint32_t total_size = *(uint32_t*)base;
    
    uint8_t* tag_ptr = base + 8;
    uint8_t* end = base + total_size;
    
    while (tag_ptr < end) {
        struct multiboot_tag* tag = (struct multiboot_tag*)tag_ptr;

        print("Multiboot tag: ");
        print_uint64_dec(tag->type);
        print(" size: ");
        print_uint64_dec(tag->size);
        printc('\n');
        
        /* * End tag. */
        if (tag->type == MULTIBOOT_TAG_TYPE_END) {
            break;
        }
        
        /* * Framebuffer found. */
        if (tag->type == MULTIBOOT_TAG_TYPE_FRAMEBUFFER) {
            DBG_PRINTLNS("FOUND FRAMEBUFFER TAG");

            return (struct multiboot_tag_framebuffer_common*)tag_ptr;
        }
        
        /* * Move to next 8-byte-aligned tag. */
        tag_ptr += multiboot2_align8(tag->size);
    }
    DBG_PRINTLNS("NO FRAMEBUFFER TAG!");

    return NULL;
}

struct multiboot_tag_framebuffer_rgb* multiboot2_find_framebuffer_rgb(uint64_t multiboot_info_addr) {
    struct multiboot_tag_framebuffer_common* framebuffer = multiboot2_find_framebuffer(multiboot_info_addr);
    
    if (framebuffer == NULL) {
        DBG_PRINTLNS("NO FRAMEBUFFER TAG");

        return NULL;
    }

    print("Framebuffer type: ");
    print_uint64_dec(framebuffer->framebuffer_type);
    printc('\n');

    print("Framebuffer address: ");
    print_uint64_hex(framebuffer->framebuffer_addr);
    printc('\n');

    print("Framebuffer width: ");
    print_uint64_dec(framebuffer->framebuffer_width);
    printc('\n');

    print("Framebuffer height: ");
    print_uint64_dec(framebuffer->framebuffer_height);
    printc('\n');

    print("Framebuffer pitch: ");
    print_uint64_dec(framebuffer->framebuffer_pitch);
    printc('\n');

    print("Framebuffer BPP: ");
    print_uint64_dec(framebuffer->framebuffer_bpp);
    printc('\n');
    
    /* * Make sure this is an RGB framebuffer. */
    if (framebuffer->framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_RGB) {
        DBG_PRINTLNS("FRAMEBUFFER IS NOT RGB");
        
        return NULL;
    }
    
    DBG_PRINTLNS("FRAMEBUFER IS RGB");
    
    return (struct multiboot_tag_framebuffer_rgb*)framebuffer;
}
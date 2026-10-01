#include "multiboot2.h"
#include "log.h"
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

        INF_PRINTF("Multiboot tag %d size %d\n", tag->type, tag->size);
        
        /* * End tag. */
        if (tag->type == MULTIBOOT_TAG_TYPE_END) {
            break;
        }
        
        /* * Framebuffer found. */
        if (tag->type == MULTIBOOT_TAG_TYPE_FRAMEBUFFER) {
            INF_PRINT("FRAMEBUFFER TAG FOUND");

            return (struct multiboot_tag_framebuffer_common*)tag_ptr;
        }
        
        /* * Move to next 8-byte-aligned tag. */
        tag_ptr += multiboot2_align8(tag->size);
    }
    ERR_PRINT("NO FRAMEBUFFER TAG FOUND");

    return NULL;
}

struct multiboot_tag_framebuffer_rgb* multiboot2_find_framebuffer_rgb(uint64_t multiboot_info_addr) {
    struct multiboot_tag_framebuffer_common* framebuffer = multiboot2_find_framebuffer(multiboot_info_addr);
    
    if (framebuffer == NULL) {
        ERR_PRINT("NO FRAMEBUFFER TAG FOUND");

        return NULL;
    }

    INF_PRINTF("fb_type=%d\n", framebuffer->framebuffer_type);
    INF_PRINTF("fb_addr=0x%x\n", framebuffer->framebuffer_addr);
    INF_PRINTF("fb_width=%d\n", framebuffer->framebuffer_width);
    INF_PRINTF("fb_height=%d\n", framebuffer->framebuffer_height);
    INF_PRINTF("fb_pitch=%d\n", framebuffer->framebuffer_pitch);
    INF_PRINTF("fb_bpp=%d\n", framebuffer->framebuffer_bpp);
    
    /* * Make sure this is an RGB framebuffer. */
    if (framebuffer->framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_RGB) {
        ERR_PRINT("FRAMEBUFFER IS NOT RGB");
        
        return NULL;
    }
    
    INF_PRINT("FRAMEBUFFER IS RGB");
    
    return (struct multiboot_tag_framebuffer_rgb*)framebuffer;
}
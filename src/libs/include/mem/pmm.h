#pragma once
#include <stdint.h>

#define PMM_FRAME_SIZE 4096

void pmm_init(uint64_t multiboot_info_addr);

uint64_t pmm_alloc_frame();

void pmm_free_frame(uint64_t phys_addr);

uint64_t pmm_total_memory_bytes();

uint64_t pmm_free_memory_bytes();
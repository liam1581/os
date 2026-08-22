#pragma once
#include <stddef.h>
#include <stdint.h>

void kheap_init();

void* kmalloc(size_t size);

void kfree(void* ptr);

uint64_t kheap_used_bytes();

uint64_t kheap_capacity_bytes();
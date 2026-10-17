#include "mem/kheap.h"
#include "mem/pmm.h"
#include "sync/spinlock.h"

#define ALIGNMENT  16
#define GROW_PAGES 4 // grow the heap in 16KiB chunks when it runs out of space

typedef struct block_header {
    size_t size; // usable size, not counting this header
    int free;
    struct block_header* next;
} block_header_t;

static spinlock_t heap_lock = SPINLOCK_INIT;
static block_header_t* heap_head = NULL;
static uint64_t used_bytes = 0;
static uint64_t capacity_bytes = 0;

static size_t align_up(size_t n, size_t a) {
    return (n + (a - 1)) & ~(a - 1);
}

// Pulls fresh physical frames from the PMM (identity-mapped, so physical
// addresses double as usable pointers) and turns them into one new free
// block. pmm_alloc_contiguous() guarantees a single physically contiguous
// run, so this always fully satisfies `min_size` in one call -- it can
// only fail outright (return NULL) if no run of that many frames exists.
static block_header_t* grow_heap(size_t min_size) {
    size_t needed = min_size + sizeof(block_header_t);
    size_t pages = (needed + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;
    if (pages < GROW_PAGES) pages = GROW_PAGES;

    uint64_t base = pmm_alloc_contiguous(pages);
    if (base == 0) return NULL;

    block_header_t* block = (block_header_t*)(uintptr_t)base;
    block->size = pages * PMM_FRAME_SIZE - sizeof(block_header_t);
    block->free = 1;
    block->next = NULL;

    capacity_bytes += pages * PMM_FRAME_SIZE;
    return block;
}

void kheap_init(void) {
    heap_head = NULL;
    used_bytes = 0;
    capacity_bytes = 0;
}

void* kmalloc_locked(size_t size) {
    if (size == 0) return NULL;
    size = align_up(size, ALIGNMENT);

    block_header_t* prev = NULL;
    block_header_t* cur = heap_head;

    while (cur) {
        if (cur->free && cur->size >= size) {
            // Split off the remainder if it's big enough to be worth it.
            if (cur->size >= size + sizeof(block_header_t) + ALIGNMENT) {
                block_header_t* split = (block_header_t*)((uint8_t*)cur + sizeof(block_header_t) + size);
                split->size = cur->size - size - sizeof(block_header_t);
                split->free = 1;
                split->next = cur->next;
                cur->next = split;
                cur->size = size;
            }
            cur->free = 0;
            used_bytes += cur->size;
            return (void*)((uint8_t*)cur + sizeof(block_header_t));
        }
        prev = cur;
        cur = cur->next;
    }

    // Nothing free was big enough -- grow the heap and retry once.
    block_header_t* fresh = grow_heap(size);
    if (!fresh || fresh->size < size) return NULL; // out of memory

    if (prev) prev->next = fresh; else heap_head = fresh;

    return kmalloc_locked(size);
}

[[nodiscard("kmalloc's return value MUST BE USED!")]]
void* kmalloc(size_t size) {
    uint64_t flags = spin_lock(&heap_lock);
    void* ptr = kmalloc_locked(size);
    spin_unlock(&heap_lock, flags);
    return ptr;
}

void kfree_locked(void* ptr) {
    if (!ptr) return;

    block_header_t* block = (block_header_t*)((uint8_t*)ptr - sizeof(block_header_t));
    if (block->free) return; // guard against double-free corrupting used_bytes

    block->free = 1;
    used_bytes -= block->size;

    // Coalesce adjacent free blocks. The list stays address-ordered since
    // grow_heap() only ever appends at the end and splitting only inserts
    // a new node immediately after the block it was split from.
    block_header_t* cur = heap_head;
    while (cur && cur->next) {
        if (cur->free && cur->next->free &&
            (uint8_t*)cur + sizeof(block_header_t) + cur->size == (uint8_t*)cur->next) {
            cur->size += sizeof(block_header_t) + cur->next->size;
            cur->next = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

void kfree(void* ptr) {
    uint64_t flags = spin_lock(&heap_lock);
    kfree_locked(ptr);
    spin_unlock(&heap_lock, flags);
}

uint64_t kheap_used_bytes(void) {
    return used_bytes;
}

uint64_t kheap_capacity_bytes(void) {
    return capacity_bytes;
}

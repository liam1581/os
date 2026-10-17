#pragma once

#include <stdint.h>

int smp_current_cpu();

static inline uint64_t irq_save() {
    uint64_t flags;
    asm volatile("pushfq; pop %0; cli" : "=r" (flags) :: "memory");
    return flags;
}
static inline void irq_restore(uint64_t flags) {
    if (flags & (1u << 9)) {
        asm volatile("sti" ::: "memory");
    }
}

typedef struct {
    volatile uint32_t locked;
} spinlock_t;

#define SPINLOCK_INIT { 0 }

static inline uint64_t spin_lock(spinlock_t* lock) {
    uint64_t flags = irq_save();

    while (__atomic_exchange_n(&lock->locked, 1, __ATOMIC_ACQUIRE)) {
        while (__atomic_load_n(&lock->locked, __ATOMIC_RELAXED)) {
            asm volatile("pause");
        }
    }

    return flags;
}
static inline void spin_unlock(spinlock_t* lock, uint64_t flags) {
    __atomic_store_n(&lock->locked, 0, __ATOMIC_RELEASE);
    irq_restore(flags);
}

typedef struct {
    volatile uint32_t locked;
    volatile int32_t owner;
    uint32_t depth;
} rspinlock_t;

#define RSPINLOCK_INIT { 0, -1, 0 }

static inline uint64_t rspin_lock(rspinlock_t* lock) {
    uint64_t flags = irq_save();
    int cpu = smp_current_cpu();

    if (__atomic_load_n(&lock->owner, __ATOMIC_RELAXED) == cpu) {
        lock->depth++;
        return flags;
    }

    while (__atomic_exchange_n(&lock->locked, 1, __ATOMIC_ACQUIRE)) {
        while (__atomic_load_n(&lock->locked, __ATOMIC_RELAXED)) {
            asm volatile ("pause");
        }
    }

    lock->owner = cpu;
    lock->depth = 1;
    return flags;
}
static inline void rspin_unlock(rspinlock_t* lock, uint64_t flags) {
    if (--lock->depth > 0) {
        return;
    }

    lock->owner = -1;
    __atomic_store_n(&lock->locked, 0, __ATOMIC_RELEASE);
    irq_restore(flags);
}
#pragma once

#include <stdint.h>
#include "bool.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef void (*thread_entry_t)(void*);

typedef enum {
    THREAD_STATE_OFFLINE = 0,
    THREAD_STATE_IDLE,
    THREAD_STATE_BUSY
} ThreadState;

int thread_total_count();
int thread_available_count();

int thread_create(thread_entry_t entry, void* arg);
int thread_create_on(int id, thread_entry_t entry, void* arg);

void thread_join(int id);
void thread_yield();

int thread_current_id();
ThreadState thread_state(int id);

#if defined(__cplusplus)
}
#endif
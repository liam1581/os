#include "threading.h"
#include "smp.h"

int thread_total_count(void) {
    return smp_cpu_count();
}

int thread_available_count(void) {
    int available = 0;

    for (int i = 1; i < smp_cpu_detected_count(); i++) {
        if (smp_cpu_is_idle(i))
            available++;
    }

    return available;
}

int thread_create(thread_entry_t entry, void* arg) {
    for (int i = 1; i < smp_cpu_detected_count(); i++) {
        // smp_run_on() claims the CPU atomically, so if another
        // dispatcher beats us to this one it just fails and we move on.
        if (smp_run_on(i, entry, arg))
            return i;
    }

    return -1;
}

int thread_create_on(int id, thread_entry_t entry, void* arg) {
    return smp_run_on(id, entry, arg) ? id : -1;
}

void thread_join(int id) {
    smp_wait(id);
}

void thread_yield(void) {
    asm volatile ("pause");
}

int thread_current_id(void) {
    return smp_current_cpu();
}

ThreadState thread_state(int id) {
    if (!smp_cpu_is_online(id))
        return THREAD_STATE_OFFLINE;

    if (id == 0 || !smp_cpu_is_idle(id))
        return THREAD_STATE_BUSY;

    return THREAD_STATE_IDLE;
}

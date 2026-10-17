#pragma once

#include <stdint.h>
#include "bool.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define SMP_MAX_CPUS 64

#define SMP_IPI_WAKE_VECTOR 0xFD
#define SMP_SPURIOUS_VECTOR 0xFF

void smp_init(uint64_t multiboot_info_addr);

int smp_cpu_count(void);
int smp_cpu_detected_count(void);

int smp_current_cpu(void);

uint32_t smp_cpu_apic_id(int cpu);
bool smp_cpu_is_online(int cpu);
bool smp_cpu_is_idle(int cpu);

bool smp_run_on(int cpu, void (*fn)(void*), void* arg);

void smp_wait(int cpu);

void smp_handle_wake_ipi(void);

#if defined(__cplusplus)
}
#endif

#pragma once

#include <stdint.h>

[[noreturn]] void idt_handler_exception(uint64_t vector, uint64_t error_code, uint64_t rip);
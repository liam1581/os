#pragma once

[[noreturn]] void KERNEL_PANIC(const char* filename, const char* function, int line, const char* error, int cls);
#pragma once

/*
 * bool/true/false compatibility shim for pre-C23 C.
 *
 * This must NEVER define bool/true/false as macros when the compiler
 * already provides them as real keywords -- doing so is non-conforming
 * and (depending on compiler/version) can silently corrupt every use
 * of bool/true/false in code compiled afterward, rather than reliably
 * failing to build:
 *
 *   - In C++, bool/true/false have been keywords since C++98. They are
 *     NOT macros in any C++ standard, and later standards (C++23 on)
 *     explicitly forbid redefining them via the preprocessor at all.
 *
 *   - In C, bool/true/false only became keywords in C23 (previously
 *     only available via <stdbool.h> macros). GCC 15+ defaults C
 *     compilation to gnu23, so plain `gcc file.c` with no explicit
 *     -std= no longer needs (or should get) this shim either.
 *
 * commands.cpp/commands.hpp/command_functions.cpp used to work around
 * the C++ side of this themselves, with a manual `#undef bool` right
 * after including this header (or anything that transitively includes
 * it) to restore real C++ bool semantics. That workaround is gone now
 * that this header never touches bool/true/false in C++ to begin with
 * -- if you're touching those files, you can remove any leftover
 * `#undef bool/true/false` you find there.
 */
#if !defined(__cplusplus) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L)
    #define bool  uint8_t
    #define true  1
    #define false 0
#endif
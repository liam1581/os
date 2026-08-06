#pragma once
#include "drivers/serial.h"

#ifdef DEBUG
    #define DBG_PRINTS(str) serial_write(str)
    #define DBG_PRINTLNS(str) serial_writeln(str)
#endif
#ifndef DEBUG
    #define DBG_PRINTS(str)
    #define DBG_PRINTLNS(str)
#endif

#pragma once
#include "drivers/serial.h"

#ifdef DEBUG
    #define DBG_PRINTS(str) serial_write(str)
    #define DBG_PRINTLNS(str) serial_writeln(str)
#elifndef DEBUG
    #define DBG_PRINTS(str)
    #define DBG_PRINTLNS(str)
#endif

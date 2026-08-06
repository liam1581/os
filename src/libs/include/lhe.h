#pragma once
#include <stdint.h>
#include "bool.h"

#define LHE_HEADER_SIZE    16
#define LHE_MAGIC_0        0xFF
#define LHE_MAGIC_1        'L'
#define LHE_MAGIC_2        'S'
#define LHE_MAGIC_3        'O'
#define LHE_MAGIC_4        'S'
#define LHE_MAGIC_5        'F'
#define LHE_MAGIC_6        'H'
#define LHE_MAGIC_7        0x00
#define LHE_MAGIC_8        0x00
#define LHE_MAGIC_9        0x00
#define LHE_MAGIC_A        0x03
#define LHE_MAGIC_B        0x00
#define LHE_MAGIC_C        0x00
#define LHE_MAGIC_D        0x00
#define LHE_MAGIC_E        0x00
#define LHE_MAGIC_F        0xFF

int lhe_exec(const char* path);
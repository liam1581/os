#pragma once
#include <stdint.h>
#include "bool.h"

#define LEH_HEADER_SIZE    16
#define LEH_MAGIC_0        0xFF
#define LEH_MAGIC_1        'L'
#define LEH_MAGIC_2        'S'
#define LEH_MAGIC_3        'O'
#define LEH_MAGIC_4        'S'
#define LEH_MAGIC_5        'E'
#define LEH_MAGIC_6        'H'
#define LEH_MAGIC_7        0x00
#define LEH_MAGIC_8        0x00
#define LEH_MAGIC_9        0x00
#define LEH_MAGIC_A        0x03
#define LEH_MAGIC_B        0x00
#define LEH_MAGIC_C        0x00
#define LEH_MAGIC_D        0x00
#define LEH_MAGIC_E        0x00
#define LEH_MAGIC_F        0xFF

int leh_exec(const char* path);
int leh_exec_from(const char* path, bool useFat);
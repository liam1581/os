#include "timer.h"
#include "x86_64/rtc.h"

void delay_s(uint32_t s) {
    for (uint32_t i = 0; i < s; i++) {
        uint8_t start = rtc_seconds();
        while (rtc_seconds() == start);
    }
}

void delay_min(uint32_t min) {
    delay_s(min * 60);
}
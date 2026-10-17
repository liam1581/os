#pragma once

uint8_t rtc_seconds();
uint8_t rtc_minutes();
uint8_t rtc_hours();
uint8_t rtc_day();
uint8_t rtc_month();
uint8_t rtc_year();

uint8_t cmos_read(uint8_t reg);
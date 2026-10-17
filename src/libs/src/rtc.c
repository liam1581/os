#include "x86_64/port.h"

#define PORT_RTC_COMMAND        0x70
#define PORT_RTC_DATA           0x71
#define RTC_REGISTER_SECONDS    0x00
#define RTC_REGISTER_MINUTES    0x02
#define RTC_REGISTER_HOURS      0x04
#define RTC_REGISTER_DAY        0x07
#define RTC_REGISTER_MONTH      0x08
#define RTC_REGISTER_YEAR       0x09
#define RTC_REGISTER_STATUS_A   0x0A
#define RTC_REGISTER_STATUS_B   0x0B
#define RTC_UPDATE_IN_PROGRESS  0x80
#define RTC_DATA_MODE           (1 << 2)

uint8_t rtc_read_register(uint8_t reg) {
    port_outb(PORT_RTC_COMMAND, reg);
    return port_inb(PORT_RTC_DATA);
}

void rtc_wait() {
    while (rtc_read_register(RTC_REGISTER_STATUS_A) & RTC_UPDATE_IN_PROGRESS);
}

uint8_t rtc_is_bcd() {
    return !(rtc_read_register(RTC_REGISTER_STATUS_B) & RTC_DATA_MODE);
}

uint8_t bcd_to_binary(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}

uint8_t rtc_seconds() {
    uint8_t seconds_a;
    uint8_t seconds_b;
    uint8_t is_bcd = rtc_is_bcd();
    
    do {
        rtc_wait();
        seconds_a = rtc_read_register(RTC_REGISTER_SECONDS);
        rtc_wait();
        seconds_b = rtc_read_register(RTC_REGISTER_SECONDS);
    } while (seconds_a != seconds_b);
    
    if (is_bcd) {
        return bcd_to_binary(seconds_b);
    }
    
    return seconds_b;
}

uint8_t rtc_minutes() {
    uint8_t minutes_a;
    uint8_t minutes_b;
    uint8_t is_bcd = rtc_is_bcd();
    
    do {
        rtc_wait();
        minutes_a = rtc_read_register(RTC_REGISTER_MINUTES);
        rtc_wait();
        minutes_b = rtc_read_register(RTC_REGISTER_MINUTES);
    } while (minutes_a != minutes_b);
    
    if (is_bcd) {
        return bcd_to_binary(minutes_b);
    }
    
    return minutes_b;
}

uint8_t rtc_hours() {
    uint8_t hours_a;
    uint8_t hours_b;
    uint8_t is_bcd = rtc_is_bcd();
    
    do {
        rtc_wait();
        hours_a = rtc_read_register(RTC_REGISTER_HOURS);
        rtc_wait();
        hours_b = rtc_read_register(RTC_REGISTER_HOURS);
    } while (hours_a != hours_b);
    
    if (is_bcd) {
        return bcd_to_binary(hours_b);
    }
    
    return hours_b;
}

uint8_t rtc_day() {
    uint8_t day_a;
    uint8_t day_b;
    uint8_t is_bcd = rtc_is_bcd();
    
    do {
        rtc_wait();
        day_a = rtc_read_register(RTC_REGISTER_DAY);
        rtc_wait();
        day_b = rtc_read_register(RTC_REGISTER_DAY);
    } while (day_a != day_b);
    
    if (is_bcd) {
        return bcd_to_binary(day_b);
    }
    
    return day_b;
}

uint8_t rtc_month() {
    uint8_t month_a;
    uint8_t month_b;
    uint8_t is_bcd = rtc_is_bcd();
    
    do {
        rtc_wait();
        month_a = rtc_read_register(RTC_REGISTER_MONTH);
        rtc_wait();
        month_b = rtc_read_register(RTC_REGISTER_MONTH);
    } while (month_a != month_b);
    
    if (is_bcd) {
        return bcd_to_binary(month_b);
    }
    
    return month_b;
}

uint8_t rtc_year() {
    uint8_t year_a;
    uint8_t year_b;
    uint8_t is_bcd = rtc_is_bcd();
    
    do {
        rtc_wait();
        year_a = rtc_read_register(RTC_REGISTER_YEAR);
        rtc_wait();
        year_b = rtc_read_register(RTC_REGISTER_YEAR);
    } while (year_a != year_b);
    
    if (is_bcd) {
        return bcd_to_binary(year_b);
    }
    
    return year_b;
}
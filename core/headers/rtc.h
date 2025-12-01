#ifndef RTC_H
#define RTC_H

#include "kernel_types.h"

typedef struct {
    uint16_t year;  // full year, e.g., 2025
    uint8_t month;  // 1-12
    uint8_t day;    // 1-31
    uint8_t hour;   // 0-23
    uint8_t minute; // 0-59
    uint8_t second; // 0-59
} rtc_datetime_t;

// Read current RTC date/time into out. Uses CMOS ports 0x70/0x71.
void rtc_read(rtc_datetime_t* out);

#endif // RTC_H

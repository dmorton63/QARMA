#include "rtc.h"
#include "io.h"

static inline uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

static inline void wait_for_update_end(void) {
    // Wait until Update-In-Progress (UIP) bit clears (Reg A bit 7)
    while (cmos_read(0x0A) & 0x80) { }
}

static inline uint8_t bcd_to_bin(uint8_t v) {
    return (uint8_t)(((v >> 4) * 10) + (v & 0x0F));
}

void rtc_read(rtc_datetime_t* out) {
    if (!out) return;
    wait_for_update_end();

    uint8_t sec = cmos_read(0x00);
    uint8_t min = cmos_read(0x02);
    uint8_t hour = cmos_read(0x04);
    uint8_t day = cmos_read(0x07);
    uint8_t mon = cmos_read(0x08);
    uint8_t year = cmos_read(0x09);

    uint8_t regB = cmos_read(0x0B);

    // Convert BCD if needed (if bit 2 of regB is 0 => BCD)
    if ((regB & 0x04) == 0) {
        sec = bcd_to_bin(sec);
        min = bcd_to_bin(min);
        hour = bcd_to_bin(hour & 0x7F); // mask 12/24h bit
        day = bcd_to_bin(day);
        mon = bcd_to_bin(mon);
        year = bcd_to_bin(year);
    } else {
        hour &= 0x7F;
    }

    // Convert to 24-hour if needed (if bit 1 of regB is 0 => 12-hour)
    if ((regB & 0x02) == 0) {
        // Bit 7 of hour is PM flag in 12-hour mode
        uint8_t is_pm = hour & 0x80;
        hour &= 0x7F;
        if (is_pm && hour < 12) hour = (uint8_t)(hour + 12);
        if (!is_pm && hour == 12) hour = 0;
    }

    // Year handling: try century register (0x32). If not valid, assume 20xx for <80
    uint8_t century = cmos_read(0x32);
    uint16_t full_year;
    if (century != 0 && century != 0xFF) {
        if ((regB & 0x04) == 0) century = bcd_to_bin(century);
        full_year = (uint16_t)(century * 100 + year);
    } else {
        full_year = (uint16_t)((year < 80) ? (2000 + year) : (1900 + year));
    }

    out->year = full_year;
    out->month = mon;
    out->day = day;
    out->hour = hour;
    out->minute = min;
    out->second = sec;
}

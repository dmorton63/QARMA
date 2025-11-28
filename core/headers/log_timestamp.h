#ifndef LOG_TIMESTAMP_H
#define LOG_TIMESTAMP_H

#include "stdtools.h"
#include "timer.h"
#include "config.h"  // For SERIAL_LOG macros

// Timer frequency (set in interrupts.c)
#define TIMER_FREQUENCY 100

// Macro to print timestamp in format [ticks:milliseconds]
// Simple format for debugging: just show ticks and ms
#define PRINT_TIMESTAMP() \
    do { \
        uint32_t _ts_ticks = get_ticks(); \
        uint32_t _ts_ms = get_system_time_millis(TIMER_FREQUENCY); \
        SERIAL_LOG("["); \
        SERIAL_LOG_DEC("", _ts_ticks); \
        SERIAL_LOG(":"); \
        SERIAL_LOG_DEC("", _ts_ms); \
        SERIAL_LOG("ms] "); \
    } while(0)

// Timestamped logging macros
#define LOG_TS(msg) \
    do { PRINT_TIMESTAMP(); SERIAL_LOG(msg); } while(0)

#define LOG_TS_DEC(msg, val) \
    do { PRINT_TIMESTAMP(); SERIAL_LOG(msg); SERIAL_LOG_DEC("", val); } while(0)

#define LOG_TS_HEX(msg, val) \
    do { PRINT_TIMESTAMP(); SERIAL_LOG(msg); SERIAL_LOG_HEX("", val); } while(0)

#endif // LOG_TIMESTAMP_H

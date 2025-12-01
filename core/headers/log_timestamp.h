#ifndef LOG_TIMESTAMP_H
#define LOG_TIMESTAMP_H

#include "stdtools.h"
#include "timer.h"
#include "config.h"  // For SERIAL_LOG macros
#include "rtc.h"

// Forward declarations to avoid newline-printing macros for timestamp parts
extern void serial_debug(const char* msg);
extern void serial_debug_decimal(uint32_t value);
extern uint64_t get_system_time_millis(uint32_t frequency);
extern void log_print_timestamp(void);
extern bool g_log_use_datetime;

// Timer frequency (set in interrupts.c)
#define TIMER_FREQUENCY 100

// Macro to print timestamp in format [ticks:milliseconds]
// Simple format for debugging: just show ticks and ms
#define PRINT_TIMESTAMP() do { log_print_timestamp(); } while(0)

// Timestamped logging macros
#define LOG_TS(msg) \
    do { PRINT_TIMESTAMP(); SERIAL_LOG(msg); } while(0)

#define LOG_TS_DEC(msg, val) \
    do { PRINT_TIMESTAMP(); SERIAL_LOG(msg); SERIAL_LOG_DEC("", val); } while(0)

#define LOG_TS_HEX(msg, val) \
    do { PRINT_TIMESTAMP(); SERIAL_LOG(msg); SERIAL_LOG_HEX("", val); } while(0)

#endif // LOG_TIMESTAMP_H

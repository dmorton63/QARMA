#pragma once
#include "stdarg.h"
#include "stdtools.h"

#define BOOT_LOG_MAX_LINES 64
#define BOOT_LOG_LINE_LENGTH 128

#define DEBUG_BUFFER_SIZE 4096



void boot_log_push(const char* msg);
void boot_log_push_hex(const char *label, uint32_t value);
void boot_log_flush(void);

void boot_log_push_decimal(const char *label, uint32_t value);

void debug_buffer_clear();

void debug_buffer_append(const char *msg);

void debug_buffer_flush();

void debug_buffer_push(const char *msg);
void debug_buffer_push_hex(const char *label, uint32_t value);
void debug_buffer_push_dec(const char *label, uint32_t value);
void debug_buffer_flush_lines();
void debug_buffer_append_hex(const char* label, uint32_t value);
void debug_buffer_append_dec(const char* label, uint32_t value);

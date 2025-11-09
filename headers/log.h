#pragma once

#include <stdio.h>
#include <stdbool.h>

// Global verbosity toggle
extern bool log_verbose;

// Always-safe conditional macro
#define LOG_IF(cond, fmt, ...) do { if (cond) printf(fmt, ##__VA_ARGS__); } while (0)

// Main logging macro
#define LOG(fmt, ...) LOG_IF(log_verbose, fmt, ##__VA_ARGS__)
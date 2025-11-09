#include "qtime.h"
#include <stdint.h>
#include <stddef.h>
#include <time.h>

uint64_t get_system_time(void) {
    return (uint64_t)time(NULL);  // Or use a tick counter if you're simulating
}
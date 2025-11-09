#include "dispatch_logf.h"
#include <time.h>

void dispatch_logf(const char* format, ...) {
    va_list args;
    va_start(args, format);

    // // Get timestamp
    // time_t now = time(NULL);
    // struct tm* t = localtime(&now);

    // // Print timestamp
    // #ifdef DEBUG
    // printf("[%02d:%02d:%02d] ", t->tm_hour, t->tm_min, t->tm_sec);
    // #endif

    // // Print formatted message
    // vprintf(format, args);
    // #ifdef DEBUG
    // printf("\n");
    // #endif

    va_end(args);
}
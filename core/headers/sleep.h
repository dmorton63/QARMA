#ifndef SLEEP_H
#define SLEEP_H

#include "stdtools.h"
#define MS_PER_TICK 10  // Assuming 100Hz timer interrupt

void sleep_ms(uint32_t ms);
void sleep_us(uint32_t us);  // Microsecond delay for USB HID timing

#endif

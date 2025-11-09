#ifndef PRIORITY_SCHEDULER_H
#define PRIORITY_SCHEDULER_H

#include "subsystem_registry.h"
#include <stdint.h>
#include <time.h>
#include "log.h"

void dispatch_by_priority(void);
void set_subsystem_priority(const char* name, uint8_t priority);

#endif // PRIORITY_SCHEDULER_H
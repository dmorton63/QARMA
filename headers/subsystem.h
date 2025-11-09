#pragma once

#include "log.h"
typedef struct Subsystem_ptr {
	const char* name;
	int (*init)(void);
	void (*shutdown)(void);
} Subsystem_ptr;

//#define MAX_SUBSYSTEMS 16
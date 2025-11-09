#ifndef KEYBOARD_SUBSYSTEM_H
#define KEYBOARD_SUBSYSTEM_H
#include <stdbool.h>
#include "message_manager.h"
#include "time.h"
#include "subsystem_registry.h"  // For DispatchSubsystem
#include "log.h"

//static bool exhausted = false;

bool keyboard_input_exhausted(void);

void keyboard_subsystem_init(void);
void keyboard_subsystem_tick(void);
void keyboard_subsystem(void);
void keyboard_manager_handler(message_t* msg);
// New helper to check if input stream is done
bool keyboard_input_exhausted(void);

#endif
#pragma once

#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include "message_manager.h"  // For message_t

#define MSG_SHUTDOWN_REQUEST 99
#define MSG_WINDOW_CREATE 100
#define MSG_WINDOW_DESTROY 101
#define MSG_WINDOW_RESIZE 102

extern Display* x_display;
extern Window win;

void create_splash_window(void);

void window_subsystem_init(void);

void window_subsystem_init_once(void);

void window_subsystem_tick(void);

void window_manager_tick(void);  // New: proper subsystem tick function

void window_manager_handler(message_t* msg);  // New: message handler

void create_main_window(void);

bool mouse_in_shutdown_region(int x, int y);

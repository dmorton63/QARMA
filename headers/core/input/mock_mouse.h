/**
 * QARMA - Mock Mouse Header
 */

#pragma once

#include "../stdtools.h"
#include "keyboard/keyboard.h"  // For key_event_t

void mock_mouse_init(void);
void mock_mouse_handle_key_event(key_event_t event);
void mock_mouse_update(void);

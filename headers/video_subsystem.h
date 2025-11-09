#ifndef VIDEO_SUBSYSTEM_H
#define VIDEO_SUBSYSTEM_H

#include "message_manager.h"  // For message_t

// Framebuffer control
void video_subsystem_clear(void);
void video_subsystem_put_char(char c);
void video_subsystem_write(const char* str);

// Subsystem lifecycle
void video_subsystem_init(void);
void video_subsystem(void);
void video_manager_tick(void);

void video_manager_shutdown(void);

// Message handling
void video_manager_handler(message_t* msg);

#endif // VIDEO_SUBSYSTEM_H
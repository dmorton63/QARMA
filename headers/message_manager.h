#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "log.h"

typedef struct {
    const char* origin;      // e.g., "keyboard_manager"
    const char* target;      // e.g., "video_manager"
    uint8_t type;            // e.g., GLYPH_INPUT_EVENT, GLYPH_RENDER_REQUEST
    const char* payload;     // message content or pointer to data
    uint64_t timestamp;      // system time
    uint8_t priority;        // 0 = low, 5 = critical
} message_t;

enum {
    MSG_INPUT_EVENT = 1,
    MSG_RENDER_REQUEST,
    MSG_LOG_MESSAGE,
    MSG_SUBSYSTEM_ALERT,
    MSG_METRIC_QUERY
};

#define MAX_QUEUE_SIZE 32

typedef struct {
    message_t messages[MAX_QUEUE_SIZE];
    int head;
    int tail;
} message_queue_t;

#define MAX_MANAGERS 16

typedef void (*manager_handler_t)(message_t*);

typedef struct {
    const char* name;
    manager_handler_t handler;
    message_queue_t inbox;
} manager_entry_t;

// Forward declarations - actual variables are in message_manager.c
extern manager_entry_t manager_table[MAX_MANAGERS];
extern int manager_count;

void send_message(message_t* msg);
void register_manager(const char* name, manager_handler_t handler);
bool enqueue_message(const char *target, const message_t *msg);
message_queue_t *get_manager_inbox(const char *name);
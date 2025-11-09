#include "message_manager.h"
#include <string.h>   // For strcmp
#include <stdio.h>    // For optional logging
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>   // For NULL


// Manager registry - actual definitions
manager_entry_t manager_table[MAX_MANAGERS];
int manager_count = 0;


// Registers a manager and initializes its inbox
void register_manager(const char* name, manager_handler_t handler) {
    if (manager_count >= MAX_MANAGERS) {
        #ifdef DEBUG
        LOG("Manager table full! Cannot register %s\n", name);
        #endif
        return;
    }

    manager_entry_t entry;
    entry.name = name;
    entry.handler = handler;
    entry.inbox.head = 0;
    entry.inbox.tail = 0;

    manager_table[manager_count++] = entry;
    #ifdef DEBUG
    LOG("Manager registered: %s\n", name);
    #endif
}

// Enqueues a message into the target manager's inbox
bool enqueue_message(const char* target, const message_t* msg) {
    for (int i = 0; i < manager_count; ++i) {
        if (strcmp(manager_table[i].name, target) == 0) {
            message_queue_t* q = &manager_table[i].inbox;
            int next = (q->tail + 1) % MAX_QUEUE_SIZE;
            if (next == q->head) {
                #ifdef DEBUG
                LOG("Message queue overflow for %s\n", target);
                #endif
                return false; // Queue full
            }

            q->messages[q->tail] = *msg;
            q->tail = next;
            #ifdef DEBUG
            LOG("Message enqueued to %s\n", target);
            #endif
            return true;
        }
    }
    #ifdef DEBUG
    LOG("Manager '%s' not found!\n", target);
    #endif
    return false; // Target not found
}

// Retrieves the inbox for a given manager
message_queue_t* get_manager_inbox(const char* name) {
    for (int i = 0; i < manager_count; ++i) {
        if (strcmp(manager_table[i].name, name) == 0) {
            return &manager_table[i].inbox;
        }
    }
    return NULL;
}

// Legacy direct dispatch (optional if using queues)
void send_message(message_t* msg) {
    for (int i = 0; i < manager_count; ++i) {
        if (strcmp(manager_table[i].name, msg->target) == 0) {
            manager_table[i].handler(msg);
            return;
        }
    }
    // Optional: log unknown target
}


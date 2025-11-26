/**
 * QARMA - Message System Implementation
 * 
 * Handle-based message routing with priority queues.
 */

#include "core/message_system.h"
#include "core/memory/heap.h"
#include "core/string.h"
#include "core/timer.h"
#include "config.h"

// ============================================================================
// Message Queue Registry
// ============================================================================

#define MAX_MESSAGE_QUEUES 256

static struct {
    message_queue_t queues[MAX_MESSAGE_QUEUES];
    uint32_t queue_count;
    message_stats_t stats;
    bool initialized;
} g_message_system = {0};

// ============================================================================
// Internal Helper Functions
// ============================================================================

/**
 * Find queue by owner handle.
 * @return Pointer to queue, or NULL if not found
 */
static message_queue_t* find_queue(qarma_handle_t owner) {
    for (uint32_t i = 0; i < g_message_system.queue_count; i++) {
        if (g_message_system.queues[i].owner == owner) {
            return &g_message_system.queues[i];
        }
    }
    return NULL;
}

/**
 * Enqueue a message in priority order.
 * @return true if successful, false if queue full
 */
static bool enqueue_message(message_queue_t* queue, qarma_message_t* msg) {
    if (queue->count >= queue->max_size) {
        g_message_system.stats.messages_dropped++;
        return false;
    }
    
    // Insert in priority order (higher priority = lower number = front of queue)
    if (!queue->head || msg->priority < queue->head->priority) {
        // Insert at head
        msg->next = queue->head;
        queue->head = msg;
        if (!queue->tail) {
            queue->tail = msg;
        }
    } else {
        // Find insertion point
        qarma_message_t* current = queue->head;
        while (current->next && current->next->priority <= msg->priority) {
            current = current->next;
        }
        msg->next = current->next;
        current->next = msg;
        if (!msg->next) {
            queue->tail = msg;
        }
    }
    
    queue->count++;
    g_message_system.stats.messages_pending++;
    return true;
}

/**
 * Dequeue next message from queue.
 * @return Message, or NULL if queue empty
 */
static qarma_message_t* dequeue_message(message_queue_t* queue) {
    if (!queue->head) {
        return NULL;
    }
    
    qarma_message_t* msg = queue->head;
    queue->head = msg->next;
    if (!queue->head) {
        queue->tail = NULL;
    }
    
    queue->count--;
    if (g_message_system.stats.messages_pending > 0) {
        g_message_system.stats.messages_pending--;
    }
    
    msg->next = NULL;
    return msg;
}

// ============================================================================
// Public API Implementation
// ============================================================================

void message_system_init(void) {
    if (g_message_system.initialized) {
        SERIAL_LOG("[MSG_SYS] Already initialized\n");
        return;
    }
    
    memset(&g_message_system, 0, sizeof(g_message_system));
    g_message_system.initialized = true;
    
    SERIAL_LOG("[MSG_SYS] Message system initialized\n");
}

void message_system_shutdown(void) {
    if (!g_message_system.initialized) {
        return;
    }
    
    // Free all queued messages
    for (uint32_t i = 0; i < g_message_system.queue_count; i++) {
        message_queue_t* queue = &g_message_system.queues[i];
        while (queue->head) {
            qarma_message_t* msg = dequeue_message(queue);
            if (msg) {
                message_free(msg);
            }
        }
    }
    
    SERIAL_LOG("[MSG_SYS] Shutdown complete. Stats: Sent=");
    SERIAL_LOG_DEC("", g_message_system.stats.messages_sent);
    SERIAL_LOG(" Posted=");
    SERIAL_LOG_DEC("", g_message_system.stats.messages_posted);
    SERIAL_LOG(" Dropped=");
    SERIAL_LOG_DEC("", g_message_system.stats.messages_dropped);
    SERIAL_LOG("\n");
    
    g_message_system.initialized = false;
}

bool message_queue_create(qarma_handle_t owner, message_handler_fn handler, uint32_t max_size) {
    if (!g_message_system.initialized) {
        return false;
    }
    
    if (g_message_system.queue_count >= MAX_MESSAGE_QUEUES) {
        SERIAL_LOG("[MSG_SYS] ERROR: Max queues reached\n");
        return false;
    }
    
    // Check if queue already exists
    if (find_queue(owner)) {
        SERIAL_LOG("[MSG_SYS] ERROR: Queue already exists for handle\n");
        return false;
    }
    
    message_queue_t* queue = &g_message_system.queues[g_message_system.queue_count++];
    queue->owner = owner;
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    queue->max_size = max_size > 0 ? max_size : MESSAGE_QUEUE_SIZE;
    queue->handler = handler;
    queue->processing = false;
    
    g_message_system.stats.active_queues++;
    
    return true;
}

void message_queue_destroy(qarma_handle_t owner) {
    message_queue_t* queue = find_queue(owner);
    if (!queue) {
        return;
    }
    
    // Free all messages in queue
    while (queue->head) {
        qarma_message_t* msg = dequeue_message(queue);
        if (msg) {
            message_free(msg);
        }
    }
    
    // Remove queue by shifting remaining queues
    uint32_t index = queue - g_message_system.queues;
    for (uint32_t i = index; i < g_message_system.queue_count - 1; i++) {
        g_message_system.queues[i] = g_message_system.queues[i + 1];
    }
    g_message_system.queue_count--;
    
    if (g_message_system.stats.active_queues > 0) {
        g_message_system.stats.active_queues--;
    }
}

int32_t message_send(qarma_message_t* msg) {
    if (!g_message_system.initialized || !msg) {
        return -1;
    }
    
    // Validate target handle
    if (!handle_validate(msg->target, HANDLE_TYPE_INVALID)) {
        SERIAL_LOG("[MSG_SYS] ERROR: Invalid target handle\n");
        return -1;
    }
    
    // Find target queue
    message_queue_t* queue = find_queue(msg->target);
    if (!queue || !queue->handler) {
        SERIAL_LOG("[MSG_SYS] ERROR: No queue or handler for target\n");
        return -1;
    }
    
    g_message_system.stats.messages_sent++;
    
    // Call handler directly (synchronous)
    int32_t result = queue->handler(msg->target, msg);
    msg->result = result;
    
    return result;
}

bool message_post(qarma_message_t* msg) {
    if (!g_message_system.initialized || !msg) {
        return false;
    }
    
    // Validate target handle
    if (!handle_validate(msg->target, HANDLE_TYPE_INVALID)) {
        SERIAL_LOG("[MSG_SYS] ERROR: Invalid target handle\n");
        return false;
    }
    
    // Find target queue
    message_queue_t* queue = find_queue(msg->target);
    if (!queue) {
        SERIAL_LOG("[MSG_SYS] ERROR: No queue for target\n");
        return false;
    }
    
    g_message_system.stats.messages_posted++;
    
    // Enqueue message (asynchronous)
    msg->access_flags |= MSG_ACCESS_ASYNC;
    return enqueue_message(queue, msg);
}

uint32_t message_broadcast(qarma_message_t* msg) {
    if (!g_message_system.initialized || !msg) {
        return 0;
    }
    
    msg->access_flags |= MSG_ACCESS_BROADCAST;
    uint32_t recipients = 0;
    
    // Send to all queues
    for (uint32_t i = 0; i < g_message_system.queue_count; i++) {
        message_queue_t* queue = &g_message_system.queues[i];
        
        // Create a copy of the message for each recipient
        qarma_message_t* msg_copy = message_create(msg->type, msg->sender,
                                                   queue->owner, msg->wparam, msg->lparam);
        if (msg_copy) {
            msg_copy->priority = msg->priority;
            msg_copy->access_flags = msg->access_flags;
            msg_copy->data = msg->data;
            msg_copy->data_size = msg->data_size;
            
            if (enqueue_message(queue, msg_copy)) {
                recipients++;
            } else {
                message_free(msg_copy);
            }
        }
    }
    
    g_message_system.stats.broadcasts_sent++;
    
    return recipients;
}

bool message_dispatch_next(qarma_handle_t owner) {
    if (!g_message_system.initialized) {
        return false;
    }
    
    message_queue_t* queue = find_queue(owner);
    if (!queue || !queue->handler) {
        return false;
    }
    
    // Prevent re-entrancy
    if (queue->processing) {
        return false;
    }
    
    qarma_message_t* msg = dequeue_message(queue);
    if (!msg) {
        return false;
    }
    
    queue->processing = true;
    
    // Dispatch message
    int32_t result = queue->handler(owner, msg);
    msg->result = result;
    
    g_message_system.stats.messages_dispatched++;
    
    queue->processing = false;
    
    // Free message
    message_free(msg);
    
    return true;
}

uint32_t message_dispatch_all(qarma_handle_t owner) {
    uint32_t count = 0;
    while (message_dispatch_next(owner)) {
        count++;
    }
    return count;
}

uint32_t message_get_pending_count(qarma_handle_t owner) {
    message_queue_t* queue = find_queue(owner);
    return queue ? queue->count : 0;
}

bool message_has_pending(qarma_handle_t owner) {
    message_queue_t* queue = find_queue(owner);
    return queue && queue->head != NULL;
}

qarma_message_t* message_create(message_type_t type, qarma_handle_t sender,
                                qarma_handle_t target, uint64_t wparam, uint64_t lparam) {
    qarma_message_t* msg = (qarma_message_t*)heap_alloc(sizeof(qarma_message_t));
    if (!msg) {
        return NULL;
    }
    
    memset(msg, 0, sizeof(qarma_message_t));
    
    msg->msg_handle = handle_allocate(HANDLE_TYPE_MESSAGE, msg, NULL);
    msg->sender = sender;
    msg->target = target;
    msg->type = type;
    msg->priority = MSG_PRIORITY_NORMAL;
    msg->access_flags = MSG_ACCESS_READ | MSG_ACCESS_WRITE;
    msg->timestamp = get_ticks();
    msg->wparam = wparam;
    msg->lparam = lparam;
    msg->data = NULL;
    msg->data_size = 0;
    msg->requires_reply = false;
    msg->reply_to = QARMA_INVALID_HANDLE;
    msg->result = 0;
    msg->next = NULL;
    
    return msg;
}

void message_free(qarma_message_t* msg) {
    if (!msg) {
        return;
    }
    
    if (msg->msg_handle != QARMA_INVALID_HANDLE) {
        handle_release(msg->msg_handle);
    }
    
    // Note: We don't free msg->data as it may be owned by someone else
    // The sender is responsible for managing that memory
    
    heap_free(msg);
}

int32_t message_send_simple(message_type_t type, qarma_handle_t sender,
                            qarma_handle_t target, uint64_t wparam, uint64_t lparam) {
    qarma_message_t* msg = message_create(type, sender, target, wparam, lparam);
    if (!msg) {
        return -1;
    }
    
    int32_t result = message_send(msg);
    message_free(msg);
    
    return result;
}

bool message_post_simple(message_type_t type, qarma_handle_t sender,
                        qarma_handle_t target, uint64_t wparam, uint64_t lparam) {
    qarma_message_t* msg = message_create(type, sender, target, wparam, lparam);
    if (!msg) {
        return false;
    }
    
    // Message will be freed when dispatched
    return message_post(msg);
}

void message_get_stats(message_stats_t* stats) {
    if (!stats || !g_message_system.initialized) {
        return;
    }
    
    memcpy(stats, &g_message_system.stats, sizeof(message_stats_t));
}

void message_dump_queue(qarma_handle_t owner) {
    message_queue_t* queue = find_queue(owner);
    if (!queue) {
        SERIAL_LOG("[MSG_SYS] Queue not found\n");
        return;
    }
    
    SERIAL_LOG("[MSG_SYS] Queue for handle 0x");
    SERIAL_LOG_HEX("", (uint32_t)(owner >> 32));
    SERIAL_LOG_HEX("", (uint32_t)owner);
    SERIAL_LOG(" - Messages: ");
    SERIAL_LOG_DEC("", queue->count);
    SERIAL_LOG("/");
    SERIAL_LOG_DEC("", queue->max_size);
    SERIAL_LOG("\n");
    
    qarma_message_t* msg = queue->head;
    uint32_t index = 0;
    while (msg) {
        SERIAL_LOG("  [");
        SERIAL_LOG_DEC("", index++);
        SERIAL_LOG("] Type: 0x");
        SERIAL_LOG_HEX("", msg->type);
        SERIAL_LOG(" Priority: ");
        SERIAL_LOG_DEC("", msg->priority);
        SERIAL_LOG("\n");
        msg = msg->next;
    }
}

void message_dump_all_queues(void) {
    SERIAL_LOG("[MSG_SYS] Active queues: ");
    SERIAL_LOG_DEC("", g_message_system.queue_count);
    SERIAL_LOG("\n");
    
    for (uint32_t i = 0; i < g_message_system.queue_count; i++) {
        message_dump_queue(g_message_system.queues[i].owner);
    }
}

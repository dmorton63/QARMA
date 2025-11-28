/**
 * QARMA - Message System
 * 
 * Handle-based message routing and dispatch system.
 * All inter-component communication flows through this system.
 */

#ifndef MESSAGE_SYSTEM_H
#define MESSAGE_SYSTEM_H

// #include <stdint.h>
// #include <stdbool.h>
#include "stdtools.h"
#include "handle_manager.h"

// ============================================================================
// Message Type Definitions
// ============================================================================

/**
 * Standard message types (similar to Windows message system).
 * Range 0x0000-0x0FFF reserved for system messages.
 * Range 0x1000-0xFFFF available for custom messages.
 */
typedef enum {
    // Null message
    MSG_NULL            = 0x0000,
    
    // Window lifecycle messages
    MSG_CREATE          = 0x0001,
    MSG_DESTROY         = 0x0002,
    MSG_CLOSE           = 0x0003,
    MSG_QUIT            = 0x0012,
    
    // Paint/display messages
    MSG_PAINT           = 0x000F,
    MSG_ERASEBKGND      = 0x0014,
    MSG_INVALIDATE      = 0x0015,
    MSG_SETREDRAW       = 0x000B,
    
    // Input messages - keyboard
    MSG_KEYDOWN         = 0x0100,
    MSG_KEYUP           = 0x0101,
    MSG_CHAR            = 0x0102,
    MSG_SYSKEYDOWN      = 0x0104,
    MSG_SYSKEYUP        = 0x0105,
    
    // Input messages - mouse
    MSG_MOUSEMOVE       = 0x0200,
    MSG_LBUTTONDOWN     = 0x0201,
    MSG_LBUTTONUP       = 0x0202,
    MSG_RBUTTONDOWN     = 0x0204,
    MSG_RBUTTONUP       = 0x0205,
    MSG_MBUTTONDOWN     = 0x0207,
    MSG_MBUTTONUP       = 0x0208,
    MSG_MOUSEWHEEL      = 0x020A,
    
    // Focus messages
    MSG_SETFOCUS        = 0x0007,
    MSG_KILLFOCUS       = 0x0008,
    MSG_ACTIVATE        = 0x0006,
    
    // Size and position messages
    MSG_MOVE            = 0x0003,
    MSG_SIZE            = 0x0005,
    MSG_SIZING          = 0x0214,
    MSG_MOVING          = 0x0216,
    
    // Control messages
    MSG_COMMAND         = 0x0111,
    MSG_NOTIFY          = 0x004E,
    MSG_SETTEXT         = 0x000C,
    MSG_GETTEXT         = 0x000D,
    MSG_GETTEXTLENGTH   = 0x000E,
    MSG_ENABLE          = 0x000A,
    
    // Timer messages
    MSG_TIMER           = 0x0113,
    
    // System messages
    MSG_SYSCOMMAND      = 0x0112,
    MSG_INITDIALOG      = 0x0110,
    
    // Custom messages start here
    MSG_USER            = 0x0400,
    
    // QARMA-specific messages
    MSG_QARMA_TASK_COMPLETE = 0x8000,
    MSG_QARMA_SUBSYSTEM_EVENT = 0x8001,
    MSG_QARMA_ERROR     = 0x8002,
} message_type_t;

// ============================================================================
// Message Access Flags
// ============================================================================

typedef enum {
    MSG_ACCESS_NONE         = 0x00,
    MSG_ACCESS_READ         = 0x01,     // Recipient can read message data
    MSG_ACCESS_WRITE        = 0x02,     // Recipient can modify message data
    MSG_ACCESS_BROADCAST    = 0x04,     // Message can be broadcast
    MSG_ACCESS_SYSTEM       = 0x08,     // System-level message
    MSG_ACCESS_ASYNC        = 0x10,     // Async delivery (post vs send)
} message_access_t;

// ============================================================================
// Message Priority Levels
// ============================================================================

typedef enum {
    MSG_PRIORITY_CRITICAL   = 0,        // System critical (interrupts, etc.)
    MSG_PRIORITY_HIGH       = 1,        // High priority (user input)
    MSG_PRIORITY_NORMAL     = 2,        // Normal priority (default)
    MSG_PRIORITY_LOW        = 3,        // Low priority (background tasks)
    MSG_PRIORITY_IDLE       = 4,        // Idle priority (cleanup, etc.)
} message_priority_t;

// ============================================================================
// Message Structure
// ============================================================================

typedef struct qarma_message {
    qarma_handle_t msg_handle;          // Unique message handle
    qarma_handle_t sender;              // Sender handle
    qarma_handle_t target;              // Target handle (or INVALID for broadcast)
    
    message_type_t type;                // Message type
    message_priority_t priority;        // Message priority
    uint32_t access_flags;              // Access control flags
    
    uint64_t timestamp;                 // Timestamp (ticks)
    
    // Message data (parameters)
    uint64_t wparam;                    // First parameter
    uint64_t lparam;                    // Second parameter
    void* data;                         // Additional data pointer
    size_t data_size;                   // Size of additional data
    
    // Reply handling
    bool requires_reply;                // Does sender expect a reply?
    qarma_handle_t reply_to;            // Original message if this is a reply
    int32_t result;                     // Message result code
    
    struct qarma_message* next;         // For queue linking
} qarma_message_t;

// ============================================================================
// Message Handler Function Type
// ============================================================================

/**
 * Message handler callback function.
 * @param recipient Handle of the recipient
 * @param message The message being delivered
 * @return Result code (0 = handled, non-zero = not handled or error)
 */
typedef int32_t (*message_handler_fn)(qarma_handle_t recipient, qarma_message_t* message);

// ============================================================================
// Message Queue Structure
// ============================================================================

#define MESSAGE_QUEUE_SIZE 256

typedef struct {
    qarma_handle_t owner;               // Queue owner handle
    qarma_message_t* head;              // Queue head
    qarma_message_t* tail;              // Queue tail
    uint32_t count;                     // Number of messages in queue
    uint32_t max_size;                  // Maximum queue size
    message_handler_fn handler;         // Message handler callback
    bool processing;                    // Currently processing messages?
} message_queue_t;

// ============================================================================
// Message System Statistics
// ============================================================================

typedef struct {
    uint64_t messages_sent;             // Total messages sent (sync)
    uint64_t messages_posted;           // Total messages posted (async)
    uint64_t messages_dispatched;       // Total messages dispatched
    uint64_t messages_dropped;          // Messages dropped (queue full)
    uint64_t broadcasts_sent;           // Broadcast messages sent
    uint32_t active_queues;             // Active message queues
    uint32_t messages_pending;          // Total pending messages
} message_stats_t;

// ============================================================================
// API Functions
// ============================================================================

/**
 * Initialize the message system.
 */
void message_system_init(void);

/**
 * Shutdown the message system.
 */
void message_system_shutdown(void);

/**
 * Create a message queue for a handle.
 * @param owner Handle that owns this queue
 * @param handler Message handler function
 * @param max_size Maximum queue size (0 for default)
 * @return true if successful
 */
bool message_queue_create(qarma_handle_t owner, message_handler_fn handler, uint32_t max_size);

/**
 * Destroy a message queue.
 * @param owner Handle whose queue to destroy
 */
void message_queue_destroy(qarma_handle_t owner);

/**
 * Send a message (synchronous - waits for handler to process).
 * @param msg Message to send
 * @return Result code from handler
 */
int32_t message_send(qarma_message_t* msg);

/**
 * Post a message (asynchronous - queues for later processing).
 * @param msg Message to post
 * @return true if queued successfully
 */
bool message_post(qarma_message_t* msg);

/**
 * Broadcast a message to all registered queues.
 * @param msg Message to broadcast
 * @return Number of recipients
 */
uint32_t message_broadcast(qarma_message_t* msg);

/**
 * Dispatch next message from a queue.
 * @param owner Queue owner handle
 * @return true if a message was dispatched
 */
bool message_dispatch_next(qarma_handle_t owner);

/**
 * Dispatch all pending messages from a queue.
 * @param owner Queue owner handle
 * @return Number of messages dispatched
 */
uint32_t message_dispatch_all(qarma_handle_t owner);

/**
 * Get number of pending messages in a queue.
 * @param owner Queue owner handle
 * @return Number of pending messages
 */
uint32_t message_get_pending_count(qarma_handle_t owner);

/**
 * Check if a queue has pending messages.
 * @param owner Queue owner handle
 * @return true if queue has messages
 */
bool message_has_pending(qarma_handle_t owner);

/**
 * Create a message (helper function).
 * @param type Message type
 * @param sender Sender handle
 * @param target Target handle
 * @param wparam First parameter
 * @param lparam Second parameter
 * @return Allocated message (must be freed with message_free)
 */
qarma_message_t* message_create(message_type_t type, qarma_handle_t sender, 
                                qarma_handle_t target, uint64_t wparam, uint64_t lparam);

/**
 * Free a message.
 * @param msg Message to free
 */
void message_free(qarma_message_t* msg);

/**
 * Send a simple message (convenience function).
 * @param type Message type
 * @param sender Sender handle
 * @param target Target handle
 * @param wparam First parameter
 * @param lparam Second parameter
 * @return Result code
 */
int32_t message_send_simple(message_type_t type, qarma_handle_t sender,
                            qarma_handle_t target, uint64_t wparam, uint64_t lparam);

/**
 * Post a simple message (convenience function).
 * @param type Message type
 * @param sender Sender handle
 * @param target Target handle
 * @param wparam First parameter
 * @param lparam Second parameter
 * @return true if queued
 */
bool message_post_simple(message_type_t type, qarma_handle_t sender,
                        qarma_handle_t target, uint64_t wparam, uint64_t lparam);

/**
 * Get message system statistics.
 * @param stats Output statistics structure
 */
void message_get_stats(message_stats_t* stats);

/**
 * Dump message queue contents (debugging).
 * @param owner Queue owner handle
 */
void message_dump_queue(qarma_handle_t owner);

/**
 * Dump all active queues (debugging).
 */
void message_dump_all_queues(void);

#endif // MESSAGE_SYSTEM_H

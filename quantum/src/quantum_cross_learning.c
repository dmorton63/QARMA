/**
 * QARMA - Quantum Cross-System Learning Implementation
 * 
 * Implements message passing and knowledge sharing between qubits.
 */

#include "quantum/quantum_cross_learning.h"
#include "core/memory/heap.h"
#include "core/string.h"
#include "config.h"

// Global message queue (circular buffer)
static cross_message_t g_message_queue[MAX_CROSS_MESSAGES];
static uint32_t g_queue_head = 0;
static uint32_t g_queue_tail = 0;
static uint32_t g_queue_count = 0;

// Global statistics
static cross_learning_stats_t g_stats;

// Global configuration
static cross_learning_config_t g_config = {
    .enabled = TRUE,
    .broadcast_enabled = TRUE,
    .message_priority = 50,
    .max_message_age_ms = 5000  // 5 seconds max age
};

// Best solution tracking (for convergence detection)
static struct {
    bool_t has_solution;
    uint32_t qubit_id;
    uint32_t quality_score;
    void* data;
    uint32_t data_size;
} g_best_solution;

/**
 * Initialize the cross-learning system
 */
void quantum_cross_learning_init(void) {
    SERIAL_LOG_MIN("Cross-Learning: Initializing message system");
    
    // Clear message queue
    g_queue_head = 0;
    g_queue_tail = 0;
    g_queue_count = 0;
    
    // Clear statistics
    memset(&g_stats, 0, sizeof(g_stats));
    
    // Clear best solution
    g_best_solution.has_solution = FALSE;
    g_best_solution.qubit_id = 0;
    g_best_solution.quality_score = 0;
    g_best_solution.data = NULL;
    g_best_solution.data_size = 0;
    
    SERIAL_LOG("Cross-Learning: Ready for inter-qubit communication");
}

/**
 * Helper: Add message to queue
 */
static bool_t enqueue_message(const cross_message_t* msg) {
    if (g_queue_count >= MAX_CROSS_MESSAGES) {
        SERIAL_LOG("Cross-Learning: Queue full, dropping message");
        return FALSE;
    }
    
    // Copy message to queue
    memcpy(&g_message_queue[g_queue_tail], msg, sizeof(cross_message_t));
    
    // Advance tail
    g_queue_tail = (g_queue_tail + 1) % MAX_CROSS_MESSAGES;
    g_queue_count++;
    
    return TRUE;
}

/**
 * Helper: Get next message for a specific qubit
 */
static bool_t dequeue_message(uint32_t qubit_id, cross_message_t* out_msg) {
    if (g_queue_count == 0) {
        return FALSE;
    }
    
    // Search for message relevant to this qubit
    uint32_t idx = g_queue_head;
    for (uint32_t i = 0; i < g_queue_count; i++) {
        cross_message_t* msg = &g_message_queue[idx];
        
        // Check if message is for this qubit (or broadcast)
        // Sender ID of -1 means broadcast
        if (msg->sender_id != qubit_id) {  // Don't receive own messages
            // Copy message
            memcpy(out_msg, msg, sizeof(cross_message_t));
            
            // Remove from queue by shifting
            for (uint32_t j = i; j < g_queue_count - 1; j++) {
                uint32_t src_idx = (g_queue_head + j + 1) % MAX_CROSS_MESSAGES;
                uint32_t dst_idx = (g_queue_head + j) % MAX_CROSS_MESSAGES;
                memcpy(&g_message_queue[dst_idx], &g_message_queue[src_idx], 
                       sizeof(cross_message_t));
            }
            
            g_queue_count--;
            return TRUE;
        }
        
        idx = (idx + 1) % MAX_CROSS_MESSAGES;
    }
    
    return FALSE;
}

/**
 * Send a message from one qubit to others
 */
bool_t cross_learning_send_message(
    uint32_t sender_id,
    cross_message_type_t type,
    void* data,
    uint32_t data_size,
    int32_t target_id
) {
    if (!g_config.enabled) {
        return FALSE;
    }
    
    // Allocate memory for message data
    void* msg_data = heap_alloc(data_size);
    if (!msg_data && data_size > 0) {
        SERIAL_LOG("Cross-Learning: Failed to allocate message data");
        return FALSE;
    }
    
    if (data_size > 0) {
        memcpy(msg_data, data, data_size);
    }
    
    // Create message
    cross_message_t msg = {
        .type = type,
        .sender_id = sender_id,
        .timestamp = 0,  // TODO: Get actual timestamp
        .data_size = data_size,
        .data = msg_data,
        .relevance_score = 75  // Default relevance
    };
    
    // Enqueue message
    if (enqueue_message(&msg)) {
        g_stats.messages_sent++;
        return TRUE;
    }
    
    // Failed to enqueue, free allocated data
    if (msg_data) {
        heap_free(msg_data);
    }
    
    return FALSE;
}

/**
 * Check for messages relevant to a specific qubit
 */
bool_t cross_learning_receive_message(
    uint32_t qubit_id,
    cross_message_t* out_msg
) {
    if (!g_config.enabled) {
        return FALSE;
    }
    
    if (dequeue_message(qubit_id, out_msg)) {
        g_stats.messages_received++;
        return TRUE;
    }
    
    return FALSE;
}

/**
 * Broadcast a "best solution found" to all qubits
 */
void cross_learning_broadcast_best(
    uint32_t sender_id,
    void* solution_data,
    uint32_t solution_size,
    uint32_t quality_score
) {
    SERIAL_LOG("Cross-Learning: Broadcasting best\n");
    
    // Update global best if this is better
    if (!g_best_solution.has_solution || 
        quality_score > g_best_solution.quality_score) {
        
        // Free old solution data if exists
        if (g_best_solution.data) {
            heap_free(g_best_solution.data);
        }
        
        // Allocate new solution data
        g_best_solution.data = heap_alloc(solution_size);
        if (g_best_solution.data) {
            memcpy(g_best_solution.data, solution_data, solution_size);
            g_best_solution.qubit_id = sender_id;
            g_best_solution.quality_score = quality_score;
            g_best_solution.data_size = solution_size;
            g_best_solution.has_solution = TRUE;
            
            SERIAL_LOG("Cross-Learning: New global best\n");
        }
    }
    
    // Broadcast message to all qubits
    cross_learning_send_message(sender_id, MSG_BEST_FOUND, 
                                solution_data, solution_size, -1);
}

/**
 * Share knowledge with other qubits
 */
void cross_learning_share_knowledge(
    uint32_t qubit_id,
    uint32_t knowledge_type,
    void* knowledge_data
) {
    // Package knowledge type with data
    struct {
        uint32_t type;
        uint8_t data[256];
    } package;
    
    package.type = knowledge_type;
    memcpy(package.data, knowledge_data, 256);
    
    cross_learning_send_message(qubit_id, MSG_HINT, 
                                &package, sizeof(package), -1);
    
    SERIAL_LOG("Cross-Learning: Qubit shared knowledge\n");
}

/**
 * Check if any qubit has signaled convergence
 */
bool_t cross_learning_check_convergence(uint32_t* best_qubit_id) {
    if (g_best_solution.has_solution) {
        if (best_qubit_id) {
            *best_qubit_id = g_best_solution.qubit_id;
        }
        return TRUE;
    }
    
    return FALSE;
}

/**
 * Get cross-learning statistics
 */
void cross_learning_get_stats(cross_learning_stats_t* stats) {
    if (stats) {
        memcpy(stats, &g_stats, sizeof(cross_learning_stats_t));
    }
}

/**
 * Configure cross-learning behavior
 */
void cross_learning_configure(const cross_learning_config_t* config) {
    if (config) {
        memcpy(&g_config, config, sizeof(cross_learning_config_t));
        SERIAL_LOG("Cross-Learning: Configuration updated\n");
    }
}

/**
 * Print cross-learning statistics
 */
void cross_learning_print_stats(void) {
    SERIAL_LOG("\nCross-Learning Statistics:\n");
    SERIAL_LOG("  Messages sent: ");
    SERIAL_LOG_HEX("", g_stats.messages_sent);
    SERIAL_LOG("\n  Messages received: ");
    SERIAL_LOG_HEX("", g_stats.messages_received);
    SERIAL_LOG("\n  Helpful: ");
    SERIAL_LOG_HEX("", g_stats.helpful_messages);
    SERIAL_LOG("\n  Ignored: ");
    SERIAL_LOG_HEX("", g_stats.ignored_messages);
    SERIAL_LOG("\n");
    
    if (g_best_solution.has_solution) {
        SERIAL_LOG("  Best solution: Qubit ");
        SERIAL_LOG_HEX("", g_best_solution.qubit_id);
        SERIAL_LOG(" quality=");
        SERIAL_LOG_HEX("", g_best_solution.quality_score);
        SERIAL_LOG("\n");
    }
}

/**
 * Clear all pending messages
 */
void cross_learning_clear_messages(void) {
    // Free all message data
    uint32_t idx = g_queue_head;
    for (uint32_t i = 0; i < g_queue_count; i++) {
        cross_message_t* msg = &g_message_queue[idx];
        if (msg->data) {
            heap_free(msg->data);
            msg->data = NULL;
        }
        idx = (idx + 1) % MAX_CROSS_MESSAGES;
    }
    
    // Reset queue
    g_queue_head = 0;
    g_queue_tail = 0;
    g_queue_count = 0;
    
    // Clear best solution
    if (g_best_solution.data) {
        heap_free(g_best_solution.data);
        g_best_solution.data = NULL;
    }
    g_best_solution.has_solution = FALSE;
}

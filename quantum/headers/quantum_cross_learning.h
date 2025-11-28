/**
 * QARMA - Quantum Cross-System Learning
 * 
 * Enables qubits to communicate and share knowledge during execution.
 * Qubits can broadcast partial results, receive updates from others,
 * and benefit from collective learning to accelerate convergence.
 */

#ifndef QUANTUM_CROSS_LEARNING_H
#define QUANTUM_CROSS_LEARNING_H

#include "kernel_types.h"

// Type compatibility
typedef bool bool_t;
#define TRUE true
#define FALSE false

// Maximum number of messages in the shared queue
#define MAX_CROSS_MESSAGES 64

// Message types for cross-qubit communication
typedef enum {
    MSG_PARTIAL_RESULT,     // Share an intermediate computation result
    MSG_BEST_FOUND,         // Broadcast best solution found so far
    MSG_HINT,               // Provide a hint/suggestion to other qubits
    MSG_CONVERGENCE,        // Signal approaching solution
    MSG_ABORT_BRANCH        // Suggest others avoid a dead-end path
} cross_message_type_t;

// Message structure for inter-qubit communication
typedef struct {
    cross_message_type_t type;
    uint32_t sender_id;          // Which qubit sent this
    uint32_t timestamp;          // When it was sent
    uint32_t data_size;          // Size of payload
    void* data;                  // Message payload
    uint32_t relevance_score;    // How relevant (0-100)
} cross_message_t;

// Shared learning statistics
typedef struct {
    uint32_t messages_sent;
    uint32_t messages_received;
    uint32_t helpful_messages;   // Messages that improved performance
    uint32_t ignored_messages;   // Messages that were not useful
    uint32_t convergence_speedup; // % faster due to sharing (estimated)
} cross_learning_stats_t;

// Configuration for cross-learning behavior
typedef struct {
    bool_t enabled;              // Enable/disable cross-learning
    bool_t broadcast_enabled;    // Allow broadcasting to all qubits
    uint32_t message_priority;   // Higher priority messages processed first
    uint32_t max_message_age_ms; // Discard messages older than this
} cross_learning_config_t;

/**
 * Initialize the cross-learning system
 */
void quantum_cross_learning_init(void);

/**
 * Send a message from one qubit to others
 * 
 * @param sender_id ID of the sending qubit
 * @param type Type of message being sent
 * @param data Payload data
 * @param data_size Size of payload in bytes
 * @param target_id Target qubit ID (or -1 for broadcast)
 * @return TRUE if message was queued successfully
 */
bool_t cross_learning_send_message(
    uint32_t sender_id,
    cross_message_type_t type,
    void* data,
    uint32_t data_size,
    int32_t target_id
);

/**
 * Check for messages relevant to a specific qubit
 * 
 * @param qubit_id ID of the qubit checking for messages
 * @param out_msg Output buffer for received message
 * @return TRUE if a message was received
 */
bool_t cross_learning_receive_message(
    uint32_t qubit_id,
    cross_message_t* out_msg
);

/**
 * Broadcast a "best solution found" to all qubits
 * Causes other qubits to compare against their current best
 * 
 * @param sender_id ID of the qubit with the best solution
 * @param solution_data The solution data
 * @param solution_size Size of solution
 * @param quality_score Quality metric (higher is better)
 */
void cross_learning_broadcast_best(
    uint32_t sender_id,
    void* solution_data,
    uint32_t solution_size,
    uint32_t quality_score
);

/**
 * Signal that a qubit has learned something useful
 * Other qubits can query this knowledge
 * 
 * @param qubit_id ID of the learning qubit
 * @param knowledge_type Type of knowledge gained
 * @param knowledge_data The learned information
 */
void cross_learning_share_knowledge(
    uint32_t qubit_id,
    uint32_t knowledge_type,
    void* knowledge_data
);

/**
 * Check if any qubit has signaled convergence
 * Allows early termination if solution is found
 * 
 * @param best_qubit_id Output: ID of qubit with best solution
 * @return TRUE if convergence detected
 */
bool_t cross_learning_check_convergence(uint32_t* best_qubit_id);

/**
 * Get cross-learning statistics
 * 
 * @param stats Output buffer for statistics
 */
void cross_learning_get_stats(cross_learning_stats_t* stats);

/**
 * Configure cross-learning behavior
 * 
 * @param config Configuration settings
 */
void cross_learning_configure(const cross_learning_config_t* config);

/**
 * Print cross-learning statistics to serial log
 */
void cross_learning_print_stats(void);

/**
 * Clear all pending messages (call between register executions)
 */
void cross_learning_clear_messages(void);

#endif // QUANTUM_CROSS_LEARNING_H

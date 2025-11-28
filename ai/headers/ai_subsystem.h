/**
 * QARMA - Embedded AI Subsystem
 * 
 * Built-in artificial intelligence for system optimization, security,
 * and predictive resource management.
 */

#ifndef AI_SUBSYSTEM_H
#define AI_SUBSYSTEM_H

#include "kernel_types.h"
#include "core/kernel.h"

// AI agent types
typedef enum {
    AI_AGENT_OPTIMIZER     = 0,    // System performance optimizer
    AI_AGENT_SECURITY      = 1,    // Security threat detector
    AI_AGENT_SCHEDULER     = 2,    // Intelligent scheduler
    AI_AGENT_RESOURCE      = 3,    // Resource manager
    AI_AGENT_PREDICTOR     = 4,    // Predictive analyzer
    AI_AGENT_ANOMALY       = 5,    // Anomaly detector
    AI_AGENT_ADAPTIVE      = 6     // Adaptive system controller
} ai_agent_type_t;

// AI learning modes
typedef enum {
    AI_LEARNING_SUPERVISED   = 0,  // Supervised learning
    AI_LEARNING_UNSUPERVISED = 1,  // Unsupervised learning
    AI_LEARNING_REINFORCEMENT= 2,  // Reinforcement learning
    AI_LEARNING_TRANSFER     = 3,  // Transfer learning
    AI_LEARNING_ONLINE       = 4   // Online learning
} ai_learning_mode_t;

// Neural network layer types
typedef enum {
    NN_LAYER_INPUT      = 0,       // Input layer
    NN_LAYER_HIDDEN     = 1,       // Hidden layer
    NN_LAYER_OUTPUT     = 2,       // Output layer
    NN_LAYER_CONV       = 3,       // Convolutional layer
    NN_LAYER_POOL       = 4,       // Pooling layer
    NN_LAYER_RECURRENT  = 5        // Recurrent layer (RNN/LSTM)
} nn_layer_type_t;

// Activation functions
typedef enum {
    ACTIVATION_SIGMOID  = 0,       // Sigmoid function
    ACTIVATION_TANH     = 1,       // Hyperbolic tangent
    ACTIVATION_RELU     = 2,       // Rectified Linear Unit
    ACTIVATION_LEAKY_RELU = 3,     // Leaky ReLU
    ACTIVATION_SOFTMAX  = 4        // Softmax (for output layer)
} activation_function_t;

// Neural network layer
typedef struct nn_layer {
    nn_layer_type_t type;          // Layer type
    uint32_t neuron_count;         // Number of neurons
    uint32_t input_count;          // Number of inputs per neuron
    
    float* weights;                // Weight matrix
    float* biases;                 // Bias vector
    float* outputs;                // Output values
    float* gradients;              // Gradients for backpropagation
    
    activation_function_t activation; // Activation function
    
    struct nn_layer* next;         // Next layer
    struct nn_layer* prev;         // Previous layer
} nn_layer_t;

// Neural network structure
typedef struct neural_network {
    uint32_t layer_count;          // Number of layers
    nn_layer_t* input_layer;       // Input layer
    nn_layer_t* output_layer;      // Output layer
    nn_layer_t* layers;            // All layers
    
    float learning_rate;           // Learning rate
    uint32_t epoch_count;          // Training epochs completed
    float accuracy;                // Current accuracy
    float loss;                    // Current loss
    
} neural_network_t;

// Training data sample
typedef struct training_sample {
    float* inputs;                 // Input vector
    float* outputs;                // Expected output vector
    uint32_t input_size;           // Size of input vector
    uint32_t output_size;          // Size of output vector
    
    struct training_sample* next;  // Next sample in dataset
} training_sample_t;

// AI agent structure
typedef struct ai_agent {
    uint32_t agent_id;             // Unique agent ID
    char name[32];                 // Agent name
    ai_agent_type_t type;          // Agent type
    
    // Neural network
    neural_network_t* network;     // Associated neural network
    ai_learning_mode_t learning_mode; // Learning mode
    
    // Training data
    training_sample_t* training_data; // Training dataset
    uint32_t sample_count;         // Number of training samples
    
    // Agent state
    bool active;                   // Agent is active
    bool learning;                 // Agent is in learning mode
    uint32_t decision_count;       // Number of decisions made
    float confidence;              // Confidence in decisions
    
    // Performance metrics
    uint64_t cpu_cycles_used;      // CPU cycles consumed
    uint32_t memory_usage;         // Memory usage in bytes
    uint32_t successful_predictions; // Successful predictions
    uint32_t failed_predictions;   // Failed predictions
    
    // Agent callbacks
    void (*decision_callback)(struct ai_agent* agent, void* input, void* output);
    void (*learning_callback)(struct ai_agent* agent, void* feedback);
    
    struct ai_agent* next;         // Next agent in system
} ai_agent_t;

// System metrics for AI analysis
typedef struct system_metrics {
    // CPU metrics
    uint32_t cpu_utilization[64];  // Per-core utilization
    uint32_t cpu_frequency[64];    // Per-core frequency
    uint64_t cpu_cycles_total;     // Total CPU cycles
    
    // Memory metrics
    uint64_t memory_total;         // Total system memory
    uint64_t memory_used;          // Used memory
    uint64_t memory_free;          // Free memory
    uint32_t page_faults;          // Page fault count
    
    // I/O metrics
    uint64_t disk_reads;           // Disk read operations
    uint64_t disk_writes;          // Disk write operations
    uint64_t network_packets_in;   // Network packets received
    uint64_t network_packets_out;  // Network packets sent
    
    // Process metrics
    uint32_t process_count;        // Active process count
    uint32_t thread_count;         // Active thread count
    uint32_t context_switches;     // Context switch count
    
    // Quantum metrics
    uint32_t quantum_processes;    // Quantum process count
    uint32_t entangled_pairs;      // Entangled process pairs
    uint32_t decoherence_events;   // Decoherence events
    
    uint64_t timestamp;            // Timestamp of metrics
} system_metrics_t;

// AI subsystem statistics
typedef struct {
    uint32_t total_agents;         // Total AI agents
    uint32_t active_agents;        // Currently active agents
    uint32_t learning_agents;      // Agents in learning mode
    uint64_t total_decisions;      // Total decisions made
    uint64_t correct_predictions;  // Correct predictions
    float average_accuracy;        // Average prediction accuracy
    uint64_t training_cycles;      // Training cycles completed
    uint32_t models_trained;       // Number of trained models
} ai_subsystem_stats_t;

// Function declarations

// Subsystem initialization
void ai_subsystem_init(void);
void ai_hardware_init(void);

// Agent management
ai_agent_t* ai_agent_create(const char* name, ai_agent_type_t type);
void ai_agent_destroy(ai_agent_t* agent);
void ai_agent_activate(ai_agent_t* agent);
void ai_agent_deactivate(ai_agent_t* agent);
void ai_agent_set_learning_mode(ai_agent_t* agent, ai_learning_mode_t mode);

// Neural network operations
neural_network_t* nn_create(uint32_t layer_count);
void nn_destroy(neural_network_t* network);
nn_layer_t* nn_add_layer(neural_network_t* network, nn_layer_type_t type, 
                        uint32_t neuron_count, activation_function_t activation);
void nn_connect_layers(neural_network_t* network);
void nn_initialize_weights(neural_network_t* network);

// Training and inference
void nn_forward_pass(neural_network_t* network, float* inputs);
void nn_backward_pass(neural_network_t* network, float* expected_outputs);
void nn_train_batch(neural_network_t* network, training_sample_t* samples, uint32_t batch_size);
float* nn_predict(neural_network_t* network, float* inputs);

// Training data management
training_sample_t* training_sample_create(float* inputs, float* outputs, 
                                         uint32_t input_size, uint32_t output_size);
void training_sample_destroy(training_sample_t* sample);
void ai_agent_add_training_sample(ai_agent_t* agent, training_sample_t* sample);

// System optimization
void ai_system_optimize(void);
void ai_optimize_scheduler(system_metrics_t* metrics);
void ai_optimize_memory_allocation(system_metrics_t* metrics);
void ai_optimize_power_management(system_metrics_t* metrics);

// Security and threat detection
void ai_security_monitor(system_metrics_t* metrics);
bool ai_detect_anomaly(system_metrics_t* current, system_metrics_t* baseline);
void ai_analyze_network_traffic(void* packet_data, size_t packet_size);
void ai_monitor_process_behavior(uint32_t pid, void* behavior_data);

// Predictive analysis
void ai_predict_resource_usage(system_metrics_t* history, uint32_t history_size);
void ai_predict_system_load(void);
void ai_predict_memory_pressure(void);

// System metrics collection
void collect_system_metrics(system_metrics_t* metrics);
void update_cpu_metrics(system_metrics_t* metrics);
void update_memory_metrics(system_metrics_t* metrics);
void update_io_metrics(system_metrics_t* metrics);

// Statistics and monitoring
ai_subsystem_stats_t* ai_get_subsystem_stats(void);
void ai_print_agent_info(ai_agent_t* agent);
void ai_debug_dump_networks(void);

// Core management integration
bool ai_request_cores(uint32_t count);
void ai_release_cores(void);
uint32_t ai_get_allocated_cores(void);
bool ai_run_on_dedicated_core(void (*function)(void*), void* data);

// Utility functions
float sigmoid(float x);
float tanh_activation(float x);
float relu(float x);
float leaky_relu(float x, float alpha);
void softmax(float* inputs, float* outputs, uint32_t size);

#endif // AI_SUBSYSTEM_H
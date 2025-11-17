#pragma once

#include "../stdtools.h"

#define MAX_SUBSYSTEMS 32

typedef enum {
    SUBSYSTEM_TYPE_CORE,
    SUBSYSTEM_TYPE_DRIVER,
    SUBSYSTEM_TYPE_GRAPHICS,
    SUBSYSTEM_TYPE_AUDIO,
    SUBSYSTEM_TYPE_NETWORK,
    SUBSYSTEM_TYPE_AI,
    SUBSYSTEM_TYPE_VIDEO,
    SUBSYSTEM_TYPE_FILESYSTEM,
    SUBSYSTEM_TYPE_QUANTUM
} subsystem_type_t;

typedef enum {
    SUBSYSTEM_STATE_STARTED,
    SUBSYSTEM_STATE_STOPPED,
    SUBSYSTEM_STATE_RUNNING,
    SUBSYSTEM_STATE_ERROR,
    SUBSYSTEM_STATE_RESTARTING 
} subsystem_state_t;

typedef struct {
    uint16_t id;
    const char *name;
    subsystem_type_t type;
    subsystem_state_t state;
    void (*start)(void);
    void (*stop)(void);
    void (*restart)(void);
    void (*message_handler)(void *msg);
    uint32_t memory_limit_kb;
    uint8_t cpu_affinity_mask;
    uint32_t stats_uptime_ms;
    uint32_t stats_messages_handled;
} subsystem_t;



void subsystem_registry_init(void);
bool subsystem_register(subsystem_t *subsystem,char* name ,  uint16_t id);
subsystem_t *subsystem_lookup(uint16_t id);
void subsystem_start(uint16_t id);
void subsystem_stop(uint16_t id);
void subsystem_restart(uint16_t id);
void subsystem_broadcast(void *msg);
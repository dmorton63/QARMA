#include "subsystem_registry.h"
#include "../stdtools.h"
#include "config.h"

static subsystem_t registry[MAX_SUBSYSTEMS];

void subsystem_registry_init(void)
{
    for (int i = 0; i < MAX_SUBSYSTEMS; i++) {

        // Initialize registry entries to default
        registry[i].id = 0;
        registry[i].name = NULL;
        registry[i].type = SUBSYSTEM_TYPE_CORE;
        registry[i].state = SUBSYSTEM_STATE_STOPPED;
        registry[i].start = NULL;
        registry[i].stop = NULL;
        registry[i].restart = NULL;
        registry[i].message_handler = NULL;
        registry[i].memory_limit_kb = 0;
        registry[i].cpu_affinity_mask = 0;
        registry[i].stats_uptime_ms = 0;
        registry[i].stats_messages_handled = 0;

    }
}

bool subsystem_register(subsystem_t *subsystem,char* name ,  uint16_t id)
{
    if (subsystem == NULL) {
        return false;
    }
    SERIAL_LOG("Subsystem registered\n");
    for (int i = 0; i < MAX_SUBSYSTEMS; i++) {
        if (registry[i].id == 0) {
            // Empty slot found, register subsystem
            registry[i] = *subsystem;
            return true;
        }
    }

    return false;
}

subsystem_t *subsystem_lookup(uint16_t id)
{
    for (int i = 0; i < MAX_SUBSYSTEMS; i++) {
        // Lookup logic here
        if (registry[i].id == id) {
            return &registry[i];
        }
    }

    return NULL;
}

void subsystem_start(uint16_t id)
{
    subsystem_t *subsystem = subsystem_lookup(id);
    if (subsystem != NULL && subsystem->start != NULL) {
        subsystem->state = SUBSYSTEM_STATE_STARTED;
        subsystem->start();

    }
}

void subsystem_stop(uint16_t id)
{
    subsystem_t *subsystem = subsystem_lookup(id);
    if (subsystem != NULL && subsystem->stop != NULL) {
        subsystem->stop();
        subsystem->state = SUBSYSTEM_STATE_STOPPED;
    }
}

void subsystem_restart(uint16_t id)
{
    subsystem_stop(id);
    subsystem_start(id);
}

void subsystem_broadcast(void *msg)
{
    for (int i = 0; i < MAX_SUBSYSTEMS; i++) {
        // Broadcast logic here
        if (registry[i].message_handler != NULL && registry[i].type == SUBSYSTEM_TYPE_VIDEO) {
            registry[i].message_handler(msg);
        }
    }
}

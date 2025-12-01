#pragma once
#include "kernel_types.h"

typedef enum {
    PM_PROTO_UDP = 17,
    PM_PROTO_TCP = 6
} pm_protocol_t;

typedef enum {
    PM_DIR_INBOUND = 0,
    PM_DIR_OUTBOUND = 1
} pm_direction_t;

void port_manager_init(void);
bool port_allow(pm_protocol_t proto, uint16_t port, pm_direction_t dir);
bool port_block(pm_protocol_t proto, uint16_t port, pm_direction_t dir);
bool port_is_allowed(pm_protocol_t proto, uint16_t port, pm_direction_t dir);
void port_list(pm_protocol_t proto, pm_direction_t dir);

// Debug: write a short summary of inbound-allowed ports to netlog
void port_dump_inbound_to_netlog(void);

// Persistence
int port_manager_save(void);
int port_manager_load(void);

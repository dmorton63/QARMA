#ifndef UDP_H
#define UDP_H

#include "kernel_types.h"
#include "network_subsystem.h"

// UDP header
typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) udp_header_t;

// Functions
void udp_init(void);
void udp_receive(net_device_t* dev, ipv4_addr_t* src_ip, const uint8_t* data, uint32_t len);
int udp_send(net_device_t* dev, ipv4_addr_t* dest_ip, uint16_t src_port, uint16_t dest_port,
             const uint8_t* data, uint32_t len);

#endif // UDP_H

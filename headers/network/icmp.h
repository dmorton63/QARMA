#ifndef ICMP_H
#define ICMP_H

#include "kernel_types.h"
#include "network_subsystem.h"

// ICMP header
typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint32_t rest;  // Varies by type/code
} __attribute__((packed)) icmp_header_t;

// ICMP types
#define ICMP_TYPE_ECHO_REPLY   0
#define ICMP_TYPE_ECHO_REQUEST 8

// Functions
void icmp_init(void);
void icmp_receive(net_device_t* dev, ipv4_addr_t* src_ip, const uint8_t* data, uint32_t len);
int icmp_send_echo_request(net_device_t* dev, ipv4_addr_t* dest_ip, uint16_t id, uint16_t seq);

#endif // ICMP_H

#ifndef IPV4_H
#define IPV4_H

#include "kernel_types.h"
#include "network_subsystem.h"

// IPv4 header (20 bytes minimum)
typedef struct {
    uint8_t version_ihl;    // Version (4 bits) + IHL (4 bits)
    uint8_t tos;            // Type of Service
    uint16_t total_length;  // Total length
    uint16_t id;            // Identification
    uint16_t flags_offset;  // Flags (3 bits) + Fragment offset (13 bits)
    uint8_t ttl;            // Time to Live
    uint8_t protocol;       // Protocol
    uint16_t checksum;      // Header checksum
    ipv4_addr_t src_ip;     // Source IP
    ipv4_addr_t dest_ip;    // Destination IP
} __attribute__((packed)) ipv4_header_t;

// IP protocols
#define IP_PROTO_ICMP   1
#define IP_PROTO_TCP    6
#define IP_PROTO_UDP    17

// Functions
void ipv4_init(void);
void ipv4_receive(net_device_t* dev, const uint8_t* data, uint32_t len);
int ipv4_send(net_device_t* dev, ipv4_addr_t* dest_ip, uint8_t protocol,
              const uint8_t* payload, uint32_t payload_len);
uint16_t ipv4_checksum(const uint8_t* data, uint32_t len);

#endif // IPV4_H

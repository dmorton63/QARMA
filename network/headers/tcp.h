#ifndef TCP_H
#define TCP_H

#include "kernel_types.h"
#include "network_subsystem.h"

// TCP header (20 bytes minimum)
typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t data_offset_flags;  // Data offset (4 bits) + Reserved (3 bits) + NS flag (1 bit)
    uint8_t flags;              // CWR, ECE, URG, ACK, PSH, RST, SYN, FIN
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_ptr;
} __attribute__((packed)) tcp_header_t;

// Endian helpers for clarity (host <-> network)
static inline uint16_t tcp_htons(uint16_t v) { return __builtin_bswap16(v); }
static inline uint16_t tcp_ntohs(uint16_t v) { return __builtin_bswap16(v); }
static inline uint32_t tcp_htonl(uint32_t v) { return __builtin_bswap32(v); }
static inline uint32_t tcp_ntohl(uint32_t v) { return __builtin_bswap32(v); }

// TCP flags
#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_FLAG_URG 0x20

// Functions
void tcp_init(void);
void tcp_receive(net_device_t* dev, ipv4_addr_t* src_ip, const uint8_t* data, uint32_t len);

// Minimal single-connection client API
typedef struct {
    bool connected;
    ipv4_addr_t remote_ip;
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t seq_num;
    uint32_t ack_num;
} tcp_client_state_t;

// Initiate a minimal TCP connect (SYN)
int tcp_connect(net_device_t* dev, ipv4_addr_t* dest_ip, uint16_t local_port, uint16_t remote_port);
// Minimal one-shot send (no retransmission)
int tcp_send(net_device_t* dev, ipv4_addr_t* dest_ip, uint16_t local_port, uint16_t remote_port,
             const uint8_t* data, uint32_t len);

// Debug snapshot of last SYN header (host-order values)
typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint8_t flags;
    uint8_t data_offset;
    uint16_t window_size;
} tcp_last_syn_t;
extern tcp_last_syn_t g_tcp_last_syn;

#endif // TCP_H

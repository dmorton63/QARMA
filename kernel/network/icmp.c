#include "icmp.h"
#include "ipv4.h"
#include "graphics/graphics.h"
#include "core/string.h"
#include "drivers/net/e1000.h"

void icmp_init(void) {
    gfx_print("ICMP layer initialized\n");
}

int icmp_send_echo_request(net_device_t* dev, ipv4_addr_t* dest_ip, uint16_t id, uint16_t seq) {
    extern void serial_debug(const char*);
    
    serial_debug("[ICMP: start]\\n");
    
    uint8_t packet[64];
    icmp_header_t* icmp = (icmp_header_t*)packet;
    
    icmp->type = ICMP_TYPE_ECHO_REQUEST;
    icmp->code = 0;
    icmp->checksum = 0;
    
    serial_debug("[ICMP: fill]\\n");
    
    // Store ID and sequence in 'rest' field
    uint16_t* rest_data = (uint16_t*)&icmp->rest;
    rest_data[0] = __builtin_bswap16(id);
    rest_data[1] = __builtin_bswap16(seq);
    
    // Add some payload data
    for (int i = 0; i < 32; i++) {
        packet[sizeof(icmp_header_t) + i] = 0x10 + i;
    }
    
    serial_debug("[ICMP: checksum]\\n");
    
    // Calculate checksum
    icmp->checksum = ipv4_checksum(packet, sizeof(icmp_header_t) + 32);
    
    serial_debug("[ICMP: send ipv4]\\n");
    
    int result = ipv4_send(dev, dest_ip, IP_PROTO_ICMP, packet, sizeof(icmp_header_t) + 32);
    
    serial_debug("[ICMP: done]\\n");
    
    return result;
}

void icmp_receive(net_device_t* dev, ipv4_addr_t* src_ip, const uint8_t* data, uint32_t len) {
    extern void serial_debug(const char*);
    
    serial_debug("[ICMP_RX: start]\\n");
    
    if (len < sizeof(icmp_header_t)) {
        serial_debug("[ICMP_RX: too short]\\n");
        return;
    }
    
    icmp_header_t* icmp = (icmp_header_t*)data;
    
    serial_debug("[ICMP_RX: check type]\\n");
    
    if (icmp->type == ICMP_TYPE_ECHO_REQUEST) {
        serial_debug("[ICMP_RX: echo request]\\n");
        // Send echo reply
        uint8_t reply[1500];
        memcpy(reply, data, len);
        
        icmp_header_t* reply_icmp = (icmp_header_t*)reply;
        reply_icmp->type = ICMP_TYPE_ECHO_REPLY;
        reply_icmp->code = 0;
        reply_icmp->checksum = 0;
        reply_icmp->checksum = ipv4_checksum(reply, len);
        
        ipv4_send(dev, src_ip, IP_PROTO_ICMP, reply, len);
        
        gfx_print("ICMP: Sent echo reply\n");
    }
    else if (icmp->type == ICMP_TYPE_ECHO_REPLY) {
        serial_debug("[ICMP_RX: echo reply!]\\n");
        gfx_print("Reply from ");
        extern void gfx_print_hex(uint32_t val);
        gfx_print_hex(src_ip->addr[0]); gfx_print(".");
        gfx_print_hex(src_ip->addr[1]); gfx_print(".");
        gfx_print_hex(src_ip->addr[2]); gfx_print(".");
        gfx_print_hex(src_ip->addr[3]);
        gfx_print(": bytes=");
        gfx_print_hex(len - sizeof(icmp_header_t));
        gfx_print("\n");
    }
    
    serial_debug("[ICMP_RX: done]\\n");
}

// Simple wrapper for command line
void icmp_send_echo(uint32_t dest_ip) {
    extern void gfx_printf(const char*, ...);
    extern void gfx_print(const char*);
    extern net_device_t* network_get_default_device(void);
    
    // Get the default network device
    net_device_t* dev = network_get_default_device();
    if (!dev) {
        gfx_print("No network device available\n");
        return;
    }
    
    // Set up destination IP
    ipv4_addr_t dest;
    dest.addr[0] = (dest_ip >> 24) & 0xFF;
    dest.addr[1] = (dest_ip >> 16) & 0xFF;
    dest.addr[2] = (dest_ip >> 8) & 0xFF;
    dest.addr[3] = dest_ip & 0xFF;
    
    // Send echo request
    int result = icmp_send_echo_request(dev, &dest, 1, 1);
    
    if (result == 0) {
        gfx_print("Sent ICMP echo request\n");
    } else {
        gfx_print("Failed to send ICMP\n");
    }
}


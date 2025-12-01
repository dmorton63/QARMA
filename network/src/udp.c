#include "udp.h"
#include "ipv4.h"
#include "port_manager.h"
#include "graphics.h"
#include "string.h"

void udp_init(void) {
    gfx_print("UDP layer initialized\n");
}

int udp_send(net_device_t* dev, ipv4_addr_t* dest_ip, uint16_t src_port, uint16_t dest_port,
             const uint8_t* data, uint32_t len) {
    // Enforce outbound policy
    if (!port_is_allowed(PM_PROTO_UDP, src_port, PM_DIR_OUTBOUND)) {
        gfx_print("UDP: outbound blocked for port "); extern void gfx_print_decimal(uint32_t); gfx_print_decimal(src_port); gfx_print("\n");
        return -1;
    }
    uint8_t packet[1500];
    udp_header_t* udp = (udp_header_t*)packet;
    
    udp->src_port = __builtin_bswap16(src_port);
    udp->dest_port = __builtin_bswap16(dest_port);
    udp->length = __builtin_bswap16(sizeof(udp_header_t) + len);
    udp->checksum = 0;  // Optional for IPv4
    
    // Copy payload
    memcpy(packet + sizeof(udp_header_t), data, len);
    
    return ipv4_send(dev, dest_ip, IP_PROTO_UDP, packet, sizeof(udp_header_t) + len);
}

void udp_receive(net_device_t* dev, ipv4_addr_t* src_ip, const uint8_t* data, uint32_t len) {
    (void)dev;
    
    if (len < sizeof(udp_header_t)) {
        return;
    }
    
    udp_header_t* udp = (udp_header_t*)data;
    uint16_t dest_port = __builtin_bswap16(udp->dest_port);
    uint16_t src_port = __builtin_bswap16(udp->src_port);
    // Enforce inbound policy
    if (!port_is_allowed(PM_PROTO_UDP, dest_port, PM_DIR_INBOUND)) {
        // Silently drop or log
        // gfx_print("UDP: inbound blocked on port "); extern void gfx_print_decimal(uint32_t); gfx_print_decimal(dest_port); gfx_print("\n");
        return;
    }
    
    const uint8_t* payload = data + sizeof(udp_header_t);
    uint32_t payload_len = len - sizeof(udp_header_t);
    
    // For now, just log reception
    gfx_print("UDP: Received packet on port ");
    extern void gfx_print_decimal(uint32_t);
    gfx_print_decimal(dest_port);
    gfx_print(" (");
    gfx_print_decimal(payload_len);
    gfx_print(" bytes)\n");

    // Minimal UDP echo server on port 9000
    if (dest_port == 9000) {
        // Echo payload back to sender from the same port
        extern int udp_send(net_device_t* dev, ipv4_addr_t* dest_ip, uint16_t src_port, uint16_t dest_port,
                            const uint8_t* data, uint32_t len);
        if (dev && src_ip && payload_len > 0) {
            (void)udp_send(dev, src_ip, 9000, src_port, payload, payload_len);
        }
    }
}

#include "tcp.h"
#include "ipv4.h"
#include "graphics/graphics.h"
#include "core/string.h"

void tcp_init(void) {
    gfx_print("TCP layer initialized\n");
}

void tcp_receive(net_device_t* dev, ipv4_addr_t* src_ip, const uint8_t* data, uint32_t len) {
    (void)dev;
    (void)src_ip;
    
    if (len < sizeof(tcp_header_t)) {
        return;
    }
    
    tcp_header_t* tcp = (tcp_header_t*)data;
    uint16_t dest_port = __builtin_bswap16(tcp->dest_port);
    uint8_t flags = tcp->flags;
    
    // For now, just log reception
    gfx_print("TCP: Received packet on port ");
    extern void gfx_print_decimal(uint32_t);
    gfx_print_decimal(dest_port);
    gfx_print(" flags=");
    extern void gfx_print_hex(uint32_t);
    gfx_print_hex(flags);
    
    if (flags & TCP_FLAG_SYN) {
        gfx_print(" [SYN]");
    }
    if (flags & TCP_FLAG_ACK) {
        gfx_print(" [ACK]");
    }
    if (flags & TCP_FLAG_FIN) {
        gfx_print(" [FIN]");
    }
    if (flags & TCP_FLAG_RST) {
        gfx_print(" [RST]");
    }
    gfx_print("\n");
}

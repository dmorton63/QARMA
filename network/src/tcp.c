#include "tcp.h"
#include "ipv4.h"
#include "graphics.h"
#include "port_manager.h"
#include "string.h"
#include "netlog.h"

void tcp_init(void) {
    gfx_print("TCP layer initialized\n");
}

static tcp_client_state_t g_tcp_client = {0};
tcp_last_syn_t g_tcp_last_syn = {0};

int tcp_connect(net_device_t* dev, ipv4_addr_t* dest_ip, uint16_t local_port, uint16_t remote_port) {
    if (!dev || !dest_ip) { netlog_write("tcp_connect: null dev/dest\n"); return -1; }
    if (dev->state != NET_DEV_RUNNING) { netlog_write("tcp_connect: device not RUNNING\n"); return -1; }
    if (!dev->send_packet) { netlog_write("tcp_connect: send_packet NULL\n"); return -1; }
    if (!port_is_allowed(PM_PROTO_TCP, local_port, PM_DIR_OUTBOUND)) {
        gfx_print("TCP: outbound blocked for port "); extern void gfx_print_decimal(uint32_t); gfx_print_decimal(local_port); gfx_print("\n");
        netlog_write("tcp_connect: outbound blocked\n");
        return -1;
    }
    // Ensure inbound is allowed on the chosen local (ephemeral) port so SYN-ACK isn't dropped
    port_allow(PM_PROTO_TCP, local_port, PM_DIR_INBOUND);

    // Build minimal TCP SYN packet
    uint8_t packet[64];
    tcp_header_t* tcp = (tcp_header_t*)packet;
    memset(packet, 0, sizeof(packet));
    tcp->src_port = tcp_htons(local_port);
    tcp->dest_port = tcp_htons(remote_port);
    tcp->seq_num = tcp_htonl(0x1000);
    tcp->ack_num = 0;
    tcp->data_offset_flags = (5 << 4); // data offset = 5 (20 bytes)
    tcp->flags = TCP_FLAG_SYN;
    tcp->window_size = tcp_htons(1024);
    tcp->checksum = 0; // skipping checksum for now
    tcp->urgent_ptr = 0;

    // Debug snapshot (host order)
    g_tcp_last_syn.src_port = local_port;
    g_tcp_last_syn.dest_port = remote_port;
    g_tcp_last_syn.seq_num = 0x1000;
    g_tcp_last_syn.flags = TCP_FLAG_SYN;
    g_tcp_last_syn.data_offset = 5;
    g_tcp_last_syn.window_size = 1024;
    netlog_write_hex("tcp_connect: SYN src=", (uint32_t)local_port); netlog_write_hex(" dest=", (uint32_t)remote_port); netlog_write("\n");
    int res = ipv4_send(dev, dest_ip, IP_PROTO_TCP, packet, sizeof(tcp_header_t));
    if (res >= 0) {
        g_tcp_client.connected = false;
        g_tcp_client.remote_ip = *dest_ip;
        g_tcp_client.local_port = local_port;
        g_tcp_client.remote_port = remote_port;
        g_tcp_client.seq_num = 0x1000;
        g_tcp_client.ack_num = 0;
        gfx_print("TCP: SYN sent\n");
        return 0;
    }
    netlog_write("tcp_connect: ipv4_send failed (ARP or send)\n");
    return -1;
}

int tcp_send(net_device_t* dev, ipv4_addr_t* dest_ip, uint16_t local_port, uint16_t remote_port,
             const uint8_t* data, uint32_t len) {
    if (!dev || !dest_ip || !data || len==0) return -1;
    if (!port_is_allowed(PM_PROTO_TCP, local_port, PM_DIR_OUTBOUND)) {
        gfx_print("TCP: outbound blocked for port "); extern void gfx_print_decimal(uint32_t); gfx_print_decimal(local_port); gfx_print("\n");
        return -1;
    }
    // Build minimal TCP segment with PSH|ACK (no checksum)
    static uint8_t packet[1600];
    tcp_header_t* tcp = (tcp_header_t*)packet;
    memset(packet, 0, sizeof(tcp_header_t));
    tcp->src_port = tcp_htons(local_port);
    tcp->dest_port = tcp_htons(remote_port);
    tcp->seq_num = tcp_htonl(g_tcp_client.seq_num);
    tcp->ack_num = tcp_htonl(g_tcp_client.ack_num);
    tcp->data_offset_flags = (5 << 4);
    tcp->flags = TCP_FLAG_PSH | TCP_FLAG_ACK;
    tcp->window_size = tcp_htons(1024);
    netlog_write_hex("tcp_send: seq=", g_tcp_client.seq_num); netlog_write_hex(" ack=", g_tcp_client.ack_num); netlog_write_hex(" len=", len); netlog_write("\n");
    tcp->checksum = 0;
    tcp->urgent_ptr = 0;
    memcpy(packet + sizeof(tcp_header_t), data, len);
    return ipv4_send(dev, dest_ip, IP_PROTO_TCP, packet, sizeof(tcp_header_t) + len);
}

void tcp_receive(net_device_t* dev, ipv4_addr_t* src_ip, const uint8_t* data, uint32_t len) {
    (void)dev;
    (void)src_ip;
    
    if (len < sizeof(tcp_header_t)) {
        return;
    }
    
    tcp_header_t* tcp = (tcp_header_t*)data;
    uint16_t dest_port = tcp_ntohs(tcp->dest_port);
    // Enforce inbound policy
    if (!port_is_allowed(PM_PROTO_TCP, dest_port, PM_DIR_INBOUND)) {
        return; // drop silently
    }
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

    // Minimal SYN-ACK recognition to mark connected
    if ((flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) == (TCP_FLAG_SYN | TCP_FLAG_ACK)) {
        // Expect on our local port
        if (dest_port == g_tcp_client.local_port) {
            g_tcp_client.connected = true;
            gfx_print("TCP: SYN-ACK received, connection established (minimal)\n");
        }
    }

        // If there's payload, print it to console (HTTP response)
        uint8_t header_words = (tcp->data_offset_flags >> 4) & 0x0F;
        uint32_t header_len = (uint32_t)header_words * 4;
        if (len > header_len) {
            const uint8_t* payload = data + header_len;
            uint32_t payload_len = len - header_len;
            gfx_print("TCP data: ");
            for (uint32_t i = 0; i < payload_len; ++i) {
                char c = (char)payload[i];
                if (c == '\r') continue; // skip CR for clarity
                if (c == '\n') { gfx_print("\n"); continue; }
                if (c >= 32 && c <= 126) {
                    char buf[2]; buf[0]=c; buf[1]='\0'; gfx_print(buf);
                } else {
                    // Non-printable: show as hex
                    gfx_print("<"); gfx_print_hex((uint32_t)(uint8_t)c); gfx_print(">");
                }
            }
            gfx_print("\n");
        }
}

#include "port_manager.h"
#include "graphics.h"
#include "vfs.h"
#include "netlog.h"
// Local CRC32 (same polynomial as AI persistence)
static uint32_t pm_crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

// Simple bitmap tables: 65536 ports * 2 directions per protocol
static uint8_t udp_in[65536/8];
static uint8_t udp_out[65536/8];
static uint8_t tcp_in[65536/8];
static uint8_t tcp_out[65536/8];

static inline void set_bit(uint8_t* bm, uint16_t port, bool val) {
    uint32_t idx = port >> 3; uint8_t mask = 1u << (port & 7);
    if (val) bm[idx] |= mask; else bm[idx] &= (uint8_t)~mask;
}
static inline bool get_bit(uint8_t* bm, uint16_t port) {
    uint32_t idx = port >> 3; uint8_t mask = 1u << (port & 7);
    return (bm[idx] & mask) != 0;
}

void port_manager_init(void) {
    // Default-deny inbound; allow all outbound by default
    for (int i=0;i<65536/8;i++) { udp_in[i]=0; tcp_in[i]=0; udp_out[i]=0xFF; tcp_out[i]=0xFF; }
    gfx_print("Port manager: inbound default DENY, outbound ALLOW\n");
}

static uint8_t* table_for(pm_protocol_t proto, pm_direction_t dir) {
    if (proto==PM_PROTO_UDP) return (dir==PM_DIR_INBOUND)? udp_in: udp_out;
    else return (dir==PM_DIR_INBOUND)? tcp_in: tcp_out;
}

bool port_allow(pm_protocol_t proto, uint16_t port, pm_direction_t dir) {
    set_bit(table_for(proto, dir), port, true);
    return true;
}

bool port_block(pm_protocol_t proto, uint16_t port, pm_direction_t dir) {
    set_bit(table_for(proto, dir), port, false);
    return true;
}

bool port_is_allowed(pm_protocol_t proto, uint16_t port, pm_direction_t dir) {
    return get_bit(table_for(proto, dir), port);
}

void port_list(pm_protocol_t proto, pm_direction_t dir) {
    const char* p = (proto==PM_PROTO_UDP)? "UDP": "TCP";
    const char* d = (dir==PM_DIR_INBOUND)? "IN": "OUT";
    gfx_print("Ports "); gfx_print(p); gfx_print(" "); gfx_print(d); gfx_print(": ");
    bool first=true;
    for (uint32_t port=0; port<65536; port++) {
        if (get_bit(table_for(proto, dir), (uint16_t)port)) {
            if (!first) { gfx_print(","); }
            first=false;
            extern void gfx_print_decimal(uint32_t);
            gfx_print_decimal(port);
        }
    }
    gfx_print("\n");
}

void port_dump_inbound_to_netlog(void) {
    // Summarize first few allowed inbound ports for UDP and TCP
    int count_udp = 0, count_tcp = 0;
    netlog_write("[port] inbound UDP: ");
    int printed = 0;
    for (uint32_t p = 0; p < 65536 && printed < 32; ++p) {
        if (get_bit(udp_in, (uint16_t)p)) {
            count_udp++; printed++;
            netlog_write_hex("", (uint32_t)p); netlog_write(" ");
        }
    }
    netlog_write(" count="); netlog_write_hex("", (uint32_t)count_udp); netlog_write("\n");
    netlog_write("[port] inbound TCP: ");
    printed = 0;
    for (uint32_t p = 0; p < 65536 && printed < 32; ++p) {
        if (get_bit(tcp_in, (uint16_t)p)) {
            count_tcp++; printed++;
            netlog_write_hex("", (uint32_t)p); netlog_write(" ");
        }
    }
    netlog_write(" count="); netlog_write_hex("", (uint32_t)count_tcp); netlog_write("\n");
}

// Simple binary format:
// magic 'PMV1' (4 bytes), then udp_in, udp_out, tcp_in, tcp_out bitmaps (each 8192 bytes)
int port_manager_save(void) {
    const char* host_path = "/host/port_rules.bin";
    const char* ram_path = "/ramdisk/port_rules.bin";
    vfs_node_t* node = vfs_create(host_path, VFS_TYPE_FILE);
    const char* used = host_path;
    if (!node) { node = vfs_create(ram_path, VFS_TYPE_FILE); used = ram_path; }
    if (!node) { gfx_print("port: failed to create rules file\n"); return -1; }
    uint32_t magic = 0x504D5631; // 'PMV1'
    int off = 0;
    vfs_write(node, &magic, sizeof(magic), off); off += sizeof(magic);
    vfs_write(node, udp_in, sizeof(udp_in), off); off += sizeof(udp_in);
    vfs_write(node, udp_out, sizeof(udp_out), off); off += sizeof(udp_out);
    vfs_write(node, tcp_in, sizeof(tcp_in), off); off += sizeof(tcp_in);
    vfs_write(node, tcp_out, sizeof(tcp_out), off); off += sizeof(tcp_out);
    // Append CRC32 over all bitmaps
    uint32_t crc = pm_crc32((const uint8_t*)udp_in, sizeof(udp_in)) ^
                   pm_crc32((const uint8_t*)udp_out, sizeof(udp_out)) ^
                   pm_crc32((const uint8_t*)tcp_in, sizeof(tcp_in)) ^
                   pm_crc32((const uint8_t*)tcp_out, sizeof(tcp_out));
    vfs_write(node, &crc, sizeof(crc), off); off += sizeof(crc);
    gfx_print("port: rules saved to "); gfx_print(used); gfx_print("\n");
    return 0;
}

int port_manager_load(void) {
    const char* host_path = "/host/port_rules.bin";
    const char* ram_path = "/ramdisk/port_rules.bin";
    vfs_node_t* node = vfs_open(host_path);
    if (!node) node = vfs_open(ram_path);
    if (!node) { gfx_print("port: no saved rules found\n"); return -1; }
    uint32_t magic=0; int off=0;
    int r = vfs_read(node, &magic, sizeof(magic), off); off += (r>0? r:0);
    if (r != sizeof(magic) || magic != 0x504D5631) { gfx_print("port: invalid rules file\n"); return -1; }
    if (vfs_read(node, udp_in, sizeof(udp_in), off) != sizeof(udp_in)) return -1;
    off += sizeof(udp_in);
    if (vfs_read(node, udp_out, sizeof(udp_out), off) != sizeof(udp_out)) return -1;
    off += sizeof(udp_out);
    if (vfs_read(node, tcp_in, sizeof(tcp_in), off) != sizeof(tcp_in)) return -1;
    off += sizeof(tcp_in);
    if (vfs_read(node, tcp_out, sizeof(tcp_out), off) != sizeof(tcp_out)) return -1;
    off += sizeof(tcp_out);
    // Verify CRC32
    uint32_t crc_file = 0;
    if (vfs_read(node, &crc_file, sizeof(crc_file), off) != sizeof(crc_file)) return -1;
    uint32_t crc_calc = pm_crc32((const uint8_t*)udp_in, sizeof(udp_in)) ^
                        pm_crc32((const uint8_t*)udp_out, sizeof(udp_out)) ^
                        pm_crc32((const uint8_t*)tcp_in, sizeof(tcp_in)) ^
                        pm_crc32((const uint8_t*)tcp_out, sizeof(tcp_out));
    if (crc_file != crc_calc) { gfx_print("port: rules CRC mismatch\n"); return -1; }
    gfx_print("port: rules loaded\n");
    return 0;
}

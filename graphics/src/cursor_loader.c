/**
 * QARMA - Cursor/Icon Asset Loader Implementation
 */

#include "cursor_loader.h"
#include "png_decoder.h"
#include "memory/heap.h"
#include "string.h"
#include "config.h"

static cursor_data_t g_cursors[CURSOR_MAX] = {0};

void cursor_loader_init(void) {
    SERIAL_LOG("[CURSOR] Initializing cursor loader\n");
    memset(g_cursors, 0, sizeof(g_cursors));
    
    // Create default software-rendered cursors
    cursor_create_defaults();
    
    // Try to load custom cursors from ISO
    SERIAL_LOG("[CURSOR] Attempting to load cursors from ISO /assets/cursors/\n");
    cursor_load_from_iso(CURSOR_ARROW, "/assets/cursors/arrow.png");
    cursor_load_from_iso(CURSOR_HAND, "/assets/cursors/hand.png");
    cursor_load_from_iso(CURSOR_IBEAM, "/assets/cursors/ibeam.png");
    
    SERIAL_LOG("[CURSOR] Cursor loader initialized\n");
}

bool cursor_load_from_iso(cursor_type_t type, const char* filename) {
    if (type >= CURSOR_MAX) return false;
    
    SERIAL_LOG("[CURSOR] Loading from ISO: ");
    SERIAL_LOG(filename);
    SERIAL_LOG("\n");
    
    // TODO: Implement ISO filesystem reading
    // For now, this is a placeholder
    // Once ISO9660 driver is working, read file and decode PNG
    
    return false;
}

bool cursor_load_from_host(cursor_type_t type, const char* filename) {
    if (type >= CURSOR_MAX) return false;
    
    SERIAL_LOG("[CURSOR] Loading from host: ");
    SERIAL_LOG(filename);
    SERIAL_LOG("\n");
    
    // Use VirtIO 9P to read from host
    extern bool virtio_9p_is_mounted(void);
    if (!virtio_9p_is_mounted()) {
        SERIAL_LOG("[CURSOR] Host filesystem not mounted\n");
        return false;
    }
    
    // TODO: Once 9P protocol is implemented:
    // 1. Open file via virtio_9p_open()
    // 2. Read entire file into buffer
    // 3. Decode PNG
    // 4. Store in g_cursors[type]
    
    return false;
}

const cursor_data_t* cursor_get(cursor_type_t type) {
    if (type >= CURSOR_MAX) return NULL;
    return &g_cursors[type];
}

// Create simple default cursors (software rendered)
void cursor_create_defaults(void) {
    SERIAL_LOG("[CURSOR] Creating default software-rendered cursors\n");
    
    // Default arrow cursor (16x16)
    uint32_t arrow_width = 16;
    uint32_t arrow_height = 16;
    
    g_cursors[CURSOR_ARROW].width = arrow_width;
    g_cursors[CURSOR_ARROW].height = arrow_height;
    g_cursors[CURSOR_ARROW].hotspot_x = 0;
    g_cursors[CURSOR_ARROW].hotspot_y = 0;
    g_cursors[CURSOR_ARROW].pixels = heap_alloc(arrow_width * arrow_height * 4);
    
    if (g_cursors[CURSOR_ARROW].pixels) {
        uint32_t* pixels = g_cursors[CURSOR_ARROW].pixels;
        
        // Simple arrow pattern
        // White with black outline
        uint32_t white = 0xFFFFFFFF;
        uint32_t black = 0xFF000000;
        uint32_t trans = 0x00000000;
        
        // Arrow shape (simplified)
        uint32_t arrow_pattern[16][16] = {
            {black,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans},
            {black,black,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans},
            {black,white,black,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans},
            {black,white,white,black,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans},
            {black,white,white,white,black,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans},
            {black,white,white,white,white,black,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans},
            {black,white,white,white,white,white,black,trans,trans,trans,trans,trans,trans,trans,trans,trans},
            {black,white,white,white,white,white,white,black,trans,trans,trans,trans,trans,trans,trans,trans},
            {black,white,white,white,white,white,white,white,black,trans,trans,trans,trans,trans,trans,trans},
            {black,white,white,white,white,black,black,black,black,black,trans,trans,trans,trans,trans,trans},
            {black,white,white,black,white,black,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans},
            {black,white,black,trans,black,white,black,trans,trans,trans,trans,trans,trans,trans,trans,trans},
            {black,black,trans,trans,black,white,black,trans,trans,trans,trans,trans,trans,trans,trans,trans},
            {trans,trans,trans,trans,trans,black,white,black,trans,trans,trans,trans,trans,trans,trans,trans},
            {trans,trans,trans,trans,trans,black,black,trans,trans,trans,trans,trans,trans,trans,trans,trans},
            {trans,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans,trans},
        };
        
        for (uint32_t y = 0; y < arrow_height; y++) {
            for (uint32_t x = 0; x < arrow_width; x++) {
                pixels[y * arrow_width + x] = arrow_pattern[y][x];
            }
        }
        
        SERIAL_LOG("[CURSOR] Default arrow cursor created\n");
    }
    
    // Hand cursor (16x16) - for clickable items
    g_cursors[CURSOR_HAND].width = 16;
    g_cursors[CURSOR_HAND].height = 16;
    g_cursors[CURSOR_HAND].hotspot_x = 5;
    g_cursors[CURSOR_HAND].hotspot_y = 0;
    g_cursors[CURSOR_HAND].pixels = heap_alloc(16 * 16 * 4);
    
    if (g_cursors[CURSOR_HAND].pixels) {
        // Fill with simple hand icon (placeholder)
        uint32_t* pixels = g_cursors[CURSOR_HAND].pixels;
        for (uint32_t i = 0; i < 16 * 16; i++) {
            pixels[i] = 0xFFFFCC99; // Skin color
        }
        SERIAL_LOG("[CURSOR] Default hand cursor created\n");
    }
    
    // I-beam cursor for text (16x16)
    g_cursors[CURSOR_IBEAM].width = 16;
    g_cursors[CURSOR_IBEAM].height = 16;
    g_cursors[CURSOR_IBEAM].hotspot_x = 8;
    g_cursors[CURSOR_IBEAM].hotspot_y = 8;
    g_cursors[CURSOR_IBEAM].pixels = heap_alloc(16 * 16 * 4);
    
    if (g_cursors[CURSOR_IBEAM].pixels) {
        uint32_t* pixels = g_cursors[CURSOR_IBEAM].pixels;
        uint32_t black = 0xFF000000;
        uint32_t trans = 0x00000000;
        
        // Simple I-beam shape
        for (uint32_t y = 0; y < 16; y++) {
            for (uint32_t x = 0; x < 16; x++) {
                if ((x == 7 || x == 8) && y >= 2 && y <= 13) {
                    pixels[y * 16 + x] = black;
                } else if ((y == 2 || y == 13) && x >= 5 && x <= 10) {
                    pixels[y * 16 + x] = black;
                } else {
                    pixels[y * 16 + x] = trans;
                }
            }
        }
        SERIAL_LOG("[CURSOR] Default I-beam cursor created\n");
    }
}

bool cursor_load(cursor_type_t type, const char* filename) {
    // Try ISO first, then host
    if (cursor_load_from_iso(type, filename)) {
        return true;
    }
    return cursor_load_from_host(type, filename);
}

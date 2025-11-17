#include "../stdtools.h"
#include "mouse.h"
#include "config.h"
#include "drivers/usb/usb_mouse.h"
#include "graphics/framebuffer.h"
#include "core/io.h"

// PS/2 Controller Commands
#define PS2_CMD_PORT    0x64
#define PS2_DATA_PORT   0x60

// PS/2 Controller Status bits
#define PS2_STATUS_OUTPUT_FULL  0x01
#define PS2_STATUS_INPUT_FULL   0x02

// PS/2 Controller Commands
#define PS2_CMD_READ_CONFIG     0x20
#define PS2_CMD_WRITE_CONFIG    0x60
#define PS2_CMD_DISABLE_MOUSE   0xA7
#define PS2_CMD_ENABLE_MOUSE    0xA8
#define PS2_CMD_TEST_MOUSE      0xA9
#define PS2_CMD_SEND_TO_MOUSE   0xD4

// Mouse Commands
#define MOUSE_CMD_RESET         0xFF
#define MOUSE_CMD_ENABLE_DATA   0xF4
#define MOUSE_CMD_SET_DEFAULTS  0xF6

static uint8_t packet[3];
static uint8_t packet_index = 0;
mouse_state_t mouse_state = {0};  // Make non-static so USB driver can access it

// External framebuffer dimensions
extern uint32_t fb_width;
extern uint32_t fb_height;

// PS/2 helper functions
static void ps2_wait_input(void) {
    while (inb(PS2_CMD_PORT) & PS2_STATUS_INPUT_FULL);
}

static void ps2_wait_output(void) {
    while (!(inb(PS2_CMD_PORT) & PS2_STATUS_OUTPUT_FULL));
}

static void ps2_write_command(uint8_t cmd) {
    ps2_wait_input();
    outb(PS2_CMD_PORT, cmd);
}

static void ps2_write_data(uint8_t data) {
    ps2_wait_input();
    outb(PS2_DATA_PORT, data);
}

static uint8_t ps2_read_data(void) {
    ps2_wait_output();
    return inb(PS2_DATA_PORT);
}

static void mouse_write(uint8_t cmd) {
    ps2_write_command(PS2_CMD_SEND_TO_MOUSE);
    ps2_write_data(cmd);
}

static uint8_t mouse_read(void) {
    return ps2_read_data();
}

void mouse_init(void) {
    SERIAL_LOG("Mouse: Initializing USB mouse...\n");
    
    // Initialize mouse position to center of screen
    mouse_state.x = fb_width / 2;
    mouse_state.y = fb_height / 2;
    mouse_state.dx = 0;
    mouse_state.dy = 0;
    mouse_state.left_pressed = false;
    mouse_state.right_pressed = false;
    mouse_state.middle_pressed = false;
    mouse_state.scroll_up = false;
    mouse_state.scroll_down = false;
    
    // Clear packet buffer
    packet_index = 0;
    
    // Initialize USB mouse - focus on fixing UHCI/TD issues
    extern int usb_mouse_init(void);
    int result = usb_mouse_init();
    if (result == 0) {
        SERIAL_LOG("Mouse: USB mouse initialized successfully\n");
    } else {
        SERIAL_LOG("Mouse: USB mouse initialization failed\n");
    }
}

void mouse_handler() {
    // Skip mouse interrupt handling since we're using USB mouse
    // Reading from port 0x60 here can interfere with keyboard
    
    // Check if there's actually mouse data pending
    uint8_t status = inb(PS2_CMD_PORT);
    if (status & 0x20) { // If there's mouse data (auxiliary device)
        uint8_t data = inb(PS2_DATA_PORT); // Read and discard to clear interrupt
        SERIAL_LOG("Mouse: Discarded PS/2 byte (USB mouse active)\n");
    }
    SERIAL_LOG("Mouse: USB mouse interrupt handled\n");
    // Send EOI to both PICs
    outb(0xA0, 0x20); // slave PIC
    outb(0x20, 0x20); // master PIC
}

// void parse_mouse_packet(uint8_t packet[3]) {
//     int x = (int8_t)packet[1]; // signed movement
//     int y = (int8_t)packet[2];
//     bool left = packet[0] & 0x01;
//     bool right = packet[0] & 0x02;
//     bool middle = packet[0] & 0x04;
//     //gfx_print("Mouse packet: ");
//     // Apply movement, update cursor, etc.
// }


mouse_state_t* get_mouse_state(void) {
    return &mouse_state;
}

void update_mouse_state_from_packet(uint8_t packet[3]) {
    // Basic packet validation
    SERIAL_LOG("Mouse packet received\n");
    if ((packet[0] & 0x08) == 0) {
        // Bit 3 should always be set in a valid PS/2 mouse packet
        return;
    }
    
    int dx = (int8_t)packet[1];
    int dy = (int8_t)packet[2];

    mouse_state.dx = dx;
    mouse_state.dy = dy;

    mouse_state.x += dx;
    mouse_state.y -= dy;  // Y is typically inverted

    // Clamp to screen bounds
    if (mouse_state.x < 0) mouse_state.x = 0;
    if (mouse_state.y < 0) mouse_state.y = 0;
    if (mouse_state.x >= (int)fb_width)  mouse_state.x = fb_width - 1;
    if (mouse_state.y >= (int)fb_height) mouse_state.y = fb_height - 1;

    // Update button states
    mouse_state.left_pressed   = packet[0] & 0x01;
    mouse_state.right_pressed  = packet[0] & 0x02;
    mouse_state.middle_pressed = packet[0] & 0x04;

    // Scroll wheel bits (if supported—extend as needed)
    mouse_state.scroll_up   = false;
    mouse_state.scroll_down = false;

    // Optional: dispatch events
    // dispatch_mouse_event(mouse_state.x, mouse_state.y, mouse_state.left_pressed, ...);

    // Optional: mark cursor region dirty for redraw
    // mark_cursor_dirty(mouse_state.x, mouse_state.y);
}

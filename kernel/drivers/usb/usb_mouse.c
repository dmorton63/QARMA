#include "usb_mouse.h"
#include "usb_hid.h"
#include "usb.h"
#include "usb_vendor_ids.h"
#include "core/input/mouse.h"
#include "core/memory/heap.h"
#include "core/sleep.h"
#include "core/log_timestamp.h"
#include "config.h"
#include "graphics/graphics.h"
#include "qarma_win_handle/qarma_input_events.h"

// Global USB mouse device
static usb_hid_device_t *g_usb_mouse = NULL;
static usb_mouse_report_t last_report = {0};

// Safety flag to disable mouse polling during critical operations
static volatile bool mouse_polling_enabled = true;

// External mouse state from mouse.c
extern mouse_state_t mouse_state;
extern uint32_t fb_width;
extern uint32_t fb_height;

// External event dispatch
extern void qarma_input_event_dispatch(QARMA_INPUT_EVENT* event);

// Track previous button state for generating DOWN/UP events
static uint8_t prev_buttons = 0;

// Tablet mode flag - auto-detected based on report data
static bool is_tablet_mode = false;
static bool mode_detected = false;

// Debug: Track report count
static uint32_t report_count = 0;
static uint32_t last_logged_report = 0;
#define MOUSE_LOG_EVERY_N_REPORTS 10

int usb_mouse_init(void) {
    GFX_LOG_MIN("USB Mouse: Starting USB stack init...\n");
    SERIAL_LOG("USB Mouse: Starting USB stack init...\n");
    
    if (usb_init() != 0) {
        GFX_LOG_MIN("USB Mouse: USB stack init failed\n");
        return USB_MOUSE_ERR_USB;
    }
    
    GFX_LOG_MIN("USB Mouse: USB stack OK, initializing HID...\n");
    
    if (usb_hid_init() != 0) {
        GFX_LOG_MIN("USB Mouse: HID init failed\n");
        return USB_MOUSE_ERR_HID;
    }
    
    GFX_LOG_MIN("USB Mouse: HID OK, enumerating devices (this may take time)...\n");
    
    if (usb_enumerate_devices() != 0) {
        GFX_LOG_MIN("USB Mouse: Device enumeration failed\n");
        return USB_MOUSE_ERR_ENUM;
    }
    
    GFX_LOG_MIN("USB Mouse: Enumeration complete, searching for mouse...\n");
    
    // The enumeration process already called usb_mouse_probe() for each device
    // Just check if we successfully attached a mouse
    if (!g_usb_mouse) {
        GFX_LOG_MIN("USB Mouse: No HID mouse found\n");
        return USB_MOUSE_ERR_ENUM;
    }

    GFX_LOG_MIN("USB Mouse: Init complete\n");
    return USB_MOUSE_OK;
}


// usb_interface_descriptor_t* usb_find_hid_mouse_interface(usb_device_t* device) {
//     uint8_t* desc = device->config_desc;
//     size_t len = device->config_desc->bLength;

//     for (size_t i = 0; i < len;) {
//         uint8_t bLength = desc[i];
//         uint8_t bType = desc[i + 1];

//         if (bType == 0x04 && bLength >= sizeof(usb_interface_descriptor_t)) {
//             usb_interface_descriptor_t* iface = (usb_interface_descriptor_t*)&desc[i];
//             if (iface->bInterfaceClass == 0x03 && iface->bInterfaceProtocol == 0x02) {
//                 return iface;
//             }
//         }

//         i += bLength;
//     }

//     return NULL;
// }
int usb_mouse_probe(usb_device_t *device) {
    SERIAL_LOG("USB Mouse: Probing device for mouse interface\n");
    
    if (!device || !device->config_desc) {
        return USB_MOUSE_ERR_USB;
    }
    
    // Parse configuration descriptor to find HID mouse interface
    uint8_t *desc_data = (uint8_t *)device->config_desc;
    uint16_t total_length = device->config_desc->wTotalLength;
    uint16_t offset = sizeof(usb_config_descriptor_t);
    
    while (offset < total_length) {
        uint8_t length = desc_data[offset];
        uint8_t type = desc_data[offset + 1];
        
        if (type == USB_DESC_INTERFACE) {
            usb_interface_descriptor_t *iface = (usb_interface_descriptor_t *)(desc_data + offset);
            
            // Check for HID class with boot protocol mouse
            if (iface->bInterfaceClass == USB_CLASS_HID &&
                iface->bInterfaceSubClass == USB_HID_SUBCLASS_BOOT &&
                iface->bInterfaceProtocol == USB_HID_PROTOCOL_MOUSE) {
                
                SERIAL_LOG("USB Mouse: Found HID mouse interface\n");
                return usb_mouse_attach_interface(device, iface);
            }
        }
        
        offset += length;
        if (length == 0) break; // Prevent infinite loop
    }
    
    return -1; // No mouse interface found
}

int usb_mouse_attach_interface(usb_device_t *device, usb_interface_descriptor_t *interface) {
    SERIAL_LOG("USB Mouse: Attaching mouse interface\n");
    
    // Detect Razer Mamba and other gaming mice
    if (device && device->device_desc.idVendor && device->device_desc.idProduct) {
        uint16_t vendor = device->device_desc.idVendor;
        uint16_t product = device->device_desc.idProduct;
        
        SERIAL_LOG("USB Mouse: Vendor ID: 0x");
        SERIAL_LOG_HEX("", vendor);
        SERIAL_LOG(" Product ID: 0x");
        SERIAL_LOG_HEX("", product);
        SERIAL_LOG("\n");
        
        if (is_razer_mamba(vendor, product)) {
            SERIAL_LOG("USB Mouse: *** RAZER MAMBA DETECTED ***\n");
            SERIAL_LOG("USB Mouse: Using 600-800us timing\n");
        } else if (is_razer_mouse(vendor, product)) {
            SERIAL_LOG("USB Mouse: Razer gaming mouse detected\n");
        }
    }
    
    // Allocate HID device structure
    g_usb_mouse = (usb_hid_device_t *)heap_alloc(sizeof(usb_hid_device_t));
    if (!g_usb_mouse) {
        SERIAL_LOG("USB Mouse: Failed to allocate HID device structure\n");
        return -1;
    }
    
    // Initialize HID device structure
    g_usb_mouse->device = device;
    g_usb_mouse->interface_num = interface->bInterfaceNumber;
    g_usb_mouse->protocol = interface->bInterfaceProtocol;
    g_usb_mouse->is_mouse = true;
    g_usb_mouse->is_keyboard = false;
    
    // Find interrupt IN endpoint for mouse reports
    if (usb_mouse_find_endpoints(interface) != 0) {
        heap_free(g_usb_mouse);
        g_usb_mouse = NULL;
        return -1;
    }
    
    // Set boot protocol (simpler than report protocol)
    if (usb_hid_set_protocol(g_usb_mouse, 0) != 0) { // 0 = boot protocol
        SERIAL_LOG("USB Mouse: Warning - failed to set boot protocol\n");
    }
    
    // Set idle rate (0 = infinite, only send on change)
    if (usb_hid_set_idle(g_usb_mouse, 0, 0) != 0) {
        SERIAL_LOG("USB Mouse: Warning - failed to set idle rate\n");
    }
    
    // Start receiving mouse reports
    usb_mouse_start_polling();
    
    SERIAL_LOG("USB Mouse: Successfully attached mouse device\n");
    return 0;
}

int usb_mouse_find_endpoints(usb_interface_descriptor_t *interface) {
    // Parse endpoint descriptors following the provided interface descriptor
    // Use the device's configuration descriptor buffer to find matching endpoints.
    usb_device_t *dev = g_usb_mouse->device;
    if (!dev || !dev->config_desc) {
        SERIAL_LOG("USB Mouse: No device config descriptor available, using defaults\n");
        g_usb_mouse->endpoint_in = 0x81;
        g_usb_mouse->endpoint_out = 0x00;
        g_usb_mouse->max_packet_size = 8;
        g_usb_mouse->interval = 10;
        return 0;
    }

    uint8_t *cfg = (uint8_t *)dev->config_desc;
    uint16_t total = dev->config_desc->wTotalLength;

    // Compute offset of the interface descriptor within the config buffer
    uintptr_t base = (uintptr_t)cfg;
    uintptr_t ifptr = (uintptr_t)interface;
    if (ifptr < base || ifptr >= base + total) {
        // Fallback to defaults
        g_usb_mouse->endpoint_in = 0x81;
        g_usb_mouse->endpoint_out = 0x00;
        g_usb_mouse->max_packet_size = 8;
        g_usb_mouse->interval = 10;
        SERIAL_LOG("USB Mouse: Interface pointer outside config buffer, using defaults\n");
        return 0;
    }

    uint16_t offset = (uint16_t)(ifptr - base) + sizeof(usb_interface_descriptor_t);
    g_usb_mouse->endpoint_in = 0x00;
    g_usb_mouse->endpoint_out = 0x00;
    g_usb_mouse->max_packet_size = 0;
    g_usb_mouse->interval = 0;

    while (offset + 2 <= total) {
        uint8_t bLength = cfg[offset];
        uint8_t bType = cfg[offset + 1];
        if (bLength == 0) break;

        if (bType == USB_DESC_ENDPOINT) {
            usb_endpoint_descriptor_t *ep = (usb_endpoint_descriptor_t *)&cfg[offset];
            uint8_t ep_dir = ep->bEndpointAddress & 0x80;
            uint8_t ep_attr = ep->bmAttributes & 0x3;
            if (ep_attr == USB_TRANSFER_INTERRUPT && ep_dir == 0x80) {
                // Found interrupt IN endpoint
                g_usb_mouse->endpoint_in = ep->bEndpointAddress;
                g_usb_mouse->max_packet_size = ep->wMaxPacketSize;
                g_usb_mouse->interval = ep->bInterval;
                break;
            }
        } else if (bType == USB_DESC_INTERFACE) {
            // Reached next interface; stop searching
            break;
        }

        offset += bLength;
    }

    if (g_usb_mouse->endpoint_in == 0x00) {
        // Fallback defaults
        g_usb_mouse->endpoint_in = 0x81;
        g_usb_mouse->max_packet_size = 8;
        g_usb_mouse->interval = 10;
    }

    SERIAL_LOG_HEX("USB Mouse: device=", (uint32_t)dev);
    SERIAL_LOG_HEX(" USB Mouse: Configured IN=", g_usb_mouse->endpoint_in);
    SERIAL_LOG_HEX(" maxpkt=", g_usb_mouse->max_packet_size);
    SERIAL_LOG_HEX(" interval=", g_usb_mouse->interval);
    SERIAL_LOG("\n");
    return 0;
}

void usb_mouse_start_polling(void) {
    static bool polling_started = false;
    
    if (!g_usb_mouse || polling_started) {
        return;
    }
    
    SERIAL_LOG("USB Mouse: Starting mouse report polling\n");
    polling_started = true;
    
    // Allocate DMA-capable buffer for mouse reports from heap (identity-mapped)
    // Allocate 8 bytes to handle both mouse (4 bytes) and tablet (6 bytes) reports
    usb_mouse_report_t *report_buffer = (usb_mouse_report_t *)heap_alloc(8);
    if (!report_buffer) {
        SERIAL_LOG("USB Mouse: Failed to allocate DMA buffer\n");
        return;
    }
    
    // Start interrupt transfer for mouse reports
    SERIAL_LOG_HEX("USB Mouse: report_buffer virt=", (uint32_t)report_buffer);
    SERIAL_LOG_HEX(" USB Mouse: report_size=", sizeof(usb_mouse_report_t));
    SERIAL_LOG("\n");

    usb_interrupt_transfer(
        g_usb_mouse->device,
        g_usb_mouse->endpoint_in,
        report_buffer,
        sizeof(usb_mouse_report_t),
        usb_mouse_report_callback
    );
}

void usb_mouse_report_callback(usb_transfer_t *transfer) {
    if (!transfer || transfer->status != 0) {
        SERIAL_LOG("USB Mouse: Transfer failed or incomplete\n");
        return; // Don't restart on error for mock implementation
    }
    
    if (transfer->actual_length < sizeof(usb_mouse_report_t)) {
        SERIAL_LOG("USB Mouse: Incomplete mouse report received\n");
        return; // Don't restart for mock implementation
    }
    
    // Process the mouse report
    usb_mouse_report_t *report = (usb_mouse_report_t *)transfer->buffer;
    usb_mouse_process_report(report);
    
    // Resubmit polling transfer to continue receiving reports
    SERIAL_LOG("USB Mouse: Resubmitting interrupt transfer for next report\n");
    usb_interrupt_transfer(
        g_usb_mouse->device,
        g_usb_mouse->endpoint_in,
        transfer->buffer,
        sizeof(usb_mouse_report_t),
        usb_mouse_report_callback
    );

    // In real implementation, would continue polling automatically
    // (we resubmit here to maintain continuous polling)
}

void usb_mouse_process_report(usb_mouse_report_t *report) {
    extern void serial_debug(const char* msg);
    
    if (!report) {
        SERIAL_LOG("[USB_MOUSE] ERROR: NULL report!\n");
        return;
    }
    
    report_count++;
    
    // Auto-detect tablet vs mouse mode on first report with movement
    uint8_t *raw_bytes = (uint8_t*)report;
    if (!mode_detected) {
        // Tablet reports have 16-bit X/Y values (typically 6 bytes total)
        // Mouse reports have 8-bit signed X/Y values (typically 4 bytes)
        // Check if bytes 2-5 look like 16-bit coordinates (little-endian)
        uint16_t potential_x = raw_bytes[1] | (raw_bytes[2] << 8);
        uint16_t potential_y = raw_bytes[3] | (raw_bytes[4] << 8);
        
        // Tablet coordinates are typically 0-32767 range
        // If we see values > 255, it's likely tablet mode
        if (potential_x > 255 || potential_y > 255) {
            is_tablet_mode = true;
            SERIAL_LOG("[USB_MOUSE] Detected TABLET mode (absolute coordinates)\n");
        } else {
            is_tablet_mode = false;
            SERIAL_LOG("[USB_MOUSE] Detected MOUSE mode (relative coordinates)\n");
        }
        mode_detected = true;
    }
    
    // Log every Nth report with full raw data for debugging
    if (report_count - last_logged_report >= MOUSE_LOG_EVERY_N_REPORTS) {
        SERIAL_LOG("[USB_MOUSE] Report #");
        SERIAL_LOG_DEC("", report_count);
        SERIAL_LOG(is_tablet_mode ? " [TABLET]" : " [MOUSE]");
        SERIAL_LOG(": buttons=0x");
        SERIAL_LOG_HEX("", report->buttons);
        SERIAL_LOG(" raw=[");
        for (int i = 0; i < 6; i++) {
            if (raw_bytes[i] < 16) SERIAL_LOG("0");
            SERIAL_LOG_HEX("", raw_bytes[i]);
            if (i < 5) SERIAL_LOG(" ");
        }
        SERIAL_LOG("]\n");
        last_logged_report = report_count;
    }
    
    // Process based on detected mode
    if (is_tablet_mode) {
        // TABLET MODE: Absolute coordinates
        // Format: [buttons, x_low, x_high, y_low, y_high, wheel]
        // Parse manually from raw bytes to avoid struct alignment issues
        uint16_t tablet_x = raw_bytes[1] | (raw_bytes[2] << 8);
        uint16_t tablet_y = raw_bytes[3] | (raw_bytes[4] << 8);
        
        // BUGFIX: Ignore spurious (0,0) reports from tablet
        // QEMU USB tablet sometimes sends (0,0) between valid reports
        // Only update position if we have valid non-zero coordinates
        if (tablet_x == 0 && tablet_y == 0) {
            // Skip this report - keep current position
            return;
        }
        
        // USB tablet coordinates are typically 0-32767
        // Scale to framebuffer dimensions
        uint32_t max_x = (fb_width > 0) ? fb_width : 1024;
        uint32_t max_y = (fb_height > 0) ? fb_height : 768;
        
        // Scale from tablet space (0-32767) to screen space
        int32_t new_x = ((uint32_t)tablet_x * max_x) / 32768;
        int32_t new_y = ((uint32_t)tablet_y * max_y) / 32768;
        
        // Debug log tablet coordinates
        static int tablet_log = 0;
        if (++tablet_log <= 20) {
            SERIAL_LOG("[TABLET] Raw: x=");
            SERIAL_LOG_DEC("", tablet_x);
            SERIAL_LOG(" y=");
            SERIAL_LOG_DEC("", tablet_y);
            SERIAL_LOG(" -> Screen: x=");
            SERIAL_LOG_DEC("", new_x);
            SERIAL_LOG(" y=");
            SERIAL_LOG_DEC("", new_y);
            SERIAL_LOG(" (fb=");
            SERIAL_LOG_DEC("", max_x);
            SERIAL_LOG("x");
            SERIAL_LOG_DEC("", max_y);
            SERIAL_LOG(")\n");
        }
        
        // Calculate delta for event dispatch
        mouse_state.dx = new_x - mouse_state.x;
        mouse_state.dy = new_y - mouse_state.y;
        
        // Update absolute position
        mouse_state.x = new_x;
        mouse_state.y = new_y;
    } else {
        // MOUSE MODE: Relative coordinates
        int8_t report_x = (int8_t)raw_bytes[1];
        int8_t report_y = (int8_t)raw_bytes[2];
        
        // Apply sensitivity multiplier for better tracking (2.5x default)
        mouse_state.dx = (report_x * 5) / 2;
        mouse_state.dy = (report_y * 5) / 2;
        
        // Only apply relative movement updates for mouse mode
        // (Tablet mode already set absolute position above)
        bool has_movement = (mouse_state.dx != 0 || mouse_state.dy != 0);
        
        if (has_movement) {
            // Clamp delta to prevent mouse flying off screen
            if (mouse_state.dx > 50) mouse_state.dx = 50;
            if (mouse_state.dx < -50) mouse_state.dx = -50;
            if (mouse_state.dy > 50) mouse_state.dy = 50;
            if (mouse_state.dy < -50) mouse_state.dy = -50;
            
            // Scale mouse movement to 60% for better control
            int32_t scaled_dx = (mouse_state.dx * 6) / 10;
            int32_t scaled_dy = (mouse_state.dy * 6) / 10;
            
            mouse_state.x += scaled_dx;
            mouse_state.y += scaled_dy;
        }
    }
    
    // Clamp position to screen boundaries (both modes)
    bool has_movement = (mouse_state.dx != 0 || mouse_state.dy != 0);
    if (has_movement || is_tablet_mode) {
        uint32_t max_x = (fb_width > 0) ? fb_width : 1024;
        uint32_t max_y = (fb_height > 0) ? fb_height : 768;
        
        static uint32_t boundary_log_count = 0;
        bool hit_boundary = false;
        
        if (mouse_state.x < 0) {
            if (boundary_log_count < 10) {
                SERIAL_LOG("[MOUSE] Clamping X from ");
                SERIAL_LOG_DEC("", mouse_state.x);
                SERIAL_LOG(" to 0\n");
            }
            mouse_state.x = 0;
            hit_boundary = true;
        }
        if (mouse_state.y < 0) {
            if (boundary_log_count < 10) {
                SERIAL_LOG("[MOUSE] Clamping Y from ");
                SERIAL_LOG_DEC("", mouse_state.y);
                SERIAL_LOG(" to 0\n");
            }
            mouse_state.y = 0;
            hit_boundary = true;
        }
        if (mouse_state.x >= (int32_t)max_x) {
            if (boundary_log_count < 10) {
                SERIAL_LOG("[MOUSE] Clamping X from ");
                SERIAL_LOG_DEC("", mouse_state.x);
                SERIAL_LOG(" to ");
                SERIAL_LOG_DEC("", max_x - 1);
                SERIAL_LOG("\n");
            }
            mouse_state.x = max_x - 1;
            hit_boundary = true;
        }
        if (mouse_state.y >= (int32_t)max_y) {
            if (boundary_log_count < 10) {
                SERIAL_LOG("[MOUSE] Clamping Y from ");
                SERIAL_LOG_DEC("", mouse_state.y);
                SERIAL_LOG(" to ");
                SERIAL_LOG_DEC("", max_y - 1);
                SERIAL_LOG("\n");
            }
            mouse_state.y = max_y - 1;
            hit_boundary = true;
        }
        
        if (hit_boundary) {
            boundary_log_count++;
            if (boundary_log_count <= 5) {
                SERIAL_LOG("[MOUSE] Final position: x=");
                SERIAL_LOG_DEC("", mouse_state.x);
                SERIAL_LOG(" y=");
                SERIAL_LOG_DEC("", mouse_state.y);
                SERIAL_LOG(" (fb=");
                SERIAL_LOG_DEC("", fb_width);
                SERIAL_LOG("x");
                SERIAL_LOG_DEC("", fb_height);
                SERIAL_LOG(")\n");
            }
        }
        
        // Throttle mouse move event dispatching to prevent overwhelming the system
        // Only dispatch every 3rd move event to preserve keyboard responsiveness
        static uint32_t move_event_throttle = 0;
        if (++move_event_throttle % 3 == 0) {
            QARMA_INPUT_EVENT move_event = qarma_input_event_create_mouse_move(
                mouse_state.x, mouse_state.y,
                mouse_state.dx, mouse_state.dy,
                NULL  // Target will be determined by hit testing
            );
            move_event.data.mouse.buttons = report->buttons;
            qarma_input_event_dispatch(&move_event);
        }
    }
    
    // Update button states
    mouse_state.left_pressed = (report->buttons & 0x01) != 0;
    mouse_state.right_pressed = (report->buttons & 0x02) != 0;
    mouse_state.middle_pressed = (report->buttons & 0x04) != 0;
    
    // Detect button state changes and generate DOWN/UP events
    uint8_t button_changes = prev_buttons ^ report->buttons;
    
    if (button_changes & 0x01) {  // Left button changed
        QARMA_INPUT_EVENT_TYPE type = (report->buttons & 0x01) ? 
            QARMA_INPUT_EVENT_MOUSE_DOWN : QARMA_INPUT_EVENT_MOUSE_UP;
        QARMA_INPUT_EVENT btn_event = qarma_input_event_create_mouse_button(
            type, mouse_state.x, mouse_state.y, QARMA_MOUSE_BUTTON_LEFT, NULL
        );
        btn_event.data.mouse.buttons = report->buttons;
        qarma_input_event_dispatch(&btn_event);
    }
    
    if (button_changes & 0x02) {  // Right button changed
        QARMA_INPUT_EVENT_TYPE type = (report->buttons & 0x02) ? 
            QARMA_INPUT_EVENT_MOUSE_DOWN : QARMA_INPUT_EVENT_MOUSE_UP;
        QARMA_INPUT_EVENT btn_event = qarma_input_event_create_mouse_button(
            type, mouse_state.x, mouse_state.y, QARMA_MOUSE_BUTTON_RIGHT, NULL
        );
        btn_event.data.mouse.buttons = report->buttons;
        qarma_input_event_dispatch(&btn_event);
    }
    
    if (button_changes & 0x04) {  // Middle button changed
        QARMA_INPUT_EVENT_TYPE type = (report->buttons & 0x04) ? 
            QARMA_INPUT_EVENT_MOUSE_DOWN : QARMA_INPUT_EVENT_MOUSE_UP;
        QARMA_INPUT_EVENT btn_event = qarma_input_event_create_mouse_button(
            type, mouse_state.x, mouse_state.y, QARMA_MOUSE_BUTTON_MIDDLE, NULL
        );
        btn_event.data.mouse.buttons = report->buttons;
        qarma_input_event_dispatch(&btn_event);
    }
    
    prev_buttons = report->buttons;
    
    // Handle scroll wheel
    if (report->wheel != 0) {
        mouse_state.scroll_up = (report->wheel > 0);
        mouse_state.scroll_down = (report->wheel < 0);
        
        QARMA_INPUT_EVENT scroll_event = qarma_input_event_create_mouse_scroll(
            mouse_state.x, mouse_state.y, (int32_t)((int8_t)report->wheel), NULL
        );
        scroll_event.data.mouse.buttons = report->buttons;
        qarma_input_event_dispatch(&scroll_event);
    } else {
        mouse_state.scroll_up = false;
        mouse_state.scroll_down = false;
    }
    
    // Optional: Log significant changes for debugging
    static int debug_count = 0;
    if (report->x != 0 || report->y != 0) {
        if (debug_count < 10) {
            SERIAL_LOG("USB Mouse: dx=");
            SERIAL_LOG_HEX("", (uint8_t)report->x);
            SERIAL_LOG(" dy=");
            SERIAL_LOG_HEX("", (uint8_t)report->y);
            SERIAL_LOG(" pos=(");
            SERIAL_LOG_DEC("", mouse_state.x);
            SERIAL_LOG(",");
            SERIAL_LOG_DEC("", mouse_state.y);
            SERIAL_LOG(")\n");
            debug_count++;
        }
    }
    
    if (report->buttons != last_report.buttons) {
        SERIAL_LOG("USB Mouse: Buttons - ");
        if (mouse_state.left_pressed) SERIAL_LOG("L");
        if (mouse_state.right_pressed) SERIAL_LOG("R");
        if (mouse_state.middle_pressed) SERIAL_LOG("M");
        SERIAL_LOG("\n");
    }
    
    last_report = *report;
    
}

void usb_mouse_detach(void) {
    if (g_usb_mouse) {
        SERIAL_LOG("USB Mouse: Detaching mouse device\n");
        
        // Cancel any ongoing transfers
        // (This would require USB stack support for transfer cancellation)
        
        // Free HID device structure
        heap_free(g_usb_mouse);
        g_usb_mouse = NULL;
        
        // Reset mouse state to default
        mouse_state.x = fb_width / 2;
        mouse_state.y = fb_height / 2;
        mouse_state.dx = 0;
        mouse_state.dy = 0;
        mouse_state.left_pressed = false;
        mouse_state.right_pressed = false;
        mouse_state.middle_pressed = false;
        mouse_state.scroll_up = false;
        mouse_state.scroll_down = false;
        
        SERIAL_LOG("USB Mouse: Mouse detached and state reset\n");
    }
}

bool usb_mouse_is_connected(void) {
    return g_usb_mouse != NULL;
}

// XHCI mouse initialization
typedef struct {
    void* controller;
    uint8_t slot;
    uint8_t padding[59];  // Pad to 64 bytes so report_buffer is aligned
    usb_mouse_report_t report_buffer __attribute__((aligned(64)));
} xhci_mouse_t;

static xhci_mouse_t *g_xhci_mouse = NULL;
static bool initialization_complete = false;

void usb_mouse_init_xhci(void* controller, uint8_t slot) {
    SERIAL_LOG("USB Mouse: Initializing XHCI mouse\n");
    
    // Allocate XHCI mouse structure with DMA-safe aligned buffer
    extern void* dma_allocator_alloc(size_t size, size_t alignment, size_t boundary);
    g_xhci_mouse = (xhci_mouse_t*)dma_allocator_alloc(sizeof(xhci_mouse_t), 64, 0);
    if (!g_xhci_mouse) {
        SERIAL_LOG("USB Mouse: Failed to allocate XHCI mouse structure\n");
        return;
    }
    
    g_xhci_mouse->controller = controller;
    g_xhci_mouse->slot = slot;
    memset(&g_xhci_mouse->report_buffer, 0, 8);  // Clear 8 bytes for tablet reports
    
    SERIAL_LOG("USB Mouse: XHCI mouse initialized on slot ");
    SERIAL_LOG_HEX("", slot);
    SERIAL_LOG("\n");
    
    // Configure endpoint before queuing transfers
    SERIAL_LOG("[USB_MOUSE] Configuring endpoint...\n");
    extern int xhci_configure_endpoint(void *xhci, uint8_t slot);
    if (xhci_configure_endpoint(controller, slot) != 0) {
        SERIAL_LOG("[USB_MOUSE] ERROR: Failed to configure endpoint\n");
        return;
    }
    SERIAL_LOG("[USB_MOUSE] Endpoint configured successfully\n");
    
    // Queue multiple initial transfers to keep the pipeline full
    // All use the same buffer since we process events synchronously
    extern int xhci_queue_transfer(void *xhci, uint8_t slot, uint8_t endpoint, void *buffer, uint16_t length);
    // DON'T queue transfers yet - wait for init to complete
    // for (int i = 0; i < 128; i++) {
    //     xhci_queue_transfer(controller, slot, 1, &g_xhci_mouse->report_buffer, sizeof(usb_mouse_report_t));
    // }
    SERIAL_LOG("[USB_MOUSE] Init complete, buffer at ");
    SERIAL_LOG_HEX("", (uint32_t)&g_xhci_mouse->report_buffer);
    SERIAL_LOG("\n");
    initialization_complete = true;
}

void usb_mouse_poll_xhci(void) {
    static uint32_t poll_count = 0;
    static uint32_t last_report_count = 0;
    
    if (!mouse_polling_enabled) {
        static bool logged_disabled = false;
        if (!logged_disabled) {
            SERIAL_LOG("[USB_MOUSE] Polling disabled\n");
            logged_disabled = true;
        }
        return;
    }
    if (!g_xhci_mouse) {
        static bool logged_no_mouse = false;
        if (!logged_no_mouse) {
            SERIAL_LOG("[USB_MOUSE] No XHCI mouse device\n");
            logged_no_mouse = true;
        }
        return;
    }
    
    poll_count++;
    
    // Every 100 polls, check if we're still getting reports
    if (poll_count % 100 == 0) {
        if (report_count == last_report_count) {
            LOG_TS("[USB_MOUSE] WARNING: No new reports after 100 polls! Stuck at ");
            SERIAL_LOG_DEC("", report_count);
            SERIAL_LOG("\n");
        }
        last_report_count = report_count;
    }
    
    // Called periodically from main loop (already has 16ms delay)
    extern void xhci_poll_events(void *xhci);
    xhci_poll_events(g_xhci_mouse->controller);
    
    // No additional sleep needed - main loop already controls polling rate
    
    // Queue initial transfers on first poll after init complete
    // Use only 2-3 transfers to avoid flooding the system
    static bool transfers_queued = false;
    if (initialization_complete && !transfers_queued) {
        extern int xhci_queue_transfer(void *xhci, uint8_t slot, uint8_t endpoint, void *buffer, uint16_t length);
        for (int i = 0; i < 2; i++) {
            xhci_queue_transfer(g_xhci_mouse->controller, g_xhci_mouse->slot, 1, 
                              &g_xhci_mouse->report_buffer, 8);  // Request 8 bytes for tablet
        }
        SERIAL_LOG("[USB_MOUSE] Queued 2 initial transfers after init\n");
        SERIAL_LOG("[USB_MOUSE] Buffer address: 0x");
        SERIAL_LOG_HEX("", (uint32_t)&g_xhci_mouse->report_buffer);
        SERIAL_LOG("\n");
        transfers_queued = true;
    }
}

void usb_mouse_set_polling_enabled(bool enabled) {
    mouse_polling_enabled = enabled;
    SERIAL_LOG(enabled ? "[USB_MOUSE] Polling enabled\n" : "[USB_MOUSE] Polling disabled\n");
}

uint32_t usb_mouse_get_report_count(void) {
    return report_count;
}

void usb_mouse_process_xhci_data(uint8_t slot) {
    if (!g_xhci_mouse || g_xhci_mouse->slot != slot) {
        SERIAL_LOG("[USB_MOUSE] process_xhci_data: slot mismatch or no mouse\n");
        return;
    }
    
    // Don't requeue during initialization to avoid infinite loop in xhci_submit_command
    if (!initialization_complete) {
        return;
    }
    
    static uint32_t process_count = 0;
    process_count++;
    
    // Log every 50th call
    if (process_count % 50 == 0) {
        SERIAL_LOG("[USB_MOUSE] process_xhci_data called #");
        SERIAL_LOG_DEC("", process_count);
        SERIAL_LOG("\n");
    }
    
    // Process the report that was just received
    usb_mouse_process_report(&g_xhci_mouse->report_buffer);
    
    // Clear the buffer after processing to prevent reading stale data
    memset(&g_xhci_mouse->report_buffer, 0, 8);  // Clear 8 bytes for tablet reports
    
    // Razer Mamba specific: Add microsecond delay after processing report
    // This prevents overwhelming the device and ensures stable communication
    sleep_us(RAZER_MAMBA_WAIT_MIN_US);
    
    // Before queueing, verify buffer contents are cleared
    static int clear_check = 0;
    if (++clear_check <= 5) {
        extern void serial_debug(const char* msg);
        serial_debug("[MOUSE_CLEAR] Before queue, buffer bytes: ");
        uint8_t *buf = (uint8_t*)&g_xhci_mouse->report_buffer;
        for (int i = 0; i < 4; i++) {
            if (buf[i] < 16) serial_debug("0");
            SERIAL_LOG_HEX("", buf[i]);
            serial_debug(" ");
        }
        serial_debug("\n");
    }
    
    // Queue next transfer
    extern int xhci_queue_transfer(void *xhci, uint8_t slot, uint8_t endpoint, void *buffer, uint16_t length);
    int result = xhci_queue_transfer(g_xhci_mouse->controller, slot, 1,
                       &g_xhci_mouse->report_buffer, 8);  // Request 8 bytes for tablet reports
    
    if (result != 0) {
        static uint32_t fail_count = 0;
        if (++fail_count <= 20 || fail_count % 100 == 0) {
            LOG_TS("[USB_MOUSE] ERROR: xhci_queue_transfer failed with code ");
            SERIAL_LOG_DEC("", result);
            SERIAL_LOG(" (fail #");
            SERIAL_LOG_DEC("", fail_count);
            SERIAL_LOG(")\n");
        }
    }
}
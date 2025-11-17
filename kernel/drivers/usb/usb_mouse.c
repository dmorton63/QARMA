#include "usb_mouse.h"
#include "usb_hid.h"
#include "usb.h"
#include "core/input/mouse.h"
#include "core/memory/heap.h"
#include "config.h"
#include "graphics/graphics.h"

// Global USB mouse device
static usb_hid_device_t *g_usb_mouse = NULL;
static usb_mouse_report_t last_report = {0};

// External mouse state from mouse.c
extern mouse_state_t mouse_state;
extern uint32_t fb_width;
extern uint32_t fb_height;

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
    usb_mouse_report_t *report_buffer = (usb_mouse_report_t *)heap_alloc(sizeof(usb_mouse_report_t));
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
    if (!report) {
        return;
    }
    
    // Update mouse state with new report data
    mouse_state.dx = report->x;
    mouse_state.dy = -report->y; // Invert Y for screen coordinates
    
    // Update absolute position (with bounds checking)
    mouse_state.x += mouse_state.dx;
    mouse_state.y += mouse_state.dy;
    
    // Clamp to screen boundaries
    if (mouse_state.x < 0) mouse_state.x = 0;
    if (mouse_state.y < 0) mouse_state.y = 0;
    if (mouse_state.x >= (int32_t)fb_width) mouse_state.x = fb_width - 1;
    if (mouse_state.y >= (int32_t)fb_height) mouse_state.y = fb_height - 1;
    
    // Update button states
    mouse_state.left_pressed = (report->buttons & 0x01) != 0;
    mouse_state.right_pressed = (report->buttons & 0x02) != 0;
    mouse_state.middle_pressed = (report->buttons & 0x04) != 0;
    
    // Update scroll wheel
    if (report->wheel > 0) {
        mouse_state.scroll_up = true;
        mouse_state.scroll_down = false;
    } else if (report->wheel < 0) {
        mouse_state.scroll_up = false;
        mouse_state.scroll_down = true;
    } else {
        mouse_state.scroll_up = false;
        mouse_state.scroll_down = false;
    }
    
    // Optional: Log significant changes for debugging
    if (report->buttons != last_report.buttons || 
        report->x != 0 || report->y != 0 || report->wheel != 0) {
        
        SERIAL_LOG("USB Mouse: Update - ");
        if (mouse_state.left_pressed) SERIAL_LOG("L");
        if (mouse_state.right_pressed) SERIAL_LOG("R");
        if (mouse_state.middle_pressed) SERIAL_LOG("M");
        // Note: Would need formatted logging for position
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
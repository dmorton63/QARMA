#ifndef USB_MOUSE_H
#define USB_MOUSE_H

#include "usb.h"
#include "usb_hid.h"
#include "core/stdtools.h"

enum {
    USB_MOUSE_OK = 0,
    USB_MOUSE_ERR_USB = -1,
    USB_MOUSE_ERR_HID = -2,
    USB_MOUSE_ERR_ENUM = -3
};

// typedef struct {
//     uint16_t vendor_id;
//     uint16_t product_id;
//     uint8_t endpoint_address;
//     uint8_t poll_interval;
//     uint8_t max_packet_size;
//     uint8_t last_report[8];
//     uint8_t last_report_len;
// } mouse_info_t;

// mouse_info_t *mouse_device = NULL;

// Function prototypes for USB mouse driver
int usb_mouse_init(void);
int usb_mouse_probe(usb_device_t *device);
int usb_mouse_attach_interface(usb_device_t *device, usb_interface_descriptor_t *interface);
int usb_mouse_find_endpoints(usb_interface_descriptor_t *interface);
void usb_mouse_start_polling(void);
void usb_mouse_report_callback(usb_transfer_t *transfer);
void usb_mouse_process_report(usb_mouse_report_t *report);
void usb_mouse_detach(void);
bool usb_mouse_is_connected(void);

#endif // USB_MOUSE_H
#ifndef USB_HID_H
#define USB_HID_H

#include "usb.h"

// USB HID device-specific timing constants (in microseconds)
// Based on Linux kernel Razer driver timing requirements
#define USB_HID_MOUSE_WAIT_MIN_US 600
#define USB_HID_MOUSE_WAIT_MAX_US 800

// Razer Mamba specific timing
#define RAZER_MAMBA_WAIT_MIN_US 600
#define RAZER_MAMBA_WAIT_MAX_US 800
#define RAZER_MAMBA_REPORT_LEN 0x5A  // 90 bytes
#define RAZER_MAMBA_ROW_LEN 15

// Wireless receiver timing (if applicable)
#define USB_HID_WIRELESS_WAIT_MIN_US 31000
#define USB_HID_WIRELESS_WAIT_MAX_US 31100

// HID Class Descriptor
typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdHID;
    uint8_t  bCountryCode;
    uint8_t  bNumDescriptors;
    uint8_t  bDescriptorType2;
    uint16_t wDescriptorLength;
} usb_hid_descriptor_t;

// HID Report Types
#define HID_REPORT_INPUT   0x01
#define HID_REPORT_OUTPUT  0x02
#define HID_REPORT_FEATURE 0x03

// HID Class Requests
#define HID_REQ_GET_REPORT   0x01
#define HID_REQ_GET_IDLE     0x02
#define HID_REQ_GET_PROTOCOL 0x03
#define HID_REQ_SET_REPORT   0x09
#define HID_REQ_SET_IDLE     0x0A
#define HID_REQ_SET_PROTOCOL 0x0B

// Mouse Report Structure (Boot Protocol - relative coordinates)
typedef struct __attribute__((packed)) {
    uint8_t buttons;    // Bit 0: Left, Bit 1: Right, Bit 2: Middle
    int8_t  x;          // X movement (relative)
    int8_t  y;          // Y movement (relative)
    int8_t  wheel;      // Wheel movement (optional)
} usb_mouse_report_t;

// Tablet Report Structure (Absolute coordinates)
typedef struct __attribute__((packed)) {
    uint8_t buttons;    // Bit 0: Left, Bit 1: Right, Bit 2: Middle
    uint16_t x;         // Absolute X position (0-32767)
    uint16_t y;         // Absolute Y position (0-32767)
    int8_t  wheel;      // Wheel movement (optional)
} usb_tablet_report_t;

// HID Device Structure
typedef struct {
    usb_device_t *device;
    uint8_t interface_num;
    uint8_t endpoint_in;
    uint8_t endpoint_out;
    uint16_t max_packet_size;
    uint8_t interval;
    uint8_t protocol;
    usb_hid_descriptor_t hid_desc;
    uint8_t *report_descriptor;
    uint16_t report_desc_length;
    bool is_mouse;
    bool is_keyboard;
} usb_hid_device_t;

// Function prototypes
int usb_hid_init(void);
void usb_hid_probe_device(void* controller, uint8_t slot, uint8_t port);
int usb_hid_probe_device_legacy(usb_device_t *device);
int usb_hid_set_protocol(usb_hid_device_t *hid_dev, uint8_t protocol);
int usb_hid_set_idle(usb_hid_device_t *hid_dev, uint8_t duration, uint8_t report_id);
int usb_hid_get_report_descriptor(usb_hid_device_t *hid_dev);
void usb_hid_mouse_callback(usb_transfer_t *transfer);

#endif // USB_HID_H
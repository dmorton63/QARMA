#ifndef USB_KEYBOARD_H
#define USB_KEYBOARD_H

#include "usb.h"
#include "usb_hid.h"
#include "core/stdtools.h"

// USB Keyboard Boot Protocol Report Format
// https://www.usb.org/sites/default/files/hid1_11.pdf Appendix B
typedef struct __attribute__((packed)) {
    uint8_t modifiers;      // Bit 0: Left Ctrl, Bit 1: Left Shift, Bit 2: Left Alt, Bit 3: Left GUI
                           // Bit 4: Right Ctrl, Bit 5: Right Shift, Bit 6: Right Alt, Bit 7: Right GUI
    uint8_t reserved;       // Reserved byte (always 0)
    uint8_t keys[6];       // Up to 6 simultaneous key presses (keycodes)
} usb_keyboard_report_t;

// Modifier key bits
#define USB_MOD_LEFT_CTRL   (1 << 0)
#define USB_MOD_LEFT_SHIFT  (1 << 1)
#define USB_MOD_LEFT_ALT    (1 << 2)
#define USB_MOD_LEFT_GUI    (1 << 3)
#define USB_MOD_RIGHT_CTRL  (1 << 4)
#define USB_MOD_RIGHT_SHIFT (1 << 5)
#define USB_MOD_RIGHT_ALT   (1 << 6)
#define USB_MOD_RIGHT_GUI   (1 << 7)

// USB HID Usage IDs for keyboards (from HID Usage Tables)
#define USB_KEY_A           0x04
#define USB_KEY_B           0x05
#define USB_KEY_C           0x06
#define USB_KEY_D           0x07
#define USB_KEY_E           0x08
#define USB_KEY_F           0x09
#define USB_KEY_G           0x0A
#define USB_KEY_H           0x0B
#define USB_KEY_I           0x0C
#define USB_KEY_J           0x0D
#define USB_KEY_K           0x0E
#define USB_KEY_L           0x0F
#define USB_KEY_M           0x10
#define USB_KEY_N           0x11
#define USB_KEY_O           0x12
#define USB_KEY_P           0x13
#define USB_KEY_Q           0x14
#define USB_KEY_R           0x15
#define USB_KEY_S           0x16
#define USB_KEY_T           0x17
#define USB_KEY_U           0x18
#define USB_KEY_V           0x19
#define USB_KEY_W           0x1A
#define USB_KEY_X           0x1B
#define USB_KEY_Y           0x1C
#define USB_KEY_Z           0x1D
#define USB_KEY_1           0x1E
#define USB_KEY_2           0x1F
#define USB_KEY_3           0x20
#define USB_KEY_4           0x21
#define USB_KEY_5           0x22
#define USB_KEY_6           0x23
#define USB_KEY_7           0x24
#define USB_KEY_8           0x25
#define USB_KEY_9           0x26
#define USB_KEY_0           0x27
#define USB_KEY_ENTER       0x28
#define USB_KEY_ESCAPE      0x29
#define USB_KEY_BACKSPACE   0x2A
#define USB_KEY_TAB         0x2B
#define USB_KEY_SPACE       0x2C
#define USB_KEY_MINUS       0x2D
#define USB_KEY_EQUAL       0x2E
#define USB_KEY_LEFTBRACE   0x2F
#define USB_KEY_RIGHTBRACE  0x30
#define USB_KEY_BACKSLASH   0x31
#define USB_KEY_SEMICOLON   0x33
#define USB_KEY_APOSTROPHE  0x34
#define USB_KEY_GRAVE       0x35
#define USB_KEY_COMMA       0x36
#define USB_KEY_DOT         0x37
#define USB_KEY_SLASH       0x38
#define USB_KEY_CAPSLOCK    0x39
#define USB_KEY_F1          0x3A
#define USB_KEY_F2          0x3B
#define USB_KEY_F3          0x3C
#define USB_KEY_F4          0x3D
#define USB_KEY_F5          0x3E
#define USB_KEY_F6          0x3F
#define USB_KEY_F7          0x40
#define USB_KEY_F8          0x41
#define USB_KEY_F9          0x42
#define USB_KEY_F10         0x43
#define USB_KEY_F11         0x44
#define USB_KEY_F12         0x45
#define USB_KEY_HOME        0x4A
#define USB_KEY_PAGEUP      0x4B
#define USB_KEY_END         0x4D
#define USB_KEY_PAGEDOWN    0x4E
#define USB_KEY_RIGHT       0x4F
#define USB_KEY_LEFT        0x50
#define USB_KEY_DOWN        0x51
#define USB_KEY_UP          0x52

// Error codes
#define USB_KBD_OK          0
#define USB_KBD_ERR_USB     -1
#define USB_KBD_ERR_HID     -2
#define USB_KBD_ERR_ENUM    -3

// Function prototypes
int usb_keyboard_init(void);
int usb_keyboard_probe(usb_device_t *device);
int usb_keyboard_attach_interface(usb_device_t *device, usb_interface_descriptor_t *interface);
int usb_keyboard_find_endpoints(usb_interface_descriptor_t *interface);
void usb_keyboard_start_polling(void);
void usb_keyboard_poll(void);
void usb_keyboard_process_report(usb_keyboard_report_t *report);
uint8_t usb_key_to_scancode(uint8_t usb_key, uint8_t modifiers);

#endif // USB_KEYBOARD_H

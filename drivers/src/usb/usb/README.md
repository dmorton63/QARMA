# USB Mouse Driver Module

This module implements USB mouse support for QARMA. It provides a layered architecture for handling USB Human Interface Devices (HID), specifically mice.

## Architecture Overview

```
Application Layer
       ↓
Mouse Interface (mouse.c/mouse.h) 
       ↓
USB Mouse Driver (usb_mouse.c/usb_mouse.h)
       ↓
USB HID Driver (usb_hid.c/usb_hid.h)
       ↓
USB Core (usb.c/usb.h)
       ↓
USB Host Controller (OHCI/EHCI/XHCI)
       ↓
Hardware
```

## Components

### 1. USB Core (`usb.c/usb.h`)
- **Purpose**: Core USB subsystem management
- **Key Functions**:
  - `usb_init()`: Initialize USB stack
  - `usb_enumerate_devices()`: Discover connected USB devices
  - `usb_control_transfer()`: Send control requests
  - `usb_interrupt_transfer()`: Handle periodic data transfers
- **Current Status**: Stub implementation with mock device support

### 2. USB HID (`usb_hid.c/usb_hid.h`)
- **Purpose**: Human Interface Device protocol handling
- **Key Functions**:
  - `usb_hid_set_protocol()`: Set boot/report protocol
  - `usb_hid_set_idle()`: Configure report frequency
  - `usb_hid_get_report_descriptor()`: Parse HID capabilities
- **Supports**: Boot protocol for simple mouse operation

### 3. USB Mouse (`usb_mouse.c/usb_mouse.h`)
- **Purpose**: Mouse-specific HID device driver
- **Key Functions**:
  - `usb_mouse_init()`: Initialize mouse driver
  - `usb_mouse_probe()`: Detect mouse interfaces
  - `usb_mouse_process_report()`: Handle mouse input data
- **Features**: Movement, buttons, scroll wheel support

### 4. Integration (`mouse.c` modifications)
- **Purpose**: Integrate USB mouse with existing mouse interface
- **Changes**: Modified `mouse_init()` to try USB first, fallback to PS/2
- **Benefits**: Seamless transition between mouse types

## Implementation Status

### ✅ Completed
- USB data structures and constants
- HID protocol definitions
- Mouse report processing
- Integration with existing mouse interface
- Basic USB device enumeration (mock)

### 🚧 Partial/Stub
- USB host controller initialization (requires OHCI/EHCI/XHCI)
- USB transfer management (requires hardware integration)
- Device enumeration (currently uses mock device)
- Error handling and recovery

### ❌ Not Implemented
- Real USB host controller drivers (OHCI/EHCI/XHCI)
- PCI device detection for USB controllers
- USB hub support
- Power management
- Hotplug support
- Multi-device support

## Next Steps to Complete Implementation

### 1. USB Host Controller Driver
Choose and implement one of:
- **OHCI** (USB 1.1) - Simpler, good for learning
- **EHCI** (USB 2.0) - More common, faster
- **XHCI** (USB 3.0+) - Modern, most complex

### 2. PCI Integration
```c
// Scan for USB controllers
pci_device_t *usb_controller = pci_find_device(USB_CLASS_SERIAL_BUS, USB_SUBCLASS_USB);
```

### 3. Memory Management
- DMA-coherent memory allocation for transfer descriptors
- Proper buffer management for USB transfers

### 4. Interrupt Handling
- Register USB controller interrupt handler
- Process transfer completions asynchronously

### 5. Timer Integration
- Implement periodic transfers using system timer
- Handle USB timeouts and retries

## Testing Current Implementation

The current implementation includes a mock USB mouse device that can be used to test the integration without real USB hardware:

1. **Build**: USB files are ready for compilation (once Makefile is updated)
2. **Test**: Mock device simulates mouse reports
3. **Debug**: Serial logging shows USB initialization steps

## Configuration

To enable USB mouse support:

1. Add USB files to Makefile
2. Call `usb_mouse_init()` from `mouse_init()`
3. Configure QEMU with USB mouse emulation:
   ```bash
   qemu-system-i386 -usb -device usb-mouse ...
   ```

## Troubleshooting

### Common Issues
1. **Build Errors**: Ensure all include paths are correct
2. **No Mouse Detected**: Check USB controller initialization
3. **Transfer Failures**: Verify host controller setup
4. **Performance**: Optimize transfer scheduling and interrupt handling

### Debug Features
- Serial logging for all major operations
- Transfer status tracking
- Device enumeration reporting
- Report parsing verification

## Future Enhancements

1. **Multiple Mouse Support**: Handle multiple USB mice simultaneously
2. **Advanced Features**: Support for gaming mouse features (high DPI, extra buttons)
3. **Hot-plug Support**: Dynamic device insertion/removal
4. **Power Management**: USB suspend/resume support
5. **Error Recovery**: Robust handling of USB errors and disconnections

This module provides the foundation for a complete USB mouse implementation. The mock device support allows for immediate testing and development of the upper layers while the USB host controller implementation is completed.
#!/bin/bash
# QARMA Test Script
# Run this to test your keyboard-enabled OS

echo "Starting QARMA with keyboard support..."
echo "Press Ctrl+Alt+G to release mouse from QEMU window"
echo "Press Ctrl+Alt+2 to switch to QEMU monitor"
echo "Type 'quit' in monitor to exit QEMU"
echo ""
echo "Starting in 3 seconds..."
sleep 3

cd /home/dmort/qarma
# Explicitly specify UHCI controller (not EHCI)
# piix3-usb-uhci should give us ProgIF 0x00
qemu-system-i386 -cdrom build/qarma.iso -m 256M -vga std \
  -device piix3-usb-uhci \
  -device usb-mouse \
  -serial stdio
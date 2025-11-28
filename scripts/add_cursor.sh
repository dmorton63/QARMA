#!/bin/bash
# Quick Start: Add Custom Cursor to QARMA

echo "=== Adding a Custom Cursor to QARMA ==="
echo ""

# Check if we're in the qarma directory
if [ ! -f "Makefile" ]; then
    echo "Error: Run this from /home/dmort/qarma"
    exit 1
fi

echo "Step 1: Create a simple test cursor"
echo "-----------------------------------"

# Check for ImageMagick
if ! command -v convert &> /dev/null; then
    echo "ImageMagick not found. Installing..."
    echo "Run: sudo apt install imagemagick"
    echo ""
    echo "For now, I'll create a placeholder..."
    mkdir -p assets/cursors
    echo "Place your arrow.png (16x16 or 32x32) in assets/cursors/"
else
    echo "Creating a test cursor with ImageMagick..."
    mkdir -p assets/cursors
    
    # Create a simple arrow cursor
    convert -size 32x32 xc:none \
        -fill white -stroke black -strokewidth 2 \
        -draw "path 'M 4,4 L 4,24 L 12,16 L 18,28 L 22,26 L 16,14 L 26,14 Z'" \
        assets/cursors/arrow.png
    
    echo "✓ Created assets/cursors/arrow.png"
fi

echo ""
echo "Step 2: Rebuild QARMA"
echo "--------------------"
echo "Running: make"
make clean > /dev/null 2>&1
make 2>&1 | grep -E "(Compiling.*cursor|Creating QARMA|ISO image produced)"

echo ""
echo "Step 3: What happens when you boot?"
echo "-----------------------------------"
echo "1. QARMA boots and initializes graphics"
echo "2. cursor_loader_init() is called"
echo "3. It creates default software-rendered cursors"
echo "4. It tries to load assets/cursors/arrow.png from ISO"
echo "5. If found, it replaces the default cursor"
echo "6. Your custom cursor is now used!"
echo ""

echo "Step 4: Run and test"
echo "-------------------"
echo "Run: make qemu"
echo ""
echo "Move your mouse - you should see:"
if [ -f "assets/cursors/arrow.png" ]; then
    echo "  ✓ Your custom cursor image!"
else
    echo "  • Default white/black arrow (until you add arrow.png)"
fi

echo ""
echo "=== Current Status ==="
if [ -d "assets/cursors" ]; then
    echo "Cursor files:"
    ls -lh assets/cursors/*.png 2>/dev/null || echo "  (none yet - using defaults)"
else
    echo "No assets/cursors directory yet"
fi

echo ""
echo "=== Quick Reference ==="
echo "Cursor types you can create:"
echo "  arrow.png    - Default pointer"
echo "  hand.png     - Hover over buttons"
echo "  ibeam.png    - Text editing"
echo "  wait.png     - Loading/busy"
echo "  crosshair.png - Precision/drawing"
echo ""
echo "All cursors should be:"
echo "  • PNG format with alpha transparency"
echo "  • 16x16, 24x24, or 32x32 pixels"
echo "  • Named exactly as shown above"

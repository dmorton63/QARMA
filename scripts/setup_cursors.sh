#!/bin/bash
# Example: Adding custom cursor icons to QARMA

echo "=== QARMA Cursor Icon Setup ==="
echo ""

# Method 1: Embedded in ISO (works now)
echo "Method 1: Embed cursors in ISO"
echo "-------------------------------"
echo "1. Place PNG files in: assets/cursors/"
echo "   - arrow.png (default cursor)"
echo "   - hand.png (hover over buttons)"
echo "   - ibeam.png (text editing)"
echo ""
echo "2. Rebuild: make"
echo "3. Run: make qemu"
echo ""

# Method 2: Load from host (requires 9P implementation)
echo "Method 2: Load from host filesystem"
echo "-----------------------------------"
echo "1. Place PNG files in: shared_files/cursors/"
echo "2. Run: make qemu"
echo "3. QARMA loads from /host/cursors/ at runtime"
echo ""

# Example workflow
echo "=== Quick Example ==="
echo ""
echo "# Create a simple test cursor"
echo "cd assets/cursors"
echo ""
echo "# Option A: Use ImageMagick to create custom cursor"
echo "convert -size 32x32 xc:none -fill white -stroke black -strokewidth 2 \\"
echo "  -draw \"path 'M 2,2 L 2,28 L 12,18 L 20,32 L 24,30 L 16,16 L 28,16 Z'\" \\"
echo "  arrow.png"
echo ""
echo "# Option B: Copy existing cursor image"
echo "cp ~/Pictures/my-cursor.png arrow.png"
echo ""
echo "# Rebuild and test"
echo "cd ../.."
echo "make qemu"
echo ""

# Quick test for presence of ImageMagick
if command -v convert &> /dev/null; then
    echo "✓ ImageMagick detected - you can create custom cursors"
else
    echo "✗ ImageMagick not found (optional)"
    echo "  Install: sudo apt install imagemagick"
fi

echo ""
echo "=== Current Status ==="
ls -lh assets/cursors/*.png 2>/dev/null || echo "No PNG cursors yet (using default software-rendered cursors)"
echo ""
echo "Files in shared directory:"
ls -lh shared_files/ 2>/dev/null | head -10

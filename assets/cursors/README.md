# QARMA Cursor Assets

This directory contains cursor/icon images that will be embedded into the QARMA ISO.

## Supported Formats

- **PNG** (preferred) - Supports transparency
- Size: 16x16 to 64x64 pixels recommended
- ARGB format for proper alpha blending

## Cursor Files

Place your cursor images here with these names:

- `arrow.png` - Default cursor (standard pointer)
- `hand.png` - Hand/pointer cursor (for buttons/links)
- `ibeam.png` - Text cursor (I-beam for text selection)
- `wait.png` - Wait/busy cursor (hourglass/spinner)
- `crosshair.png` - Precision cursor (for drawing)

## How It Works

### Method 1: Embedded in ISO (Current)

1. Place PNG files in `assets/cursors/`
2. Run `make` - files are automatically copied into the ISO
3. QARMA loads them from `/assets/cursors/` at boot

### Method 2: From Host Shared Directory (Future)

Once VirtIO 9P is fully implemented:
1. Place files in `shared_files/cursors/`
2. QARMA loads them from `/host/cursors/` at runtime
3. Can hot-swap cursors without rebuilding ISO

## Creating Custom Cursors

### Using GIMP:
1. Create 32x32 image (or 16x16, 24x24, 48x48)
2. Add alpha channel (Layer → Transparency → Add Alpha Channel)
3. Draw your cursor with transparency
4. Export as PNG (with alpha)

### Using ImageMagick:
```bash
# Convert existing cursor
convert cursor.png -resize 32x32 arrow.png

# Create with transparency
convert -size 32x32 xc:none -fill black -draw "path 'M 0,0 L 0,20 L 5,15 Z'" arrow.png
```

## Hotspot

Cursors have a "hotspot" - the exact pixel that represents the click point.
- Arrow: Top-left (0, 0)
- Hand: Tip of index finger (~5, 0)
- I-beam: Center (8, 8 for 16x16)
- Crosshair: Center

Hotspot is configured in `cursor_loader.c`

## Example: Adding a Custom Cursor

```bash
cd /home/dmort/qarma/assets/cursors
# Copy your custom cursor
cp ~/Pictures/my-cool-arrow.png arrow.png
# Rebuild
cd /home/dmort/qarma && make
```

## Default Cursors

If no PNG files are found, QARMA uses simple software-rendered cursors created in `cursor_loader.c`.

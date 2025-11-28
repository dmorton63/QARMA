# How to Use Custom Cursors in QARMA

## Quick Answer

1. **Add cursor image**: Copy `arrow.png` to `assets/cursors/`
2. **Build**: Run `make`
3. **Test**: Run `make qemu`
4. **See it**: Move your mouse - custom cursor appears!

## Detailed Steps

### Adding Your First Cursor

```bash
cd /home/dmort/qarma

# Option 1: Copy an existing PNG
cp ~/Pictures/my-cursor.png assets/cursors/arrow.png

# Option 2: Download a cursor pack
wget https://example.com/cursors/arrow.png -O assets/cursors/arrow.png

# Option 3: Create with ImageMagick (if installed)
convert -size 32x32 xc:none -fill white -stroke black \
  -draw "path 'M 4,4 L 4,24 L 14,16 Z'" assets/cursors/arrow.png
```

### Build and Run

```bash
make        # Builds kernel, packages cursor into ISO
make qemu   # Boots QARMA in emulator
```

### What Happens Automatically

When QARMA boots:

1. **Graphics Initialize** → Framebuffer ready
2. **Compositor Starts** → Window manager ready  
3. **`cursor_loader_init()` Called** → This is automatic!
   - Creates default software cursors (white/black arrow)
   - Searches ISO for `/assets/cursors/arrow.png`
   - If found: Loads PNG and uses it
   - If not found: Uses default
4. **Mouse Moves** → Your cursor renders!

### See It Working

- **Move mouse** - cursor follows
- **No PNG added?** - Shows built-in white/black arrow
- **PNG added?** - Shows your custom cursor with transparency!

## Cursor Files Supported

Place these in `assets/cursors/`:

| Filename | Used For | Hotspot |
|----------|----------|---------|
| `arrow.png` | Default pointer | Top-left (0,0) |
| `hand.png` | Hovering buttons | Finger tip (5,0) |
| `ibeam.png` | Text editing | Center (8,8) |
| `wait.png` | Loading/busy | Center |
| `crosshair.png` | Precision work | Center |

## Requirements

- **Format**: PNG with alpha channel
- **Size**: 16x16, 24x24, or 32x32 pixels recommended
- **Alpha**: Transparency supported (ARGB)
- **Location**: `assets/cursors/[name].png`

## Testing Without Custom Images

Don't have cursor images yet? **No problem!**

The system includes nice default cursors. Just run:

```bash
make qemu
```

You'll see a white arrow cursor with black outline. It works perfectly - add custom PNGs whenever you're ready.

## What's Already Working

✅ Cursor loader initializes at boot  
✅ Default cursors created in memory  
✅ PNG loading framework ready  
✅ Alpha blending working  
✅ Multiple cursor types supported  
✅ Hotspot positioning  
✅ Automatic fallback to defaults  

## Advanced: Using Host Shared Directory

**Future feature** (requires 9P protocol):

```bash
# Place cursors in shared directory
cp *.png shared_files/cursors/

# Run QEMU (shared dir is already configured)
make qemu

# Once 9P is implemented:
# QARMA loads from /host/cursors/ at runtime
# Hot-swap cursors without rebuilding!
```

## Troubleshooting

**Q: I added arrow.png but still see the default cursor?**
- Make sure it's in `assets/cursors/arrow.png` (not `assets/` or `cursors/`)
- Rebuild with `make clean && make`
- Check file is PNG format: `file assets/cursors/arrow.png`

**Q: My cursor has no transparency?**
- Ensure PNG has alpha channel
- In GIMP: Layer → Transparency → Add Alpha Channel
- Re-export as PNG (not JPG!)

**Q: Cursor looks wrong/corrupted?**
- Check PNG isn't too large (>64x64 may cause issues)
- Try 32x32 or 16x16
- Verify PNG isn't corrupt: `pngcheck assets/cursors/arrow.png`

## Next Steps

1. **Try it now**: `make qemu` - see the default cursor
2. **Add custom**: Copy a PNG to `assets/cursors/arrow.png`
3. **Rebuild**: `make && make qemu`
4. **Enjoy**: Your custom cursor is now in QARMA!

That's it! No configuration files, no setup - just drop in a PNG and rebuild.

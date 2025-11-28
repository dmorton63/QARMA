# Using Host Files and Assets in QARMA

Yes! You can absolutely copy icons and other files for use in QARMA. Here are two methods:

## Method 1: Embed in ISO (✅ Works Now!)

Perfect for static assets like cursors, fonts, icons that don't change often.

### Setup
```bash
cd /home/dmort/qarma

# Add cursor images
cp ~/Pictures/arrow-cursor.png assets/cursors/arrow.png
cp ~/Pictures/hand-cursor.png assets/cursors/hand.png

# Rebuild - cursors are automatically included in ISO
make

# Test
make qemu
```

### How it works
- Files in `assets/cursors/` are copied into ISO at `/assets/cursors/`
- QARMA's cursor loader (`cursor_loader.c`) loads them at boot
- No external dependencies - everything is self-contained

### Supported
- ✅ PNG images (with alpha transparency)
- ✅ Any static assets
- ✅ Works immediately

## Method 2: Load from Host via VirtFS (⏳ In Progress)

Perfect for development - edit files on host, see changes in QARMA without rebuilding.

### Setup
```bash
# Place files in shared directory
mkdir -p shared_files/cursors
cp ~/Pictures/*.png shared_files/cursors/

# Run QEMU (shared directory is already configured)
make qemu
```

### How it works
- QEMU's VirtIO 9P shares `./shared_files` with QARMA
- Appears at `/host` in QARMA filesystem
- ✅ Device detection working
- ⏳ Full 9P protocol implementation needed
- Once complete: hot-reload assets without ISO rebuild!

## Current Status

### ✅ Working Now
- ISO asset embedding (Method 1)
- VirtIO 9P device detection
- QEMU configured with host directory sharing
- Default software-rendered cursors
- Framework for loading PNG cursors

### ⏳ In Progress
- Full 9P protocol (TVERSION, TATTACH, TWALK, TREAD)
- Runtime loading from `/host`
- Hot-reloading assets

## Quick Examples

### Add a custom mouse cursor:
```bash
# Copy your cursor image
cp my-arrow.png assets/cursors/arrow.png

# Rebuild and test
make qemu
```

### Place test files for future 9P access:
```bash
# These will be accessible at /host once 9P is complete
echo "config=value" > shared_files/qarma.conf
cp data.json shared_files/
mkdir -p shared_files/cursors
cp *.png shared_files/cursors/
```

## File Locations

| Purpose | ISO Path | Host Path | Status |
|---------|----------|-----------|--------|
| Cursors | `/assets/cursors/*.png` | `assets/cursors/*.png` | ✅ Working |
| Shared files | `/host/*` | `shared_files/*` | ⏳ 9P protocol needed |
| Config | `/host/qarma.conf` | `shared_files/qarma.conf` | ⏳ 9P protocol needed |

## Next Steps

Want to implement full 9P support? The driver skeleton is in:
- `kernel/drivers/virtio_9p.c` - PCI detection ✅, protocol ⏳
- `headers/drivers/virtio_9p.h` - API definition

Need to implement:
1. Virtqueue setup
2. 9P message encoding/decoding
3. File operations (open, read, write, close)

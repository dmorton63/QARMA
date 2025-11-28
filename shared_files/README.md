# QARMA OS Shared Files

This directory is shared between your Linux host and QARMA OS via VirtIO 9P (VirtFS).

## How It Works

- **Host Side**: Files placed in `./shared_files/` are visible to QARMA
- **QARMA Side**: Accessible at `/host` mount point (once driver is fully implemented)
- **Technology**: VirtIO 9P filesystem protocol over PCI

## Current Status

✅ VirtIO 9P driver skeleton created
✅ PCI device detection implemented  
✅ QEMU configured with `-fsdev` and `-device virtio-9p-pci`
⏳ Full 9P protocol implementation (TVERSION, TATTACH, TWALK, TREAD, etc.) - TODO

## Usage

1. Place files in `./shared_files/` on the host
2. Boot QARMA OS: `make qemu`
3. Once the protocol is fully implemented, access files via `/host/` path

## Use Cases

### Cursors & Icons
```bash
mkdir -p shared_files/cursors
cp my-arrow.png shared_files/cursors/arrow.png
```
Once 9P is working, QARMA can load cursors from `/host/cursors/` at runtime!

### Configuration Files
```bash
echo "theme=dark" > shared_files/qarma.conf
```

### Data Files
```bash
cp data.json shared_files/
```

## Test Files

- `test.txt` - Simple test file to verify access
- `README.md` - This file

## Next Steps

The driver currently detects the VirtIO 9P device but needs full protocol implementation:
- Virtqueue setup and management
- 9P2000.L message formatting and parsing  
- File operations (open, read, write, close)
- Directory operations (readdir, stat)

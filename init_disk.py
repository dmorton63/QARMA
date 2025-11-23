#!/usr/bin/env python3
"""
Initialize qarma_disk.img with SimpleFS format
"""
import struct
import sys

# SimpleFS header structure
SIMPLEFS_MAGIC = 0x51554144  # "QUAD"
MAX_FILES = 16
DISK_SIZE = 10 * 1024 * 1024  # 10MB

def main():
    # Create disk image
    with open('qarma_disk.img', 'wb') as f:
        # Write SimpleFS header (sector 0)
        # magic (4 bytes) + file_count (4 bytes) + file entries
        header = struct.pack('<I', SIMPLEFS_MAGIC)  # magic
        header += struct.pack('<I', 0)  # file_count = 0
        
        # Write 16 file entries (each 44 bytes: 32 name + 4 offset + 4 size + 4 used)
        for i in range(MAX_FILES):
            header += b'\x00' * 32  # name
            header += struct.pack('<I', 0)  # offset
            header += struct.pack('<I', 0)  # size
            header += struct.pack('<I', 0)  # used
        
        # Pad to 512 bytes
        header += b'\x00' * (512 - len(header))
        f.write(header)
        
        # Write rest of disk (zeros)
        remaining = DISK_SIZE - 512
        chunk_size = 1024 * 1024  # 1MB chunks
        while remaining > 0:
            write_size = min(chunk_size, remaining)
            f.write(b'\x00' * write_size)
            remaining -= write_size
    
    print(f"Created qarma_disk.img ({DISK_SIZE} bytes) with SimpleFS format")
    print(f"Magic: 0x{SIMPLEFS_MAGIC:08X}")
    print(f"File slots: {MAX_FILES}")

if __name__ == '__main__':
    main()

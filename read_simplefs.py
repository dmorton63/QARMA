#!/usr/bin/env python3
"""
Read files from SimpleFS disk image
"""
import struct
import sys

SIMPLEFS_MAGIC = 0x51554144  # "QUAD"
MAX_FILES = 16

def read_simplefs(disk_path):
    with open(disk_path, 'rb') as f:
        # Read header (sector 0)
        header = f.read(512)
        
        # Parse header
        magic, file_count = struct.unpack_from('<II', header, 0)
        
        if magic != SIMPLEFS_MAGIC:
            print(f"Error: Invalid magic number 0x{magic:08X} (expected 0x{SIMPLEFS_MAGIC:08X})")
            return
        
        print(f"SimpleFS Disk Image: {disk_path}")
        print(f"Magic: 0x{magic:08X}")
        print(f"Files: {file_count}/{MAX_FILES}")
        print()
        
        # Read full disk to access file data
        f.seek(0)
        disk_data = f.read()
        
        # Parse file entries
        offset = 8
        files = []
        for i in range(MAX_FILES):
            if offset + 44 > len(header):
                break
            name_bytes = header[offset:offset+32]
            name = name_bytes.split(b'\x00')[0].decode('ascii', errors='ignore')
            file_offset, file_size, used = struct.unpack_from('<III', header, offset + 32)
            
            if used:
                files.append({
                    'name': name,
                    'offset': file_offset,
                    'size': file_size,
                    'index': i
                })
            
            offset += 44  # 32 + 4 + 4 + 4
        
        if not files:
            print("No files found.")
            return
        
        # Display files
        print("Files:")
        print(f"{'Name':<32} {'Size':>8}  {'Offset':>8}")
        print("-" * 50)
        for file_info in files:
            print(f"{file_info['name']:<32} {file_info['size']:>8}  0x{file_info['offset']:06X}")
        
        print()
        
        # Extract files
        for file_info in files:
            data = disk_data[file_info['offset']:file_info['offset'] + file_info['size']]
            
            output_name = f"extracted_{file_info['name']}"
            with open(output_name, 'wb') as out:
                out.write(data)
            print(f"Extracted: {output_name} ({file_info['size']} bytes)")
            
            # Show hex dump of first 64 bytes
            print(f"  First 64 bytes:")
            for i in range(0, min(64, len(data)), 16):
                hex_str = ' '.join(f'{b:02x}' for b in data[i:i+16])
                ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in data[i:i+16])
                print(f"    {i:04x}: {hex_str:<48}  {ascii_str}")
            print()

if __name__ == '__main__':
    disk_path = sys.argv[1] if len(sys.argv) > 1 else 'qarma_disk.img'
    read_simplefs(disk_path)

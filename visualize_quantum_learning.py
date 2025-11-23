#!/usr/bin/env python3
"""
Visualize quantum learning data from qlearn.dat
"""
import struct
import sys

def parse_qlearn_file(filename):
    """Parse the quantum learning data file"""
    with open(filename, 'rb') as f:
        data = f.read()
    
    # Parse header (20 bytes)
    if len(data) < 20:
        print("File too small")
        return
    
    magic, version, data_type, data_size, checksum = struct.unpack('<IIIII', data[:20])
    
    print(f"=== Quantum Learning Data File ===")
    print(f"Magic: 0x{magic:08X} {'(valid)' if magic == 0x51414941 else '(INVALID!)'}")
    print(f"Version: {version}")
    print(f"Data type: {data_type} (1=quantum, 2=cache)")
    print(f"Data size: {data_size} bytes")
    print(f"Checksum: 0x{checksum:08X}")
    print()
    
    if magic != 0x51414941:
        print("Invalid magic number!")
        return
    
    # Parse learning data
    offset = 20
    
    # Read db_size (number of entries)
    if len(data) < offset + 4:
        print("No learning data")
        return
    
    db_size = struct.unpack('<I', data[offset:offset+4])[0]
    offset += 4
    
    print(f"Learning Database Entries: {db_size}")
    print()
    
    # Each entry structure:
    # quantum_workload_profile_t (24 bytes)
    # strategy_metrics_t[6] (20 bytes × 6 = 120 bytes)
    # observation_count (4 bytes)
    # confidence (4 bytes)
    # Total: 152 bytes per entry
    
    entry_size = 152
    
    for i in range(db_size):
        if len(data) < offset + entry_size:
            print(f"Warning: Truncated entry {i}")
            break
        
        entry_data = data[offset:offset+entry_size]
        offset += entry_size
        
        # Parse workload profile (24 bytes: 4+4+4+4+4+4)
        # Note: bool might be padded, so treating as uint32_t
        qubit_count, avg_exec_time, variance, has_eval, requires_all, data_sz = \
            struct.unpack('<IIIIII', entry_data[:24])
        
        print(f"--- Entry {i+1} ---")
        print(f"Workload Profile:")
        print(f"  Qubits: {qubit_count}")
        print(f"  Avg execution time: {avg_exec_time} ms")
        print(f"  Variance: {variance}")
        print(f"  Has evaluation: {bool(has_eval)}")
        print(f"  Requires all: {bool(requires_all)}")
        print(f"  Data size: {data_sz} bytes")
        print()
        
        # Parse strategy metrics (6 strategies × 20 bytes)
        strategies = [
            "SEQUENTIAL",
            "PARALLEL", 
            "ADAPTIVE",
            "PROBABILISTIC",
            "AI_PREDICTED",
            "ENSEMBLE"
        ]
        
        print(f"Strategy Performance:")
        metrics_offset = 24
        for j, strategy_name in enumerate(strategies):
            total_uses, success_count, total_time, last_used = \
                struct.unpack('<IIII', entry_data[metrics_offset:metrics_offset+16])
            avg_quality = struct.unpack('<f', entry_data[metrics_offset+16:metrics_offset+20])[0]
            metrics_offset += 20
            
            if total_uses > 0:
                success_rate = (success_count / total_uses) * 100
                avg_time = total_time / total_uses if total_uses > 0 else 0
                print(f"  {strategy_name:15} | Uses: {total_uses:3} | "
                      f"Success: {success_rate:5.1f}% | "
                      f"Avg time: {avg_time:6.1f}ms | "
                      f"Quality: {avg_quality:4.2f}")
        
        # Parse observation count and confidence
        obs_count = struct.unpack('<I', entry_data[144:148])[0]
        confidence = struct.unpack('<f', entry_data[148:152])[0]
        
        print(f"\nObservations: {obs_count}")
        print(f"Confidence: {confidence:.1%}")
        print()

if __name__ == '__main__':
    # Read from extracted file or directly from disk image
    if len(sys.argv) > 1:
        filename = sys.argv[1]
    else:
        # Try to find the extracted file
        import os
        if os.path.exists('extracted_qlearn.dat'):
            filename = 'extracted_qlearn.dat'
        else:
            print("Usage: python3 visualize_quantum_learning.py <qlearn.dat>")
            print("Or run: python3 read_simplefs.py qarma_disk.img")
            print("to extract the file first")
            sys.exit(1)
    
    parse_qlearn_file(filename)

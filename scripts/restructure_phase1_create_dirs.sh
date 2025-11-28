#!/bin/bash

# QARMA Restructure Script - Phase 1: Create Directory Structure
# This script creates the new component-based directory structure

set -e  # Exit on error

echo "==================================="
echo "QARMA Restructure - Phase 1"
echo "Creating new directory structure"
echo "==================================="
echo ""

# Create new structure
echo "Creating kernel directories..."
mkdir -p kernel/{headers,src}

echo "Creating core directories..."
mkdir -p core/headers/{scheduler,memory/pmm,memory/vmm,input,text_functions,overlay}
mkdir -p core/src/{scheduler,memory/pmm,memory/vmm,input,text_functions,overlay}

echo "Creating drivers directories..."
mkdir -p drivers/headers/{usb,net,block}
mkdir -p drivers/src/{usb,net,block}

echo "Creating fs directories..."
mkdir -p fs/headers/file_subsystem
mkdir -p fs/src/file_subsystem

echo "Creating graphics directories..."
mkdir -p graphics/headers/subsystem
mkdir -p graphics/src/subsystem

echo "Creating gui directories..."
mkdir -p gui/headers/controls
mkdir -p gui/src/controls

echo "Creating window_manager directories..."
mkdir -p window_manager/{headers,src}

echo "Creating keyboard directories..."
mkdir -p keyboard/{headers,src}

echo "Creating network directories..."
mkdir -p network/{headers,src}

echo "Creating shell directories..."
mkdir -p shell/{headers,src}

echo "Creating ai directories..."
mkdir -p ai/{headers,src}

echo "Creating quantum directories..."
mkdir -p quantum/{headers,src}

echo "Creating security directories..."
mkdir -p security/{headers,src}

echo "Creating splash_app directories..."
mkdir -p splash_app/{headers,src}

echo "Creating parallel directories..."
mkdir -p parallel/{headers,src}

echo "Creating ide directories..."
mkdir -p ide/{headers,src}

echo ""
echo "✓ Directory structure created successfully"
echo ""
echo "Next steps:"
echo "1. Review the structure: ls -R kernel/ core/ drivers/"
echo "2. Run phase 2 script to move files"
echo "3. Update Makefile"
echo "4. Update includes"

#!/bin/bash
# Phase 4: Fix include statements to work with new structure

echo "=== Phase 4: Fixing Include Statements ==="

# For files in each component, fix includes to use proper paths
# Since we have -I<component>/headers, includes should be relative to that

cd /home/dmort/qarma

# Fix includes in all C files
find core/src drivers/src fs/src graphics/src gui/src window_manager/src \
     keyboard/src network/src shell/src ai/src quantum/src security/src \
     parallel/src ide/src splash_app/src kernel/src -name "*.c" -o -name "*.h" | while read file; do
    
    # Determine which component this file belongs to
    component=$(echo "$file" | cut -d'/' -f1)
    
    # Skip if it's already a header file in headers/ (those are the targets)
    if [[ "$file" == *"/headers/"* ]]; then
        continue
    fi
    
    # Common patterns to fix:
    # 1. Simple includes like "task_manager.h" need proper path
    # 2. Parent directory includes like "../kernel.h" 
    # 3. Subdirectory includes need adjustment
    
    echo "Processing $file..."
    
    # This is complex - we'll need to handle case by case
    # For now, let's just update the obvious ones:
    
    # kernel.h is in core/headers
    sed -i 's|#include "../kernel.h"|#include "kernel.h"|g' "$file"
    sed -i 's|#include "../../kernel.h"|#include "kernel.h"|g' "$file"
    
    # config.h is in core/headers
    sed -i 's|#include "../config.h"|#include "config.h"|g' "$file"
    sed -i 's|#include "../../config.h"|#include "config.h"|g' "$file"
    
    # string.h is in core/headers
    sed -i 's|#include "../string.h"|#include "string.h"|g' "$file"
    sed -i 's|#include "../../string.h"|#include "string.h"|g' "$file"
    
    # memory/heap.h
    sed -i 's|#include "../memory/heap.h"|#include "memory/heap.h"|g' "$file"
    sed -i 's|#include "../../memory/heap.h"|#include "memory/heap.h"|g' "$file"
    
done

echo "=== Phase 4 Complete ==="
echo "Note: This is a first pass. Some includes may need manual adjustment."

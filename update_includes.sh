#!/bin/bash

# Script to update all #include statements to use new headers directory structure

cd /home/dmort/qarma

echo "Updating #include statements in all .c and .h files..."

# Find all C and header files
find kernel -type f \( -name "*.c" -o -name "*.h" \) | while read file; do
    # Skip if file doesn't exist
    [ ! -f "$file" ] && continue
    
    # Backup original
    cp "$file" "$file.bak"
    
    # Update includes - replace relative paths with headers/ paths
    sed -i 's|#include "\.\./\.\./\.\./kernel_types\.h"|#include "kernel_types.h"|g' "$file"
    sed -i 's|#include "\.\./\.\./kernel_types\.h"|#include "kernel_types.h"|g' "$file"
    sed -i 's|#include "\.\./kernel_types\.h"|#include "kernel_types.h"|g' "$file"
    sed -i 's|#include "\.\.kernel_types\.h"|#include "kernel_types.h"|g' "$file"
    
    sed -i 's|#include "\.\./\.\./\.\./config\.h"|#include "config.h"|g' "$file"
    sed -i 's|#include "\.\./\.\./config\.h"|#include "config.h"|g' "$file"
    sed -i 's|#include "\.\./config\.h"|#include "config.h"|g' "$file"
    
    sed -i 's|#include "\.\./\.\./\.\./assert\.h"|#include "assert.h"|g' "$file"
    sed -i 's|#include "\.\./\.\./assert\.h"|#include "assert.h"|g' "$file"
    sed -i 's|#include "\.\./assert\.h"|#include "assert.h"|g' "$file"
    
    sed -i 's|#include "\.\./\.\./\.\./splash_data\.h"|#include "splash_data.h"|g' "$file"
    sed -i 's|#include "\.\./\.\./splash_data\.h"|#include "splash_data.h"|g' "$file"
    sed -i 's|#include "\.\./splash_data\.h"|#include "splash_data.h"|g' "$file"
    
    # Core directory
    sed -i 's|#include "\.\./\.\./\.\./core/\([^"]*\)"|#include "core/\1"|g' "$file"
    sed -i 's|#include "\.\./\.\./core/\([^"]*\)"|#include "core/\1"|g' "$file"
    sed -i 's|#include "\.\./core/\([^"]*\)"|#include "core/\1"|g' "$file"
    sed -i 's|#include "\.\.core/\([^"]*\)"|#include "core/\1"|g' "$file"
    
    # GUI directory
    sed -i 's|#include "\.\./\.\./\.\./gui/\([^"]*\)"|#include "gui/\1"|g' "$file"
    sed -i 's|#include "\.\./\.\./gui/\([^"]*\)"|#include "gui/\1"|g' "$file"
    sed -i 's|#include "\.\./gui/\([^"]*\)"|#include "gui/\1"|g' "$file"
    sed -i 's|#include "\.\.gui/\([^"]*\)"|#include "gui/\1"|g' "$file"
    
    # Graphics directory
    sed -i 's|#include "\.\./\.\./\.\./graphics/\([^"]*\)"|#include "graphics/\1"|g' "$file"
    sed -i 's|#include "\.\./\.\./graphics/\([^"]*\)"|#include "graphics/\1"|g' "$file"
    sed -i 's|#include "\.\./graphics/\([^"]*\)"|#include "graphics/\1"|g' "$file"
    sed -i 's|#include "\.\.graphics/\([^"]*\)"|#include "graphics/\1"|g' "$file"
    
    # Network directory
    sed -i 's|#include "\.\./\.\./\.\./network/\([^"]*\)"|#include "network/\1"|g' "$file"
    sed -i 's|#include "\.\./\.\./network/\([^"]*\)"|#include "network/\1"|g' "$file"
    sed -i 's|#include "\.\./network/\([^"]*\)"|#include "network/\1"|g' "$file"
    
    # Drivers directory
    sed -i 's|#include "\.\./\.\./\.\./drivers/\([^"]*\)"|#include "drivers/\1"|g' "$file"
    sed -i 's|#include "\.\./\.\./drivers/\([^"]*\)"|#include "drivers/\1"|g' "$file"
    sed -i 's|#include "\.\./drivers/\([^"]*\)"|#include "drivers/\1"|g' "$file"
    
    # Filesystem directory
    sed -i 's|#include "\.\./\.\./\.\./fs/\([^"]*\)"|#include "fs/\1"|g' "$file"
    sed -i 's|#include "\.\./\.\./fs/\([^"]*\)"|#include "fs/\1"|g' "$file"
    sed -i 's|#include "\.\./fs/\([^"]*\)"|#include "fs/\1"|g' "$file"
    
    # Keyboard directory
    sed -i 's|#include "\.\./\.\./\.\./keyboard/\([^"]*\)"|#include "keyboard/\1"|g' "$file"
    sed -i 's|#include "\.\./\.\./keyboard/\([^"]*\)"|#include "keyboard/\1"|g' "$file"
    sed -i 's|#include "\.\./keyboard/\([^"]*\)"|#include "keyboard/\1"|g' "$file"
    
    # QARMA win handle directory
    sed -i 's|#include "\.\./\.\./\.\./qarma_win_handle/\([^"]*\)"|#include "qarma_win_handle/\1"|g' "$file"
    sed -i 's|#include "\.\./\.\./qarma_win_handle/\([^"]*\)"|#include "qarma_win_handle/\1"|g' "$file"
    sed -i 's|#include "\.\./qarma_win_handle/\([^"]*\)"|#include "qarma_win_handle/\1"|g' "$file"
    
    # Splash app directory
    sed -i 's|#include "\.\./\.\./\.\./splash_app/\([^"]*\)"|#include "splash_app/\1"|g' "$file"
    sed -i 's|#include "\.\./\.\./splash_app/\([^"]*\)"|#include "splash_app/\1"|g' "$file"
    sed -i 's|#include "\.\./splash_app/\([^"]*\)"|#include "splash_app/\1"|g' "$file"
    
    # AI directory
    sed -i 's|#include "\.\./\.\./\.\./ai/\([^"]*\)"|#include "ai/\1"|g' "$file"
    sed -i 's|#include "\.\./\.\./ai/\([^"]*\)"|#include "ai/\1"|g' "$file"
    sed -i 's|#include "\.\./ai/\([^"]*\)"|#include "ai/\1"|g' "$file"
    
    # Parallel directory  
    sed -i 's|#include "\.\./\.\./\.\./parallel/\([^"]*\)"|#include "parallel/\1"|g' "$file"
    sed -i 's|#include "\.\./\.\./parallel/\([^"]*\)"|#include "parallel/\1"|g' "$file"
    sed -i 's|#include "\.\./parallel/\([^"]*\)"|#include "parallel/\1"|g' "$file"
    
    # Quantum directory
    sed -i 's|#include "\.\./\.\./\.\./quantum/\([^"]*\)"|#include "quantum/\1"|g' "$file"
    sed -i 's|#include "\.\./\.\./quantum/\([^"]*\)"|#include "quantum/\1"|g' "$file"
    sed -i 's|#include "\.\./quantum/\([^"]*\)"|#include "quantum/\1"|g' "$file"
    
    # Shell directory
    sed -i 's|#include "\.\./\.\./\.\./shell/\([^"]*\)"|#include "shell/\1"|g' "$file"
    sed -i 's|#include "\.\./\.\./shell/\([^"]*\)"|#include "shell/\1"|g' "$file"
    sed -i 's|#include "\.\./shell/\([^"]*\)"|#include "shell/\1"|g' "$file"
    
    # Security directory
    sed -i 's|#include "\.\./\.\./\.\./security/\([^"]*\)"|#include "security/\1"|g' "$file"
    sed -i 's|#include "\.\./\.\./security/\([^"]*\)"|#include "security/\1"|g' "$file"
    sed -i 's|#include "\.\./security/\([^"]*\)"|#include "security/\1"|g' "$file"
    
    # Special case: fix paths that reference kernel/kernel_types.h or ../../kernel/...
    sed -i 's|#include "\.\./\.\./\.\./kernel/\([^"]*\)"|#include "\1"|g' "$file"
    sed -i 's|#include "\.\./\.\./kernel/\([^"]*\)"|#include "\1"|g' "$file"
    sed -i 's|#include "\.\./kernel/\([^"]*\)"|#include "\1"|g' "$file"
    sed -i 's|#include "\.\.kernel/\([^"]*\)"|#include "\1"|g' "$file"
    
done

echo "Include statement updates complete!"
echo "Backup files created with .bak extension"
echo ""
echo "To verify changes, run: diff -r kernel kernel.bak"
echo "To remove backups after verification, run: find kernel -name '*.bak' -delete"

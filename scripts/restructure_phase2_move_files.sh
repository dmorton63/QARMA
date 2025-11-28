#!/bin/bash

# QARMA Restructure Script - Phase 2: Move Files
# This script moves files from old structure to new structure using git mv
# to preserve file history

set -e  # Exit on error

echo "=========================================="
echo "QARMA Restructure - Phase 2"
echo "Moving files to new structure"
echo "=========================================="
echo ""
echo "WARNING: This will move files using git mv."
echo "Press Ctrl+C to cancel, or Enter to continue..."
read

# Function to safely move files
safe_git_mv() {
    local src="$1"
    local dst="$2"
    
    if [ -e "$src" ]; then
        echo "  Moving: $src -> $dst"
        git mv "$src" "$dst" 2>/dev/null || {
            echo "  ⚠️  Warning: Could not git mv $src (may not exist or already moved)"
        }
    else
        echo "  ⏭️  Skipping: $src (doesn't exist)"
    fi
}

# Function to move directory contents
move_dir_contents() {
    local src_dir="$1"
    local dst_dir="$2"
    
    if [ -d "$src_dir" ] && [ "$(ls -A $src_dir 2>/dev/null)" ]; then
        echo "  Moving contents of $src_dir/ to $dst_dir/"
        for item in "$src_dir"/*; do
            if [ -e "$item" ]; then
                local basename=$(basename "$item")
                git mv "$item" "$dst_dir/$basename" 2>/dev/null || {
                    echo "    ⚠️  Warning: Could not move $item"
                }
            fi
        done
    else
        echo "  ⏭️  Skipping: $src_dir (empty or doesn't exist)"
    fi
}

echo ""
echo "=== SUBSYSTEM 1: KEYBOARD ==="
echo "Moving keyboard subsystem..."
move_dir_contents "headers/keyboard" "keyboard/headers"
move_dir_contents "kernel/keyboard" "keyboard/src"
echo "✓ Keyboard moved"
echo ""

echo "=== SUBSYSTEM 2: SHELL ==="
echo "Moving shell subsystem..."
move_dir_contents "headers/shell" "shell/headers"
move_dir_contents "kernel/shell" "shell/src"
echo "✓ Shell moved"
echo ""

echo "=== SUBSYSTEM 3: AI ==="
echo "Moving ai subsystem..."
move_dir_contents "headers/ai" "ai/headers"
move_dir_contents "kernel/ai" "ai/src"
echo "✓ AI moved"
echo ""

echo "=== SUBSYSTEM 4: QUANTUM ==="
echo "Moving quantum subsystem..."
move_dir_contents "headers/quantum" "quantum/headers"
move_dir_contents "kernel/quantum" "quantum/src"
echo "✓ Quantum moved"
echo ""

echo "=== SUBSYSTEM 5: SECURITY ==="
echo "Moving security subsystem..."
move_dir_contents "headers/security" "security/headers"
move_dir_contents "kernel/security" "security/src"
echo "✓ Security moved"
echo ""

echo "=== SUBSYSTEM 6: PARALLEL ==="
echo "Moving parallel subsystem..."
move_dir_contents "headers/parallel" "parallel/headers"
move_dir_contents "kernel/parallel" "parallel/src"
echo "✓ Parallel moved"
echo ""

echo "=== SUBSYSTEM 7: IDE ==="
echo "Moving ide subsystem..."
move_dir_contents "headers/ide" "ide/headers"
move_dir_contents "kernel/ide" "ide/src"
echo "✓ IDE moved"
echo ""

echo "=== SUBSYSTEM 8: SPLASH_APP ==="
echo "Moving splash_app subsystem..."
move_dir_contents "headers/splash_app" "splash_app/headers"
move_dir_contents "kernel/splash_app" "splash_app/src"
echo "✓ Splash App moved"
echo ""

echo "=== SUBSYSTEM 9: NETWORK ==="
echo "Moving network subsystem..."
move_dir_contents "headers/network" "network/headers"
move_dir_contents "kernel/network" "network/src"
echo "✓ Network moved"
echo ""

echo "=== SUBSYSTEM 10: FILESYSTEM ==="
echo "Moving filesystem subsystem..."
move_dir_contents "headers/fs" "fs/headers"
move_dir_contents "kernel/fs" "fs/src"
echo "✓ Filesystem moved"
echo ""

echo "=== SUBSYSTEM 11: GRAPHICS ==="
echo "Moving graphics subsystem..."
move_dir_contents "headers/graphics" "graphics/headers"
move_dir_contents "kernel/graphics" "graphics/src"
echo "✓ Graphics moved"
echo ""

echo "=== SUBSYSTEM 12: GUI ==="
echo "Moving gui subsystem..."
move_dir_contents "headers/gui" "gui/headers"
move_dir_contents "kernel/gui" "gui/src"
echo "✓ GUI moved"
echo ""

echo "=== SUBSYSTEM 13: WINDOW_MANAGER (qarma_win_handle) ==="
echo "Moving window manager subsystem..."
move_dir_contents "headers/qarma_win_handle" "window_manager/headers"
move_dir_contents "kernel/qarma_win_handle" "window_manager/src"
echo "✓ Window Manager moved"
echo ""

echo "=== SUBSYSTEM 14: DRIVERS ==="
echo "Moving drivers subsystem..."
move_dir_contents "headers/drivers" "drivers/headers"
move_dir_contents "kernel/drivers" "drivers/src"
echo "✓ Drivers moved"
echo ""

echo "=== SUBSYSTEM 15: CORE ==="
echo "Moving core subsystem..."
move_dir_contents "headers/core" "core/headers"
move_dir_contents "kernel/core" "core/src"
echo "✓ Core moved"
echo ""

echo "=== ROOT-LEVEL HEADERS ==="
echo "Moving root-level header files..."
# These are standalone headers that don't belong to a specific subsystem
safe_git_mv "headers/config.h" "core/headers/config.h"
safe_git_mv "headers/kernel_types.h" "kernel/headers/kernel_types.h"
safe_git_mv "headers/assert.h" "core/headers/assert.h"
safe_git_mv "headers/nomain_syntax.h" "ide/headers/nomain_syntax.h"
safe_git_mv "headers/splash_data.h" "splash_app/headers/splash_data.h"
echo "✓ Root headers moved"
echo ""

echo "=== LINKER SCRIPTS ==="
echo "Moving linker scripts to kernel/src..."
safe_git_mv "kernel/linker.ld" "kernel/src/linker.ld"
safe_git_mv "kernel/linker32.ld" "kernel/src/linker32.ld"
safe_git_mv "kernel/linker64.ld" "kernel/src/linker64.ld"
echo "✓ Linker scripts moved"
echo ""

echo "=========================================="
echo "✓ Phase 2 Complete!"
echo "=========================================="
echo ""
echo "Files moved to new structure."
echo ""
echo "Next steps:"
echo "1. Check for any remaining files: ls -R headers/ kernel/"
echo "2. Commit these changes: git commit -m 'Phase 2: Restructure - moved files'"
echo "3. Run Phase 3: Update Makefile"
echo "4. Run Phase 4: Update includes"

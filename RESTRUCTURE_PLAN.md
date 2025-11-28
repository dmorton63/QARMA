# QARMA Codebase Restructure Plan

## Goal
Reorganize from flat `headers/` and `kernel/` structure to component-based structure where each subsystem has its own `headers/` and `src/` folders.

## New Structure

```
/qarma/
  ├── Makefile (updated)
  ├── build/
  ├── docs/
  ├── assets/
  ├── scripts/
  ├── shared_files/
  │
  ├── kernel/              # Core kernel functionality
  │   ├── headers/
  │   │   ├── kernel.h
  │   │   ├── interrupts.h
  │   │   ├── gdt.h
  │   │   ├── idt.h
  │   │   └── ...
  │   └── src/
  │       ├── kernel.c
  │       ├── interrupts.c
  │       ├── boot_stub.c
  │       └── ...
  │
  ├── core/                # Core OS services (separate from kernel)
  │   ├── headers/
  │   │   ├── init.h
  │   │   ├── panic.h
  │   │   ├── string.h
  │   │   ├── io.h
  │   │   ├── pci.h
  │   │   ├── timer.h
  │   │   ├── sleep.h
  │   │   ├── scheduler/
  │   │   ├── memory/
  │   │   ├── input/
  │   │   └── ...
  │   └── src/
  │       ├── init.c
  │       ├── panic.c
  │       ├── string.c
  │       ├── scheduler/
  │       ├── memory/
  │       ├── input/
  │       └── ...
  │
  ├── drivers/             # Device drivers
  │   ├── headers/
  │   │   ├── usb/
  │   │   ├── net/
  │   │   ├── block/
  │   │   └── virtio_9p.h
  │   └── src/
  │       ├── usb/
  │       ├── net/
  │       ├── block/
  │       └── virtio_9p.c
  │
  ├── fs/                  # Filesystem
  │   ├── headers/
  │   │   ├── vfs.h
  │   │   ├── fat16.h
  │   │   ├── iso9660.h
  │   │   └── file_subsystem/
  │   └── src/
  │       ├── vfs.c
  │       ├── fat16.c
  │       └── file_subsystem/
  │
  ├── graphics/            # Graphics subsystem
  │   ├── headers/
  │   │   ├── graphics.h
  │   │   ├── framebuffer.h
  │   │   ├── compositor.h
  │   │   ├── cursor_loader.h
  │   │   └── subsystem/
  │   └── src/
  │       ├── graphics.c
  │       ├── framebuffer.c
  │       ├── cursor_loader.c
  │       └── subsystem/
  │
  ├── gui/                 # GUI components
  │   ├── headers/
  │   │   ├── controls/
  │   │   ├── desktop_toolbar.h
  │   │   └── ...
  │   └── src/
  │       ├── controls/
  │       └── ...
  │
  ├── window_manager/      # Window management (renamed from qarma_win_handle)
  │   ├── headers/
  │   │   ├── window_compositor.h
  │   │   ├── qarma_window_manager.h
  │   │   ├── qarma_input_events.h
  │   │   └── ...
  │   └── src/
  │       ├── window_compositor.c
  │       ├── qarma_window_manager.c
  │       └── ...
  │
  ├── keyboard/            # Keyboard subsystem
  │   ├── headers/
  │   │   ├── keyboard.h
  │   │   └── command.h
  │   └── src/
  │       ├── keyboard.c
  │       └── command.c
  │
  ├── network/             # Networking stack
  │   ├── headers/
  │   └── src/
  │
  ├── shell/               # Shell
  │   ├── headers/
  │   └── src/
  │
  ├── ai/                  # AI subsystem
  │   ├── headers/
  │   └── src/
  │
  ├── quantum/             # Quantum subsystem
  │   ├── headers/
  │   └── src/
  │
  ├── security/            # Security subsystem
  │   ├── headers/
  │   └── src/
  │
  ├── splash_app/          # Splash application
  │   ├── headers/
  │   └── src/
  │
  ├── parallel/            # Parallel execution
  │   ├── headers/
  │   └── src/
  │
  └── ide/                 # IDE
      ├── headers/
      └── src/
```

## Include Path Changes

**Before:**
```c
#include "core/string.h"              // From headers/core/string.h
#include "graphics/graphics.h"        // From headers/graphics/graphics.h
#include "drivers/usb/usb.h"          // From headers/drivers/usb/usb.h
```

**After:**
```c
#include "core/headers/string.h"      // From core/headers/string.h
#include "graphics/headers/graphics.h" // From graphics/headers/graphics.h
#include "drivers/headers/usb/usb.h"  // From drivers/headers/usb/usb.h
```

**Or even better with proper Makefile -I flags:**
```c
#include "string.h"                   // -Icore/headers
#include "graphics.h"                 // -Igraphics/headers
#include "usb/usb.h"                  // -Idrivers/headers
```

## Migration Steps

### Phase 1: Create new directory structure (no files moved yet)
```bash
mkdir -p kernel/{headers,src}
mkdir -p core/{headers,src}
mkdir -p drivers/{headers,src}
mkdir -p fs/{headers,src}
mkdir -p graphics/{headers,src}
mkdir -p gui/{headers,src}
mkdir -p window_manager/{headers,src}
mkdir -p keyboard/{headers,src}
mkdir -p network/{headers,src}
mkdir -p shell/{headers,src}
mkdir -p ai/{headers,src}
mkdir -p quantum/{headers,src}
mkdir -p security/{headers,src}
mkdir -p splash_app/{headers,src}
mkdir -p parallel/{headers,src}
mkdir -p ide/{headers,src}
```

### Phase 2: Move files (using git mv to preserve history)
```bash
# Example for core:
git mv headers/core/* core/headers/
git mv kernel/core/* core/src/

# Repeat for each subsystem...
```

### Phase 3: Update Makefile
- Update include paths: `-Iheaders` → `-Icore/headers -Igraphics/headers ...`
- Update source compilation paths
- Update linker script paths

### Phase 4: Update all #include statements
- Run script to update all includes across codebase
- Test compilation after each subsystem

### Phase 5: Clean up
- Remove old empty `headers/` and `kernel/` directories
- Update documentation
- Commit and push

## Benefits
1. ✅ Clearer separation of concerns
2. ✅ Easier to find related files (headers + source together)
3. ✅ Simpler include paths (no more deep nesting confusion)
4. ✅ Better for modular development
5. ✅ Easier to extract subsystems if needed
6. ✅ Standard industry practice

## Risks
1. ⚠️ Large number of file moves
2. ⚠️ All includes need updating
3. ⚠️ Makefile complexity increases initially
4. ⚠️ Potential for breaking build during transition

## Mitigation
- Do it in phases with git commits between each phase
- Test build after each subsystem migration
- Keep old structure until new one fully works
- Create rollback plan

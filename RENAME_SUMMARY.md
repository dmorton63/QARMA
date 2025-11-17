# Project Rename: QuantumOS → QARMA

## Completed: November 17, 2025

### Summary
Successfully renamed the project from "QuantumOS" to "QARMA" (pronounced like Karma) to avoid confusion with existing quantum computing focused projects.

### Changes Made

#### 1. Directory Structure
- **Old**: `/home/dmort/quantum_os`
- **New**: `/home/dmort/qarma`

#### 2. Build Artifacts
- **ISO filename**: `quantum_os.iso` → `qarma.iso`
- **Kernel ELF**: `quantum_os.elf` → `qarma.elf`
- **Makefile**: Updated all references and build targets
- **GRUB config**: Updated boot menu entries and kernel paths

#### 3. User-Facing Strings
- Boot menu entries: "QuantumOS (Silent Mode)" → "QARMA (Silent Mode)", etc.
- Login screen: "QuantumOS Login" → "QARMA Login"
- Desktop window: "QuantumOS Desktop" → "QARMA Desktop"
- Welcome message: "Welcome to QuantumOS" → "Welcome to QARMA"
- Version string: "QuantumOS v1.0.0-alpha" → "QARMA v1.0.0-alpha"

#### 4. Source Code & Documentation
- Updated ~150+ source files (.c, .h)
- Updated all documentation (.md files)
- Updated configuration files (.cfg, .template)
- Updated shell scripts (.sh)
- Updated file headers and comments

### Search & Replace Applied
- `QuantumOS` → `QARMA`
- `quantum_os` → `qarma`
- `QUANTUM_OS` → `QARMA`
- `/boot/quantum_os.elf` → `/boot/qarma.elf`

### Build Verification
✅ Clean build successful
✅ ISO created: `build/qarma.iso` (6.9 MB, 3482 sectors)
✅ All branding updated consistently
✅ No compilation errors

### Files Modified
- Makefile
- config/grub.cfg
- build_config/grub.cfg.template
- kernel/core/kernel.c
- kernel/qarma_win_handle/login_screen.c
- kernel/qarma_win_handle/main_window.c
- kernel/keyboard/command.c
- All documentation in docs/
- All source headers
- README.md
- And 100+ other files with references

### Note
The internal "quantum" kernel module directory name remains unchanged as it refers to the quantum computing subsystem, not the OS name.

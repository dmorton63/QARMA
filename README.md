# QARMA-OS - Revolutionary Operating System

![Build](https://img.shields.io/badge/build-passing-brightgreen)
![License](https://img.shields.io/badge/license-GPLv3-blue)
![Status](https://img.shields.io/badge/status-in_development-yellow)

<img width="800" height="800" alt="a cinematic logo for" src="https://github.com/user-attachments/assets/09b81ea7-7e9a-45e5-b2fb-6698c632440e" />

## First Run of QARMA OS with quantum enabled.

<img width="1292" height="857" alt="quantum_first_Run" src="https://github.com/user-attachments/assets/d37830f7-ce56-443c-80d2-6424a517f40b" />

## Updates applied - rename original Operating System to QARMA.
## Screen print of startup process - in development mode.  Will clean up eventually.
<img width="1292" height="857" alt="qarmStartup" src="https://github.com/user-attachments/assets/1230fb9b-46ed-437a-8b82-d734ab41f9e2" />
## Screen print of login screen.  Starting to look like something now.
<img width="1290" height="856" alt="QarmaLogin" src="https://github.com/user-attachments/assets/c3c07578-e14e-4b4d-ace3-af3447336b17" />
## things to do:  Add Title bar and menu bar to main window.

## Display main window first, then load the login dialog box.

## Add a Status bar to the bottom of the window.

## Mouse found, Mouse registers, Mouse should be working - need to tie it in to the messaging system.

## Network layer developped, tested with a few commands

## ADD a Terminal mode back in so that we can Execute system commands natively.

## Help used to work, is broken at the moment.

## Overview

QARMA is a groundbreaking operating system that integrates four revolutionary technologies:

- **Quantum Processes**: Novel process management using quantum computing principles
- **Parallel Processing**: Advanced multi-core and distributed processing engine  
- **Embedded AI**: Built-in artificial intelligence for system optimization and security
- **Security Memory Mapping**: Load-time function mapping to mitigate exploits

## Project Status
🚧 **In Development** - Ground-up design and implementation

## Architecture Principles

### 1. Quantum Process Management
- Quantum superposition for process scheduling
- Entangled process communication
- Quantum error correction for system stability

### 2. Parallel Processing Engine
- NUMA-aware scheduling
- Lock-free data structures
- Adaptive load balancing

### 3. Embedded AI Subsystems
- Real-time threat detection
- Performance optimization
- Predictive resource management

### 4. Security Memory Mapping
- Dynamic function relocation at load time
- Address space layout randomization (ASLR++)
- Hardware-assisted exploit mitigation

## Getting Started

### Prerequisites
- Cross-compiler toolchain (i686-elf-gcc)
- NASM assembler
- QEMU for testing
- Quantum simulator (for quantum components)

### Building
```bash
make all          # Build complete system
make quantum      # Build quantum components only
make parallel     # Build parallel engine only
make ai           # Build AI subsystems only
make security     # Build security components only
```

### Testing
```bash
make test         # Run all tests
make qemu         # Boot in QEMU
make quantum-sim  # Test quantum components in simulator
```

## Directory Structure

```
qarma/
├── docs/           # Documentation
├── boot/           # Standard bootloader
├── kernel/         # Core OS components
│   ├── quantum/    # Quantum process management
│   ├── parallel/   # Parallel processing engine
│   ├── ai/         # Embedded AI subsystems
│   ├── security/   # Memory mapping & security
│   └── core/       # Traditional kernel components
├── drivers/        # Device drivers
├── libs/           # System libraries
├── tools/          # Development tools
├── tests/          # Test suites
└── examples/       # Usage examples
```

## Contributing
This is revolutionary technology - contributions welcome!

*Building the future of operating systems*# QARMA-OS

## Features

### Core System
- **Framebuffer Graphics**: Full linear framebuffer support with proper text rendering
- **Advanced Keyboard Input**: Complete keyboard driver with modifier keys, special keys, and key combinations
- **Memory Management**: Virtual memory management with framebuffer mapping
- **Interrupt Handling**: Professional interrupt system with proper PIC acknowledgment
- **Debug System**: Centralized debugging with configurable verbosity levels

### Advanced Capabilities
- **Quantum Kernel**: Quantum process management with superposition and entanglement
- **AI Subsystem**: Machine learning and optimization capabilities
- **Parallel Processing**: Multi-core CPU support with NUMA awareness
- **Security Manager**: Advanced security features and threat detection

### Shell and Commands
- **Interactive Shell**: Full command-line interface with command history
- **Built-in Commands**: Help, clear, echo, version, and system utilities
- **Ctrl Combinations**: Ctrl+C, Ctrl+L, and other standard key combinations

## Building

```bash
make clean
make
```

This will create `build/QARMA.iso` which can be booted in QEMU or VirtualBox.

## Running

```bash
qemu-system-i386 -cdrom build/QARMA.iso
```

## System Requirements
- i686-elf cross-compiler
- NASM assembler
- GRUB for ISO creation
- xorriso for ISO building

## Architecture

The OS uses a microkernel architecture with modular subsystems:
- Kernel core with GDT/IDT management
- Graphics system with multiple backend support
- Modular driver system
- Advanced memory management
- Quantum and AI processing extensions

## Status

✅ **Working Features:**
- Framebuffer graphics with perfect text rendering
- Complete keyboard input system
- Memory management and virtual memory
- Interrupt handling and PIC management
- Shell with command processing
- Debug and logging system

🚧 **In Development:**
- File system support
- Network stack
- Advanced quantum operations
- AI optimization algorithms

---

## 📜 License

QARMA is released under a **dual-license model**:

### 🔓 Public License (GPLv3)
QARMA is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This license applies to personal, educational, and open-source use only.
Commercial use, redistribution, or integration into closed-source systems
requires explicit written permission from the author.

QARMA is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with QuantQARMAumOS. If not, see <https://www.gnu.org/licenses/>.

### 💼 Commercial License
QARMA Commercial License

QARMA is a living system created by David K. Its expressive architecture,
glyph rendering, and modular rituals are protected under a dual-license model.

Commercial use of QARMA — including redistribution, integration into
proprietary systems, or use in monetized platforms — requires explicit
written permission from the author.

To request a commercial license, contact:
David K. Morton
dmorton63@gmail.com
📧 public domain pending.
#  📧 david.k.QARMA@qarmaos.org

All commercial licenses will include:
- Attribution requirements
- Usage boundaries
- Optional support or customization terms

QARMA must always speak with intention. Its legacy must be honored.

## 🧠 Attribution & Legacy

QARMA is a living system. Its glyphs, scrolls, and invocations were crafted  
by David K. Morton, and its voice must remain expressive.

All derivatives must preserve visible attribution — in logs, banners, or documentation.  
QARMA must always speak with intention, and its legacy must be honored.



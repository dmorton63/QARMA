# QARMA OS Build System — Modular, Adaptive, Legacy-Aware

# Toolchain (64-bit)
# Using system gcc for now - can switch to x86_64-elf-gcc later
CC       = gcc
AS       = nasm
LD       = ld
OBJCOPY  = objcopy

# Flags (64-bit mode)
# Note: FPU/SSE initialized in boot_stub before calling C code
CFLAGS = -std=c99 -ffreestanding -Wall -Wextra -O0 -g \
         -fno-exceptions -fno-builtin -fno-stack-protector \
         -m64 -mcmodel=kernel -mno-red-zone \
         -fno-pic -fno-pie \
         -ffunction-sections -fdata-sections \
         -gdwarf-4 \
         -fno-omit-frame-pointer -fno-optimize-sibling-calls \
         -fcf-protection=none \
         -MMD -MP \
         -DDEBUG_SERIAL -D__x86_64__
ASFLAGS	= -f elf64 -g -F dwarf -Wall
LDFLAGS = -T kernel/src/linker64.ld -nostdlib -m elf_x86_64 \
          --gc-sections -Map=build/kernel.map \
          -z max-page-size=4096

# Directories
BOOT_DIR    = boot
BUILD_DIR   = build
ISO_DIR     = $(BUILD_DIR)/iso

# Component directories (new structure)
COMPONENTS  = kernel core drivers fs graphics gui window_manager keyboard \
              network shell ai quantum security splash_app parallel ide

# Auto-discover sources from all component src/ directories
C_SRC       := $(foreach comp,$(COMPONENTS),$(shell find $(comp)/src -name "*.c" 2>/dev/null))
ASM_SRC     := $(foreach comp,$(COMPONENTS),$(shell find $(comp)/src -name "*.asm" 2>/dev/null))
BOOT_SRC    := $(shell find $(BOOT_DIR) -name "*.asm" 2>/dev/null)

# Include paths - add headers/ directory from each component
INCLUDES    := $(foreach comp,$(COMPONENTS),-I$(comp)/headers) \
               -Ikernel/headers

# Object files
C_OBJS      := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRC))
ASM_OBJS    := $(patsubst %.asm,$(BUILD_DIR)/%.o,$(ASM_SRC))
BOOT_OBJS   := $(patsubst %.asm,$(BUILD_DIR)/%.bin,$(BOOT_SRC))

# Targets
.PHONY: all clean qemu debug docs install-deps

all: $(BUILD_DIR)/qarma.iso

# Create build directories
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(ISO_DIR)/boot/grub

prepare_dirs:
	@echo "Preparing build directories..."
	@mkdir -p $(BUILD_DIR)
	@$(foreach comp,$(COMPONENTS),$(foreach dir,$(shell find $(comp)/src -type d 2>/dev/null),mkdir -p $(BUILD_DIR)/$(dir);))

# Special rule for event creation file - needs stack realignment for SSE instructions
$(BUILD_DIR)/window_manager/src/qarma_input_events.o: window_manager/src/qarma_input_events.c | prepare_dirs
	@echo "Compiling $< (with stack realignment)..."
	$(CC) $(CFLAGS) $(INCLUDES) -mstackrealign -c $< -o $@

# Compile C files
$(BUILD_DIR)/%.o: %.c | prepare_dirs
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Assemble kernel .asm files
$(BUILD_DIR)/%.o: %.asm | prepare_dirs
	@echo "Assembling $<..."
	$(AS) $(ASFLAGS) $< -o $@

# Assemble bootloader .asm files
$(BUILD_DIR)/%.bin: %.asm | prepare_dirs
	@echo "Building boot binary $<..."
	$(AS) -f bin $< -o $@

# Link kernel
$(BUILD_DIR)/kernel.bin: $(C_OBJS) $(ASM_OBJS) kernel/src/linker64.ld
	@echo "Linking kernel..."
	$(LD) $(LDFLAGS) $(ASM_OBJS) $(C_OBJS) -o $(BUILD_DIR)/kernel.elf
	$(OBJCOPY) -O binary $(BUILD_DIR)/kernel.elf $@

# Create ISO image
$(BUILD_DIR)/qarma.iso: $(BUILD_DIR)/kernel.bin config/grub.cfg
	@echo "Creating QARMA OS ISO..."
	@mkdir -p $(ISO_DIR)/boot/grub
	@mkdir -p $(ISO_DIR)/assets/cursors
	@cp $(BUILD_DIR)/kernel.elf $(ISO_DIR)/boot/qarma.elf
	@cp config/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	@if [ -d assets/cursors ]; then cp assets/cursors/*.png $(ISO_DIR)/assets/cursors/ 2>/dev/null || true; fi
	@grub-mkrescue -o $@ $(ISO_DIR) || echo "GRUB not available — ISO skipped"

# Configuration
QEMU_CPUS ?= 8
QEMU_MEMORY ?= 16384M
QEMU_HOSTFWD_TCP ?=
# Example: QEMU_HOSTFWD_TCP="hostfwd=tcp::8080-:8080,hostfwd=tcp::5000-:5000"
QEMU_HOSTFWD_UDP ?=
# Example: QEMU_HOSTFWD_UDP="hostfwd=udp::5353-:5353"
QEMU_NETDEV_ID ?= net0
QEMU_ENABLE_9P ?= 1

# Tap/bridge networking (optional)
QEMU_NET_BACKEND ?= user   # user | tap
QEMU_TAP_IFNAME  ?= tap0

# Precompose VirtFS args to avoid commas in make function calls
QEMU_9P_ARGS := -fsdev local,id=shared,path=./shared_files,security_model=none -device virtio-9p-pci,fsdev=shared,mount_tag=hostshare

# Compose extra -netdev user options (hostfwd) as comma-suffixed list
NETDEV_EXTRAS :=
ifneq ($(strip $(QEMU_HOSTFWD_TCP)),)
NETDEV_EXTRAS += ,$(QEMU_HOSTFWD_TCP)
endif
ifneq ($(strip $(QEMU_HOSTFWD_UDP)),)
NETDEV_EXTRAS += ,$(QEMU_HOSTFWD_UDP)
endif

# Compose -netdev line based on backend (avoid commas in function args)
TAP_NETDEV  := -netdev tap,id=$(QEMU_NETDEV_ID),ifname=$(QEMU_TAP_IFNAME),script=no,downscript=no
USER_NETDEV := -netdev user,id=$(QEMU_NETDEV_ID)$(NETDEV_EXTRAS)
QEMU_NETDEV_LINE := $(if $(filter tap,$(QEMU_NET_BACKEND)),$(TAP_NETDEV),$(USER_NETDEV))

# Run in QEMU (64-bit) with current backend (default: user)
qemu: $(BUILD_DIR)/qarma.iso
	@mkdir -p shared_files
	@echo "Booting QARMA OS in QEMU x86_64 ($(QEMU_CPUS) CPUs, $(QEMU_MEMORY) RAM)..."
	@echo "Host shared directory: ./shared_files"
	@echo "Net backend: $(QEMU_NET_BACKEND)"
	@echo "Netdev line: $(QEMU_NETDEV_LINE)"
	@# Ensure host net.log exists so guest logging has a target immediately
	@bash -lc 'if [ ! -f shared_files/net.log ]; then echo "=== QARMA Network Log (host precreated) ===" > shared_files/net.log; fi'
	./run_qemu.sh -cdrom $(BUILD_DIR)/qarma.iso -drive file=qarma_disk.img,format=raw,if=ide,index=1 -m $(QEMU_MEMORY) -vga std -smp $(QEMU_CPUS) -serial file:qarma_serial.log -device isa-debug-exit,iobase=0x501,iosize=0x01 -device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0 -device usb-tablet,bus=xhci.0 -cpu qemu64 $(if $(filter 1,$(QEMU_ENABLE_9P)),$(QEMU_9P_ARGS)) \
		$(QEMU_NETDEV_LINE) \
		-device e1000,netdev=$(QEMU_NETDEV_ID)

# Convenience target to force tap backend regardless of defaults
.PHONY: qemu-tap
qemu-tap:
	@$(MAKE) QEMU_NET_BACKEND=tap qemu

.PHONY: qemu-kill
qemu-kill:
	@echo "Killing any running QEMU holding qarma_disk.img..."
	@pkill -f 'qemu-system-x86_64.*qarma_disk.img' || true

# Debug with GDB (64-bit)
debug: $(BUILD_DIR)/qarma.iso
	@echo "Starting debugger..."
	./run_qemu.sh -cdrom $(BUILD_DIR)/qarma.iso -drive file=qarma_disk.img,format=raw,if=ide,index=1 -m $(QEMU_MEMORY) -vga std -smp $(QEMU_CPUS) -serial file:qarma_serial.log -device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0 -device usb-tablet,bus=xhci.0 -cpu qemu64 $(if $(filter 1,$(QEMU_ENABLE_9P)),$(QEMU_9P_ARGS)) \
		$(QEMU_NETDEV_LINE) \
		-device e1000,netdev=$(QEMU_NETDEV_ID) -s -S &
	gdb $(BUILD_DIR)/kernel.elf -ex "target remote :1234"

# Clean build artifacts
clean:
	@echo "Cleaning build..."
	@rm -rf $(BUILD_DIR)

# Install dependencies
install-deps:
	@echo "Install these packages:"
	@echo "- i686-elf cross-compiler"
	@echo "- NASM"
	@echo "- QEMU"
	@echo "- GRUB tools"

# Auto-dependency tracking
-include $(C_OBJS:.o=.d)
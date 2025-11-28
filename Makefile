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
LDFLAGS = -T kernel/linker64.ld -nostdlib -m elf_x86_64 \
          --gc-sections -Map=build/kernel.map \
          -z max-page-size=4096

# Directories
SRC_DIR     = kernel
BOOT_DIR    = boot
BUILD_DIR   = build
ISO_DIR     = $(BUILD_DIR)/iso
QUANTUM_DIR = kernel/quantum
#QUANTUM_OBJ = $(BUILD_DIR)/$(QUANTUM_DIR)/quantum.o

# Auto-discover sources
C_SRC       := $(shell find $(SRC_DIR) -name "*.c")
ASM_SRC     := $(shell find $(SRC_DIR) -name "*.asm")
BOOT_SRC    := $(shell find $(BOOT_DIR) -name "*.asm")

# Unified headers directory
INCLUDES    := -Iheaders $(foreach dir,$(shell find headers -type d),-I$(dir))

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
	@$(foreach dir,$(shell find $(SRC_DIR) -type d),mkdir -p $(BUILD_DIR)/$(dir);)

# Special rule for event creation file - needs stack realignment for SSE instructions
$(BUILD_DIR)/kernel/qarma_win_handle/qarma_input_events.o: kernel/qarma_win_handle/qarma_input_events.c | prepare_dirs
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

$(QUANTUM_OBJ):
	@echo "[quantum] Building quantum.o..."
	@$(MAKE) -C $(QUANTUM_DIR)q

# Link kernel
$(BUILD_DIR)/kernel.bin: $(C_OBJS) $(ASM_OBJS) $(QUANTUM_OBJ) $(SRC_DIR)/linker64.ld
	@echo "Linking kernel..."
	$(LD) $(LDFLAGS) $(ASM_OBJS) $(C_OBJS) $(QUANTUM_OBJ) -o $(BUILD_DIR)/kernel.elf
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

# Run in QEMU (64-bit)
qemu: $(BUILD_DIR)/qarma.iso
	@mkdir -p shared_files
	@echo "Booting QARMA OS in QEMU x86_64 ($(QEMU_CPUS) CPUs, $(QEMU_MEMORY) RAM)..."
	@echo "Host shared directory: ./shared_files"
	qemu-system-x86_64 -cdrom $(BUILD_DIR)/qarma.iso -drive file=qarma_disk.img,format=raw,if=ide,index=1 -m $(QEMU_MEMORY) -vga std -smp $(QEMU_CPUS) -serial file:qarma_serial.log -device isa-debug-exit,iobase=0x501,iosize=0x01 -device qemu-xhci,id=xhci -device usb-kbd,bus=xhci.0 -device usb-tablet,bus=xhci.0 -cpu qemu64 -fsdev local,id=shared,path=./shared_files,security_model=none -device virtio-9p-pci,fsdev=shared,mount_tag=hostshare

# Debug with GDB (64-bit)
debug: $(BUILD_DIR)/qarma.iso
	@echo "Starting debugger..."
	qemu-system-x86_64 -drive file=$<,format=raw,media=cdrom,if=ide -m 4096M -vga std -smp $(QEMU_CPUS) -s -S -cpu qemu64 &
	gdb $(BUILD_DIR)/kernel.elf -ex "target remote :1234"

# Clean build artifacts
clean:
	@echo "Cleaning build..."
	@rm -rf $(BUILD_DIR)
	@$(MAKE) -C $(QUANTUM_DIR) clean

# Install dependencies
install-deps:
	@echo "Install these packages:"
	@echo "- i686-elf cross-compiler"
	@echo "- NASM"
	@echo "- QEMU"
	@echo "- GRUB tools"

# Auto-dependency tracking
-include $(C_OBJS:.o=.d)
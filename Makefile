# QARMA OS Build System — Modular, Adaptive, Legacy-Aware

# Toolchain
CC       = i686-elf-gcc
AS       = nasm
LD       = i686-elf-ld
OBJCOPY  = i686-elf-objcopy

# Flags
CFLAGS   = -std=c99 -ffreestanding -O2 -Wall -Wextra -O0 -g \
           -fno-exceptions -fno-builtin -fno-stack-protector \
           -m32 -nostdlib -nostdinc -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
           -MMD -MP
ASFLAGS  = -g -f elf32 -Wall
LDFLAGS  = -T kernel/linker.ld -nostdlib -m elf_i386

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
	@$(MAKE) -C $(QUANTUM_DIR)

# Link kernel
$(BUILD_DIR)/kernel.bin: $(C_OBJS) $(ASM_OBJS) $(QUANTUM_OBJ) $(SRC_DIR)/linker.ld
	@echo "Linking kernel..."
	$(LD) $(LDFLAGS) $(ASM_OBJS) $(C_OBJS) $(QUANTUM_OBJ) -o $(BUILD_DIR)/kernel.elf
	$(OBJCOPY) -O binary $(BUILD_DIR)/kernel.elf $@

# Create ISO image
$(BUILD_DIR)/qarma.iso: $(BUILD_DIR)/kernel.bin config/grub.cfg $(ISO_DIR)/splash.png
	@echo "Creating QARMA OS ISO..."
	@mkdir -p $(ISO_DIR)/boot/grub
	@cp $(BUILD_DIR)/kernel.elf $(ISO_DIR)/boot/qarma.elf
	@cp config/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	@grub-mkrescue -o $@ $(ISO_DIR) || echo "GRUB not available — ISO skipped"

# Configuration
QEMU_CPUS ?= 8

# Run in QEMU
qemu: $(BUILD_DIR)/qarma.iso
	@echo "Booting QARMA OS in QEMU ($(QEMU_CPUS) CPUs)..."
	qemu-system-i386 -drive file=$<,format=raw,media=cdrom,if=ide -m 256M -vga std -smp $(QEMU_CPUS)

# Debug with GDB
debug: $(BUILD_DIR)/qarma.iso
	@echo "Starting debugger..."
	qemu-system-i386 -drive file=$<,format=raw,media=cdrom,if=ide -m 256M -vga std -smp $(QEMU_CPUS) -s -S &
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
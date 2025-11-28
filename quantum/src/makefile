# Toolchain
CC      := i686-elf-gcc
LD      := i686-elf-ld
AS      := nasm

# Paths
BUILD_DIR := ../../build/kernel/quantum

# Flags
INCLUDES := -I../../headers $(foreach dir,$(shell find ../../headers -type d),-I$(dir))
CFLAGS  := -ffreestanding -O2 -Wall -Wextra $(INCLUDES) -m32
ASFLAGS := -f elf
LDFLAGS := -m elf_i386

# Sources
SRC_C   := quantum_kernel.c quantum_register.c quantum_register_example.c quantum_ai_observer.c quantum_adaptive.c quantum_scheduler.c quantum_cross_learning.c
OBJ_C   := $(addprefix $(BUILD_DIR)/,$(SRC_C:.c=.o))
TARGET  := $(BUILD_DIR)/quantum.o

# Build rules
.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ_C)
	@echo "[quantum] Linking objects into $(TARGET)..."
	@mkdir -p $(BUILD_DIR)
	$(LD) $(LDFLAGS) -r -o $@ $^

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(BUILD_DIR)
	@echo "[quantum] Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo "[quantum] Cleaning..."
	rm -f $(BUILD_DIR)/*.o $(TARGET)
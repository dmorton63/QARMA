BUILD_MODE ?= release
CC = gcc
CFLAGS = -Wall -Wextra -O2 -I headers -I/usr/local/include 
ifeq ($(BUILD_MODE),debug)
    CFLAGS += -g -DDEBUG
endif
LDFLAGS = -lpthread -lEGL -lGLESv2 -L/usr/local/lib -lX11 -lm
debug: BUILD_MODE = debug
debug: clean all

# Directories
SRCDIR := src
BUILDDIR := build
HEADERDIR := headers
TARGET := $(BUILDDIR)/bin/qarma

# Find all source files recursively
SOURCES := $(shell find $(SRCDIR) -name '*.c')
# Create object file paths in build directory, preserving subdirectory structure
OBJECTS := $(SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)

# Default target
all: $(TARGET)

# Create target executable
$(TARGET): $(OBJECTS) | $(BUILDDIR)/bin
	@echo "Linking: $@"
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

# Compile source files to object files, preserving directory structure
$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	@mkdir -p $(dir $@)
	@echo "Compiling: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Create build directories
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/bin:
	mkdir -p $(BUILDDIR)/bin

# Clean build files
clean:
	@echo "Cleaning build files..."
	rm -rf $(BUILDDIR)
	rm -f *.o

# Run the program
run: $(TARGET)
	./$(TARGET)

# Text rendering demo
text_demo: examples/text_demo.c src/mesa_bootstrap.c src/log.c
	@echo "Building text demo..."
	$(CC) $(CFLAGS) -o build/text_demo $^ $(LDFLAGS)

# Run text demo
run_text_demo: text_demo
	./build/text_demo

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

.PHONY: all clean run debug
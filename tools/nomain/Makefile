# NOMAIN Language Implementation Makefile
# Standalone build system for the NOMAIN interpreter and toolchain

PYTHON := python3
SRC_DIR := src
TEST_DIR := tests
PROGRAMS_DIR := $(TEST_DIR)/programs
EXAMPLES_DIR := examples

# Python source files
SOURCES := $(wildcard $(SRC_DIR)/*.py)
TEST_FILES := $(wildcard $(TEST_DIR)/*.py)
PROGRAM_FILES := $(wildcard $(PROGRAMS_DIR)/*.nom)

# Virtual environment
VENV := .venv
VENV_ACTIVATE := $(VENV)/bin/activate

.PHONY: all clean test test-lexer run-examples lint format help venv install

# Default target
all: help

# Help target - show available commands
help:
	@echo "NOMAIN Language Makefile"
	@echo "========================"
	@echo ""
	@echo "Available targets:"
	@echo "  make venv         - Create Python virtual environment"
	@echo "  make install      - Install dependencies (if any)"
	@echo "  make test         - Run all tests"
	@echo "  make test-lexer   - Run lexer tests"
	@echo "  make test-parser  - Run parser tests"
	@echo "  make test-errors  - Run semantic error tests"
	@echo "  make parse-FILE   - Parse a specific program (e.g., make parse-hello)"
	@echo "  make ast-FILE     - Show detailed AST for a program (e.g., make ast-hello)"
	@echo "  make exec-FILE    - Execute a specific program (e.g., make exec-hello)"
	@echo "  make run-examples - Run all example programs (interpreter)"
	@echo "  make lint         - Run linting checks"
	@echo "  make format       - Format code with black"
	@echo "  make clean        - Remove generated files and cache"
	@echo "  make clean-all    - Remove everything including venv"
	@echo ""
	@echo "Current implementation status:"
	@echo "  ✅ Lexer/Tokenizer"
	@echo "  ✅ Parser"
	@echo "  ✅ AST nodes"
	@echo "  ✅ Interpreter"
	@echo "  ✅ Built-in functions (I/O)"
	@echo "  ✅ Quantum functions"
	@echo "  📋 Semantic analyzer (optional)"

# Create virtual environment
venv:
	@echo "Creating Python virtual environment..."
	$(PYTHON) -m venv $(VENV)
	@echo "Virtual environment created at $(VENV)"
	@echo "Activate with: source $(VENV_ACTIVATE)"

# Install dependencies
install:
	@if [ -d "$(VENV)" ]; then \
		. $(VENV_ACTIVATE) && pip install --upgrade pip; \
		echo "Dependencies installed"; \
	else \
		echo "Virtual environment not found. Run 'make venv' first."; \
		exit 1; \
	fi

# Run all tests
test: test-lexer test-parser test-errors
	@echo "All tests completed"

# Run lexer tests
test-lexer:
	@echo "Running lexer tests..."
	cd $(TEST_DIR) && $(PYTHON) test_lexer.py

# Run parser tests
test-parser:
	@echo "Running parser tests..."
	cd $(TEST_DIR) && $(PYTHON) test_parser.py

# Run semantic error tests
test-errors:
	@echo "Running semantic error tests..."
	cd $(TEST_DIR)/semantic_errors && $(PYTHON) test_errors.py

# Run all example programs through interpreter
run-examples:
	@echo "Running all test programs..."
	@for prog in $(PROGRAM_FILES); do \
		echo ""; \
		echo "===== Executing $$prog ====="; \
		$(PYTHON) $(SRC_DIR)/interpreter.py $$prog || echo "Failed to execute $$prog"; \
	done
	@echo ""
	@echo "All programs executed"

# Lint Python code
lint:
	@echo "Linting Python code..."
	@command -v pylint >/dev/null 2>&1 || { echo "pylint not found. Install with: pip install pylint"; exit 1; }
	pylint $(SRC_DIR)/*.py || true

# Format code with black
format:
	@echo "Formatting Python code..."
	@command -v black >/dev/null 2>&1 || { echo "black not found. Install with: pip install black"; exit 1; }
	black $(SRC_DIR)
	black $(TEST_DIR)

# Clean generated files
clean:
	@echo "Cleaning generated files..."
	find . -type d -name "__pycache__" -exec rm -rf {} + 2>/dev/null || true
	find . -type f -name "*.pyc" -delete
	find . -type f -name "*.pyo" -delete
	find . -type f -name "*.pyd" -delete
	find . -type f -name ".coverage" -delete
	find . -type d -name "*.egg-info" -exec rm -rf {} + 2>/dev/null || true
	find . -type d -name ".pytest_cache" -exec rm -rf {} + 2>/dev/null || true
	@echo "Clean complete"

# Clean everything including virtual environment
clean-all: clean
	@echo "Removing virtual environment..."
	rm -rf $(VENV)
	@echo "Full clean complete"

# Parse a specific program and show AST
parse-%:
	@prog="$(PROGRAMS_DIR)/$*.nom"; \
	if [ -f "$$prog" ]; then \
		echo "Parsing $$prog..."; \
		$(PYTHON) -c "import sys; sys.path.insert(0, 'src'); from parser import parse_file; from ast_nodes import print_ast; ast = parse_file('$$prog'); print('Parse successful!\n'); print_ast(ast)"; \
	else \
		echo "Program $$prog not found"; \
		exit 1; \
	fi

# Show detailed AST for a specific program
ast-%:
	@prog="$(PROGRAMS_DIR)/$*.nom"; \
	if [ -f "$$prog" ]; then \
		echo "Detailed AST for $$prog:"; \
		echo ""; \
		$(PYTHON) -c "import sys; sys.path.insert(0, 'src'); from parser import parse_file; from ast_printer import print_detailed_ast; ast = parse_file('$$prog'); print_detailed_ast(ast)"; \
	else \
		echo "Program $$prog not found"; \
		exit 1; \
	fi

# Execute a specific program through interpreter
exec-%:
	@prog="$(PROGRAMS_DIR)/$*.nom"; \
	if [ -f "$$prog" ]; then \
		$(PYTHON) $(SRC_DIR)/interpreter.py $$prog; \
	else \
		echo "Program $$prog not found"; \
		exit 1; \
	fi

# Quick test of a single program (lexer only)
run-%:
	@prog="$(PROGRAMS_DIR)/$*.nom"; \
	if [ -f "$$prog" ]; then \
		echo "Running $$prog..."; \
		$(PYTHON) $(SRC_DIR)/lexer.py < "$$prog"; \
	else \
		echo "Program $$prog not found"; \
		exit 1; \
	fi

# Show available test programs
list-programs:
	@echo "Available test programs:"
	@ls -1 $(PROGRAMS_DIR)/*.nom 2>/dev/null | xargs -n1 basename || echo "No programs found"

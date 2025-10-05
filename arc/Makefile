# © 2025 Casey Koons All rights reserved
# Arc CLI Makefile

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Werror -Wextra -std=c11 -g -I./include -I../include
LDFLAGS = -lpthread

# Installation
PREFIX ?= $(HOME)/.local

# Directories
SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
BUILD_DIR = build
PARENT_BUILD = ../build

# Argo core library
ARGO_CORE = $(PARENT_BUILD)/libargo_core.a

# Source files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

# Objects without main (for test linking)
TEST_OBJECTS_LIB = $(filter-out $(BUILD_DIR)/main.o,$(OBJECTS))

# Test files
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.c)
TEST_OBJECTS = $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/test_%.o,$(TEST_SOURCES))
TEST_TARGETS = $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/test_%,$(TEST_SOURCES))

# Main target
TARGET = arc

.PHONY: all clean test test-arc-unit test-arc-cli test-arc test-arc-full directories count install uninstall

# Default target
all: directories $(TARGET)

# Create build directories
directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(PARENT_BUILD)

# Build arc executable
$(TARGET): $(OBJECTS) $(ARGO_CORE)
	$(CC) $(OBJECTS) $(ARGO_CORE) -o $@ $(LDFLAGS)
	@echo "Built arc CLI -> ./arc"

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test files
$(BUILD_DIR)/test_%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build test executables (exclude main.o to avoid duplicate main)
$(BUILD_DIR)/test_%: $(BUILD_DIR)/test_%.o $(TEST_OBJECTS_LIB) $(ARGO_CORE)
	$(CC) $< $(TEST_OBJECTS_LIB) $(ARGO_CORE) -o $@ $(LDFLAGS)

# Unit tests (C tests)
test-arc-unit: $(TEST_TARGETS)
	@echo ""
	@echo "=========================================="
	@echo "Arc Unit Tests"
	@echo "=========================================="
	@for test in $(TEST_TARGETS); do \
		echo "Running $$test..."; \
		./$$test || exit 1; \
	done

# CLI tests (bash script)
test-arc-cli: $(TARGET)
	@echo ""
	@echo "=========================================="
	@echo "Arc CLI Tests"
	@echo "=========================================="
	@cd $(TEST_DIR) && ./test_arc_cli.sh

# Standard arc testing (unit + cli)
test-arc: test-arc-unit test-arc-cli
	@echo ""
	@echo "=========================================="
	@echo "All Arc Tests Passed"
	@echo "=========================================="
	@echo ""

# Backward compatibility (run standard tests)
test: test-arc

# Full arc testing (placeholder for workflow tests later)
test-arc-full: test-arc
	@echo "Full arc tests complete (workflow tests not yet implemented)"

# Count component size
count:
	@../scripts/count_component.sh arc --budget 5000

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# Installation targets
install:
	install -d $(PREFIX)/bin
	install -m 0755 $(TARGET) $(PREFIX)/bin/arc

uninstall:
	rm -f $(PREFIX)/bin/arc

# Debug
debug:
	@echo "CC: $(CC)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "SOURCES: $(SOURCES)"
	@echo "OBJECTS: $(OBJECTS)"
	@echo "TARGET: $(TARGET)"

# © 2025 Casey Koons All rights reserved
# Arc CLI Makefile

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Werror -Wextra -std=c11 -g -I./include -I../include
LDFLAGS = -lpthread -lcurl

# Installation
PREFIX ?= $(HOME)/.local

# Directories
SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
BUILD_DIR = build
PARENT_BUILD = ../build

# Argo libraries (arc needs all three: core + daemon + workflow)
ARGO_CORE = $(PARENT_BUILD)/libargo_core.a
ARGO_DAEMON = $(PARENT_BUILD)/libargo_daemon.a
ARGO_WORKFLOW = $(PARENT_BUILD)/libargo_workflow.a

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

.PHONY: all clean test test-arc-unit test-arc-cli test-arc test-arc-workflow test-arc-background test-arc-full test-arc-all directories count install uninstall

# Default target
all: directories $(TARGET)

# Create build directories
directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(PARENT_BUILD)

# Build arc executable (links daemon + workflow + core)
$(TARGET): $(OBJECTS) $(ARGO_CORE) $(ARGO_DAEMON) $(ARGO_WORKFLOW)
	$(CC) $(OBJECTS) $(ARGO_DAEMON) $(ARGO_WORKFLOW) $(ARGO_CORE) -o $@ $(LDFLAGS)
	@echo "Built arc CLI -> ./arc"

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test files
$(BUILD_DIR)/test_%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build test executables (exclude main.o to avoid duplicate main)
$(BUILD_DIR)/test_%: $(BUILD_DIR)/test_%.o $(TEST_OBJECTS_LIB) $(ARGO_CORE) $(ARGO_DAEMON) $(ARGO_WORKFLOW)
	$(CC) $< $(TEST_OBJECTS_LIB) $(ARGO_DAEMON) $(ARGO_WORKFLOW) $(ARGO_CORE) -o $@ $(LDFLAGS)

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

# Workflow integration tests (manual/nightly)
test-arc-workflow: $(TARGET)
	@echo ""
	@echo "=========================================="
	@echo "Arc Workflow Integration Tests"
	@echo "=========================================="
	@cd $(TEST_DIR) && ./test_arc_workflows.sh

# Background/attach tests (manual only)
test-arc-background: $(TARGET)
	@echo ""
	@echo "=========================================="
	@echo "Arc Background Process Tests"
	@echo "=========================================="
	@cd $(TEST_DIR) && ./test_arc_background.sh

# Full arc testing (unit + cli + workflow)
test-arc-full: test-arc test-arc-workflow
	@echo ""
	@echo "=========================================="
	@echo "Full Arc Tests Complete"
	@echo "=========================================="
	@echo ""

# All arc tests (including background - manual only)
test-arc-all: test-arc-full test-arc-background
	@echo ""
	@echo "=========================================="
	@echo "All Arc Tests Complete (Pyramid Built!)"
	@echo "=========================================="
	@echo ""

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

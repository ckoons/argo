# © 2025 Casey Koons All rights reserved
# CI Tool Makefile

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

# Argo libraries (ci needs core + daemon for HTTP client)
ARGO_CORE = $(PARENT_BUILD)/libargo_core.a
ARGO_DAEMON = $(PARENT_BUILD)/libargo_daemon.a

# Source files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

# Objects without main (for test linking)
TEST_OBJECTS_LIB = $(filter-out $(BUILD_DIR)/ci_main.o,$(OBJECTS))

# Test files
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.c)
TEST_OBJECTS = $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/test_%.o,$(TEST_SOURCES))
TEST_TARGETS = $(patsubst $(TEST_DIR)/%.c,bin/tests/test_%,$(TEST_SOURCES))

# Main target (build into ../bin/ when called from parent, bin/ when standalone)
# This ensures ci ALWAYS builds to the main project bin/ directory
TARGET = ../bin/ci

.PHONY: all clean test test-ci-unit test-ci-cli test-ci directories count install uninstall

# Default target
all: directories $(TARGET)

# Create build directories
directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(PARENT_BUILD)
	@mkdir -p ../bin/tests
	@mkdir -p bin/tests

# Build ci executable into ../bin/ (links daemon + core)
$(TARGET): $(OBJECTS) $(ARGO_CORE) $(ARGO_DAEMON)
	@mkdir -p ../bin
	$(CC) $(OBJECTS) $(ARGO_DAEMON) $(ARGO_CORE) -o $@ $(LDFLAGS)
	@echo "Built ci tool -> $@"

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test files
$(BUILD_DIR)/test_%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build test executables into bin/tests/ (exclude ci_main.o to avoid duplicate main)
bin/tests/test_%: $(BUILD_DIR)/test_%.o $(TEST_OBJECTS_LIB) $(ARGO_CORE) $(ARGO_DAEMON)
	@mkdir -p bin/tests
	$(CC) $< $(TEST_OBJECTS_LIB) $(ARGO_DAEMON) $(ARGO_CORE) -o $@ $(LDFLAGS)

# Unit tests (C tests)
test-ci-unit: $(TEST_TARGETS)
	@echo ""
	@echo "=========================================="
	@echo "CI Unit Tests"
	@echo "=========================================="
	@for test in $(TEST_TARGETS); do \
		echo "Running $$test..."; \
		./$$test || exit 1; \
	done

# Integration tests (bash script)
test-ci-integration: $(TARGET)
	@echo ""
	@echo "=========================================="
	@echo "CI Integration Tests"
	@echo "=========================================="
	@$(TEST_DIR)/test_ci_integration.sh

# CLI tests (backward compatibility)
test-ci-cli: test-ci-integration

# Standard ci testing (unit + cli)
test-ci: test-ci-unit test-ci-cli
	@echo ""
	@echo "=========================================="
	@echo "All CI Tests Passed"
	@echo "=========================================="
	@echo ""

# Backward compatibility (run standard tests)
test: test-ci

# Count component size
count:
	@../scripts/count_component.sh ci --budget 5000

# Clean build artifacts (only ci's local directories)
clean:
	rm -rf $(BUILD_DIR)
	rm -rf bin/tests

# Installation targets
install:
	install -d $(PREFIX)/bin
	install -m 0755 $(TARGET) $(PREFIX)/bin/ci

uninstall:
	rm -f $(PREFIX)/bin/ci

# Debug
debug:
	@echo "CC: $(CC)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "SOURCES: $(SOURCES)"
	@echo "OBJECTS: $(OBJECTS)"
	@echo "TARGET: $(TARGET)"

# © 2025 Casey Koons All rights reserved
# Argo-Term UI Makefile

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Werror -Wextra -std=c11 -g -I./include -I../../include -I../../arc/include
LDFLAGS = -lpthread -lncurses

# Installation
PREFIX ?= $(HOME)/.local

# Directories
SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
BUILD_DIR = build
PARENT_BUILD = ../../build

# Argo core library
ARGO_CORE = $(PARENT_BUILD)/libargo_core.a

# Source files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

# Test files
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.c)
TEST_OBJECTS = $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/test_%.o,$(TEST_SOURCES))
TEST_TARGETS = $(patsubst $(TEST_DIR)/%.c,bin/tests/test_%,$(TEST_SOURCES))

# Main target (build into bin/)
TARGET = bin/argo-term

.PHONY: all clean test directories count install uninstall

# Default target
all: directories $(TARGET)

# Create build directories
directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(PARENT_BUILD)
	@mkdir -p bin/tests

# Build argo-term executable into bin/
$(TARGET): $(OBJECTS) $(ARGO_CORE)
	@mkdir -p bin
	$(CC) $(OBJECTS) $(ARGO_CORE) -o $@ $(LDFLAGS)
	@echo "Built argo-term UI -> $@"

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test files
$(BUILD_DIR)/test_%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build test executables into bin/tests/
bin/tests/test_%: $(BUILD_DIR)/test_%.o $(OBJECTS) $(ARGO_CORE)
	@mkdir -p bin/tests
	$(CC) $< $(OBJECTS) $(ARGO_CORE) -o $@ $(LDFLAGS)

# Run tests
test: $(TEST_TARGETS)
	@echo ""
	@echo "=========================================="
	@echo "Argo-Term UI Tests"
	@echo "=========================================="
	@for test in $(TEST_TARGETS); do \
		echo "Running $$test..."; \
		./$$test || exit 1; \
	done
	@echo ""
	@echo "=========================================="
	@echo "All Argo-Term tests passed"
	@echo "=========================================="

# Count component size
count:
	@../../scripts/count_component.sh ui/argo-term --budget 3000

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR) bin

# Installation targets
install:
	install -d $(PREFIX)/bin
	install -m 0755 $(TARGET) $(PREFIX)/bin/argo-term

uninstall:
	rm -f $(PREFIX)/bin/argo-term

# Debug
debug:
	@echo "CC: $(CC)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "SOURCES: $(SOURCES)"
	@echo "OBJECTS: $(OBJECTS)"
	@echo "TARGET: $(TARGET)"

# © 2025 Casey Koons All rights reserved
# Argo-Term UI Makefile

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Werror -Wextra -std=c11 -g -I./include -I../../include -I../../arc/include
LDFLAGS = -lpthread -lncurses

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
TEST_TARGETS = $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/test_%,$(TEST_SOURCES))

# Main target
TARGET = $(PARENT_BUILD)/argo-term

.PHONY: all clean test directories count

# Default target
all: directories $(TARGET)

# Create build directories
directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(PARENT_BUILD)

# Build argo-term executable
$(TARGET): $(OBJECTS) $(ARGO_CORE)
	$(CC) $(OBJECTS) $(ARGO_CORE) -o $@ $(LDFLAGS)
	@echo "Built argo-term UI -> ../../build/argo-term"

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test files
$(BUILD_DIR)/test_%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build test executables
$(BUILD_DIR)/test_%: $(BUILD_DIR)/test_%.o $(OBJECTS) $(ARGO_CORE)
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
	rm -rf $(BUILD_DIR) $(TARGET)

# Debug
debug:
	@echo "CC: $(CC)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "SOURCES: $(SOURCES)"
	@echo "OBJECTS: $(OBJECTS)"
	@echo "TARGET: $(TARGET)"

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

# Argo libraries (arc needs core + daemon for HTTP client)
ARGO_CORE = $(PARENT_BUILD)/libargo_core.a
ARGO_DAEMON = $(PARENT_BUILD)/libargo_daemon.a

# Source files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

# Objects without main (for test linking)
TEST_OBJECTS_LIB = $(filter-out $(BUILD_DIR)/main.o,$(OBJECTS))

# Test files
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.c)
TEST_OBJECTS = $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/test_%.o,$(TEST_SOURCES))
TEST_TARGETS = $(patsubst $(TEST_DIR)/%.c,bin/tests/test_%,$(TEST_SOURCES))

# Main target (build into ../bin/ when called from parent, bin/ when standalone)
# This ensures arc ALWAYS builds to the main project bin/ directory
TARGET = ../bin/arc

.PHONY: all clean test test-arc-unit test-arc-cli test-arc test-arc-workflow test-arc-background test-arc-full test-arc-all directories count install uninstall

# Default target
all: directories $(TARGET)

# Create build directories
directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(PARENT_BUILD)
	@mkdir -p ../bin/tests
	@mkdir -p bin/tests

# Build arc executable into ../bin/ (links daemon + core)
$(TARGET): $(OBJECTS) $(ARGO_CORE) $(ARGO_DAEMON)
	@mkdir -p ../bin
	$(CC) $(OBJECTS) $(ARGO_DAEMON) $(ARGO_CORE) -o $@ $(LDFLAGS)
	@echo "Built arc CLI -> $@"

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test files
$(BUILD_DIR)/test_%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build test executables into bin/tests/ (exclude main.o to avoid duplicate main)
bin/tests/test_%: $(BUILD_DIR)/test_%.o $(TEST_OBJECTS_LIB) $(ARGO_CORE) $(ARGO_DAEMON)
	@mkdir -p bin/tests
	$(CC) $< $(TEST_OBJECTS_LIB) $(ARGO_DAEMON) $(ARGO_CORE) -o $@ $(LDFLAGS)

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

# Clean build artifacts (only arc's local directories)
clean:
	rm -rf $(BUILD_DIR)
	rm -rf bin/tests

# Installation targets
install:
	install -d $(PREFIX)/bin
	install -m 0755 $(TARGET) $(PREFIX)/bin/arc

# Install shell completions
install-completion:
	@echo "Installing shell completions..."
	@# Detect shell and install appropriate completion
	@if [ "$$SHELL" = "/bin/bash" ] || [ "$$SHELL" = "/usr/bin/bash" ]; then \
		mkdir -p $(HOME)/.local/share/bash-completion/completions; \
		install -m 0644 completions/arc.bash $(HOME)/.local/share/bash-completion/completions/arc; \
		echo "✓ Bash completion installed to ~/.local/share/bash-completion/completions/arc"; \
		if ! grep -q "bash-completion/completions/arc" $(HOME)/.bashrc 2>/dev/null; then \
			echo "  Add to ~/.bashrc: source ~/.local/share/bash-completion/completions/arc"; \
		else \
			echo "  (Already configured in ~/.bashrc)"; \
		fi; \
	elif [ "$$SHELL" = "/bin/zsh" ] || [ "$$SHELL" = "/usr/bin/zsh" ]; then \
		mkdir -p $(HOME)/.zsh/completions; \
		install -m 0644 completions/_arc $(HOME)/.zsh/completions/_arc; \
		echo "✓ Zsh completion installed to ~/.zsh/completions/_arc"; \
		echo ""; \
		if ! grep -q ".zsh/completions" $(HOME)/.zshrc 2>/dev/null; then \
			echo "  Setup needed - add to ~/.zshrc:"; \
			echo "    fpath=(~/.zsh/completions \$$fpath)"; \
			echo "    autoload -U compinit && compinit"; \
			echo ""; \
			echo "  Then reload: exec zsh (or start a new shell)"; \
		else \
			echo "  ✓ Already configured in ~/.zshrc"; \
			echo "  Reload current shell: exec zsh (or start a new shell)"; \
		fi; \
	elif [ "$$SHELL" = "/usr/bin/fish" ] || [ -d "$(HOME)/.config/fish" ]; then \
		mkdir -p $(HOME)/.config/fish/completions; \
		install -m 0644 completions/arc.fish $(HOME)/.config/fish/completions/arc.fish; \
		echo "✓ Fish completion installed to ~/.config/fish/completions/arc.fish"; \
		echo "  (Fish auto-loads completions, restart shell to activate)"; \
	else \
		echo "⚠ Unknown shell: $$SHELL"; \
		echo "  Completions available in: completions/"; \
		echo "    - Bash: completions/arc.bash"; \
		echo "    - Zsh:  completions/_arc"; \
		echo "    - Fish: completions/arc.fish"; \
	fi

uninstall:
	rm -f $(PREFIX)/bin/arc
	rm -f $(HOME)/.local/share/bash-completion/completions/arc
	rm -f $(HOME)/.zsh/completions/_arc
	rm -f $(HOME)/.config/fish/completions/arc.fish

# Debug
debug:
	@echo "CC: $(CC)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "SOURCES: $(SOURCES)"
	@echo "OBJECTS: $(OBJECTS)"
	@echo "TARGET: $(TARGET)"

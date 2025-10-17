# © 2025 Casey Koons All rights reserved
# Argo Project Makefile - Main Orchestrator
# This file includes functional modules and defines top-level targets

# Include configuration (must come first)
include Makefile.config

# Include functional modules
include Makefile.build
include Makefile.test
include Makefile.install
include Makefile.clean
include Makefile.help

# Default target
all: directories $(CORE_LIB) $(DAEMON_LIB) $(WORKFLOW_LIB) $(DAEMON_BINARY) $(WORKFLOW_EXECUTOR_BINARY)

# Full build - clean, build all components, install
full-build: clean-all all-components install-all
	@echo ""
	@echo "=========================================="
	@echo "Full build complete!"
	@echo "=========================================="
	@echo "  - All components built"
	@echo "  - All binaries installed to ~/.local/bin/"
	@echo "  - Arc installed to ~/.local/bin/arc"
	@echo "  - Daemon ready: argo-daemon --port 9876"
	@echo "=========================================="
	@echo ""

# .PHONY declarations
.PHONY: all directories scripts test-quick test-all test-api-live test-providers \
        test-registry test-memory test-lifecycle test-providers test-messaging \
        test-workflow test-persistence test-workflow-loader \
        test-session test-env test-config test-isolated-env test-api test-api-calls test-harnesses test-http \
        test-json test-claude-providers test-arc test-arc-workflow test-arc-background \
        test-arc-full test-arc-all count-core clean distclean check debug \
        update-models harnesses harness-init-basic harness-env-inspect harness-reinit \
        harness-init-error harness-socket harness-terminal harness-workflow-context \
        harness-control-flow harness-ci-interactive harness-loop harness-persona \
        harness-workflow-chain arc ci ui ui-argo-term all-components test-arc test-ui \
        test-all-components count count-summary count-report count-snapshot count-json \
        clean-arc clean-ci clean-ui clean-all install install-arc install-ci install-term install-completion \
        install-all uninstall uninstall-arc uninstall-ci uninstall-term uninstall-all \
        test-shutdown-signals test-concurrent-workflows test-env-precedence \
        test-shared-services test-workflow-registry test-valgrind build-asan \
        test-asan test-asan-full help help-test help-count restart-daemon \
        code-analysis code-analysis-quick code-coverage find-dead-code full-build \
        programming-guidelines

# Code Analysis Targets
code-analysis:
	@echo "Running comprehensive code analysis..."
	@./scripts/analyze_code.sh

code-analysis-quick:
	@echo "Running quick unused function check..."
	@cppcheck --enable=unusedFunction --quiet src/ arc/src/ 2>&1 | grep "The function" | wc -l | xargs echo "Unused functions:"

code-coverage:
	@echo "Setting up code coverage analysis..."
	@./scripts/setup_coverage.sh

find-dead-code:
	@echo "Running dead code detection..."
	@./scripts/find_dead_code.sh

# Programming Guidelines Check
programming-guidelines:
	@echo ""
	@echo "=========================================="
	@echo "Running Programming Guidelines Check"
	@echo "=========================================="
	@./scripts/programming_guidelines.sh /tmp/argo_programming_guidelines_report.txt
	@echo ""
	@echo "Full report available at: /tmp/argo_programming_guidelines_report.txt"


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
all: directories $(BUILD_DIR)/stubs.c $(CORE_LIB) $(DAEMON_LIB) $(WORKFLOW_LIB) $(EXECUTOR_BINARY) $(DAEMON_BINARY)

# .PHONY declarations
.PHONY: all directories scripts test-quick test-all test-api-live test-providers \
        test-registry test-memory test-lifecycle test-providers test-messaging \
        test-workflow test-integration test-persistence test-workflow-loader \
        test-session test-env test-api test-api-calls test-harnesses test-http \
        test-json test-claude-providers test-arc test-arc-workflow test-arc-background \
        test-arc-full test-arc-all count-core clean distclean check debug \
        update-models harnesses harness-init-basic harness-env-inspect harness-reinit \
        harness-init-error harness-socket harness-terminal harness-workflow-context \
        harness-control-flow harness-ci-interactive harness-loop harness-persona \
        harness-workflow-chain arc ui ui-argo-term all-components test-arc test-ui \
        test-all-components count count-summary count-report count-snapshot count-json \
        clean-arc clean-ui clean-all install install-arc install-term install-all \
        uninstall uninstall-arc uninstall-term uninstall-all test-thread-safety \
        test-shutdown-signals test-concurrent-workflows test-env-precedence \
        test-shared-services test-workflow-registry test-valgrind build-asan \
        test-asan test-asan-full help help-test help-count restart-daemon

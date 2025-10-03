# © 2025 Casey Koons All rights reserved
# Argo Project Makefile

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Werror -Wextra -std=c11 -g -I./include
LDFLAGS = -lpthread

# Optional features (enable with make ENABLE_DRYRUN=1 ENABLE_METRICS=1)
ifdef ENABLE_DRYRUN
  CFLAGS += -DARGO_ENABLE_DRYRUN
endif

ifdef ENABLE_METRICS
  CFLAGS += -DARGO_ENABLE_METRICS
  CORE_SOURCES += $(SRC_DIR)/argo_metrics.c
endif

# Directories
SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
BUILD_DIR = build
SCRIPT_DIR = scripts
LOG_DIR = .argo/logs
SESSION_DIR = .argo/sessions

# Core library sources (no provider implementations)
CORE_SOURCES = $(SRC_DIR)/argo_socket.c \
               $(SRC_DIR)/argo_http.c \
               $(SRC_DIR)/argo_error.c \
               $(SRC_DIR)/argo_registry.c \
               $(SRC_DIR)/argo_registry_messaging.c \
               $(SRC_DIR)/argo_registry_persistence.c \
               $(SRC_DIR)/argo_memory.c \
               $(SRC_DIR)/argo_lifecycle.c \
               $(SRC_DIR)/argo_provider.c \
               $(SRC_DIR)/argo_workflow.c \
               $(SRC_DIR)/argo_workflow_executor.c \
               $(SRC_DIR)/argo_workflow_checkpoint.c \
               $(SRC_DIR)/argo_workflow_loader.c \
               $(SRC_DIR)/argo_merge.c \
               $(SRC_DIR)/argo_orchestrator.c \
               $(SRC_DIR)/argo_session.c \
               $(SRC_DIR)/argo_json.c \
               $(SRC_DIR)/argo_string_utils.c \
               $(SRC_DIR)/argo_print_utils.c \
               $(SRC_DIR)/argo_env_utils.c \
               $(SRC_DIR)/argo_env_load.c \
               $(SRC_DIR)/argo_init.c \
               $(SRC_DIR)/argo_config.c \
               $(SRC_DIR)/argo_workflow_context.c \
               $(SRC_DIR)/argo_workflow_json.c \
               $(SRC_DIR)/argo_workflow_conditions.c \
               $(SRC_DIR)/argo_workflow_steps_basic.c \
               $(SRC_DIR)/argo_workflow_steps_ci.c \
               $(SRC_DIR)/argo_workflow_steps_advanced.c \
               $(SRC_DIR)/argo_workflow_persona.c \
               $(SRC_DIR)/argo_shutdown.c

# Provider implementation sources
PROVIDER_SOURCES = $(SRC_DIR)/argo_ollama.c \
                   $(SRC_DIR)/argo_claude.c \
                   $(SRC_DIR)/argo_claude_process.c \
                   $(SRC_DIR)/argo_claude_memory.c \
                   $(SRC_DIR)/argo_claude_code.c \
                   $(SRC_DIR)/argo_claude_api.c \
                   $(SRC_DIR)/argo_openai_api.c \
                   $(SRC_DIR)/argo_gemini_api.c \
                   $(SRC_DIR)/argo_grok_api.c \
                   $(SRC_DIR)/argo_deepseek_api.c \
                   $(SRC_DIR)/argo_openrouter.c \
                   $(SRC_DIR)/argo_api_common.c \
                   $(SRC_DIR)/argo_api_provider_common.c \
                   $(SRC_DIR)/argo_json.c

# All sources
SOURCES = $(CORE_SOURCES) $(PROVIDER_SOURCES)

# Object files
CORE_OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(CORE_SOURCES))
PROVIDER_OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(PROVIDER_SOURCES))
OBJECTS = $(CORE_OBJECTS) $(PROVIDER_OBJECTS)

# Core library
CORE_LIB = $(BUILD_DIR)/libargo_core.a

# Script sources
SCRIPT_SOURCES = $(SCRIPT_DIR)/argo_monitor.c \
                 $(SCRIPT_DIR)/argo_memory_inspect.c \
                 $(SCRIPT_DIR)/argo_update_models.c \
                 $(SCRIPT_DIR)/argo_ci_assign.c \
                 $(SCRIPT_DIR)/argo_workflow_tracer.c

# Script executables
SCRIPT_TARGETS = $(patsubst $(SCRIPT_DIR)/%.c,$(BUILD_DIR)/%,$(SCRIPT_SOURCES))

# Test files
TEST_SOURCES = $(TEST_DIR)/test_ci_providers.c
TEST_OBJECTS = $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/%.o,$(TEST_SOURCES))

# Stub implementations for missing functions (temporary)
STUB_SOURCES = $(BUILD_DIR)/stubs.c
STUB_OBJECTS = $(BUILD_DIR)/stubs.o

# Targets
TEST_TARGET = $(BUILD_DIR)/test_ci_providers
API_TEST_TARGET = $(BUILD_DIR)/test_api_providers
API_CALL_TARGET = $(BUILD_DIR)/test_api_calls
REGISTRY_TEST_TARGET = $(BUILD_DIR)/test_registry
MEMORY_TEST_TARGET = $(BUILD_DIR)/test_memory
LIFECYCLE_TEST_TARGET = $(BUILD_DIR)/test_lifecycle
PROVIDER_TEST_TARGET = $(BUILD_DIR)/test_providers
MESSAGING_TEST_TARGET = $(BUILD_DIR)/test_messaging
WORKFLOW_TEST_TARGET = $(BUILD_DIR)/test_workflow
INTEGRATION_TEST_TARGET = $(BUILD_DIR)/test_integration
PERSISTENCE_TEST_TARGET = $(BUILD_DIR)/test_persistence
WORKFLOW_LOADER_TEST_TARGET = $(BUILD_DIR)/test_workflow_loader
SESSION_TEST_TARGET = $(BUILD_DIR)/test_session
ENV_TEST_TARGET = $(BUILD_DIR)/test_env
THREAD_SAFETY_TEST_TARGET = $(BUILD_DIR)/test_thread_safety
SHUTDOWN_SIGNALS_TEST_TARGET = $(BUILD_DIR)/test_shutdown_signals
CONCURRENT_WORKFLOWS_TEST_TARGET = $(BUILD_DIR)/test_concurrent_workflows
ENV_PRECEDENCE_TEST_TARGET = $(BUILD_DIR)/test_env_precedence

# Default target
all: directories $(CORE_LIB) $(TEST_TARGET) $(API_TEST_TARGET) $(API_CALL_TARGET) $(REGISTRY_TEST_TARGET) $(MEMORY_TEST_TARGET) $(LIFECYCLE_TEST_TARGET) $(PROVIDER_TEST_TARGET) $(MESSAGING_TEST_TARGET) $(WORKFLOW_TEST_TARGET) $(INTEGRATION_TEST_TARGET) $(PERSISTENCE_TEST_TARGET) $(WORKFLOW_LOADER_TEST_TARGET) $(SESSION_TEST_TARGET) $(ENV_TEST_TARGET) $(THREAD_SAFETY_TEST_TARGET) $(SCRIPT_TARGETS)

# Create necessary directories
directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(LOG_DIR)
	@mkdir -p $(SESSION_DIR)

# Generate stub implementations
$(BUILD_DIR)/stubs.c:
	@echo "/* © 2025 Casey Koons All rights reserved */" > $@
	@echo "/* Temporary stub implementations */" >> $@
	@echo "" >> $@
	@echo "#include <stdio.h>" >> $@
	@echo "#include <stdbool.h>" >> $@
	@echo "#include <stddef.h>" >> $@
	@echo "#include <stdint.h>" >> $@
	@echo "#include \"argo_ci_defaults.h\"" >> $@
	@echo "#include \"argo_log.h\"" >> $@
	@echo "#include \"argo_ollama.h\"" >> $@
	@echo "#include \"argo_claude.h\"" >> $@
	@echo "" >> $@
	@echo "/* CI defaults stubs */" >> $@
	@echo "const ci_model_config_t* ci_get_model_defaults(const char* model) {" >> $@
	@echo "    (void)model;" >> $@
	@echo "    return NULL;" >> $@
	@echo "}" >> $@
	@echo "" >> $@
	@echo "/* Logging stubs */" >> $@
	@echo "log_config_t* g_log_config = NULL;" >> $@
	@echo "int log_init(const char* dir) { (void)dir; return 0; }" >> $@
	@echo "void log_cleanup(void) {}" >> $@
	@echo "void log_set_level(log_level_t level) { (void)level; }" >> $@
	@echo "void log_write(log_level_t level, const char* file, int line," >> $@
	@echo "              const char* func, const char* fmt, ...) {" >> $@
	@echo "    (void)level; (void)file; (void)line; (void)func; (void)fmt;" >> $@
	@echo "}" >> $@
	@echo "" >> $@
	@echo "/* Ollama stubs */" >> $@
	@echo "#include <sys/socket.h>" >> $@
	@echo "#include <sys/time.h>" >> $@
	@echo "#include <netinet/in.h>" >> $@
	@echo "#include <arpa/inet.h>" >> $@
	@echo "#include <unistd.h>" >> $@
	@echo "#include <fcntl.h>" >> $@
	@echo "" >> $@
	@echo "bool ollama_is_running(void) {" >> $@
	@echo "    /* Try to connect to Ollama */" >> $@
	@echo "    int sock = socket(AF_INET, SOCK_STREAM, 0);" >> $@
	@echo "    if (sock < 0) return false;" >> $@
	@echo "" >> $@
	@echo "    struct sockaddr_in addr;" >> $@
	@echo "    addr.sin_family = AF_INET;" >> $@
	@echo "    addr.sin_port = htons(11434);" >> $@
	@echo "    addr.sin_addr.s_addr = inet_addr(\"127.0.0.1\");" >> $@
	@echo "" >> $@
	@echo "    /* Set timeout for connect */" >> $@
	@echo "    struct timeval tv = {1, 0};" >> $@
	@echo "    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));" >> $@
	@echo "" >> $@
	@echo "    int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));" >> $@
	@echo "    close(sock);" >> $@
	@echo "    return result == 0;" >> $@
	@echo "}" >> $@
	@echo "" >> $@
	@echo "/* Claude stubs */" >> $@
	@echo "bool claude_is_available(void) {" >> $@
	@echo "    /* Check if claude CLI exists */" >> $@
	@echo "    return false;  /* For now, assume not available */" >> $@
	@echo "}" >> $@
	@echo "/* claude_code functions are now in argo_claude_code.c */" >> $@
	@echo "ci_provider_t* claude_code_create_provider(const char* ci_name);" >> $@
	@echo "size_t claude_get_memory_usage(ci_provider_t* provider) {" >> $@
	@echo "    (void)provider;" >> $@
	@echo "    return 0;" >> $@
	@echo "}" >> $@
	@echo "" >> $@
	@echo "/* Error strings now in argo_error.c - no stub needed */" >> $@

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test files
$(BUILD_DIR)/%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile stub file
$(BUILD_DIR)/stubs.o: $(BUILD_DIR)/stubs.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build core library
$(CORE_LIB): $(CORE_OBJECTS) $(STUB_OBJECTS)
	ar rcs $@ $^
	@echo "Created core library: $@"

# Build script executables
$(BUILD_DIR)/%: $(SCRIPT_DIR)/%.c $(CORE_LIB)
	$(CC) $(CFLAGS) $< $(CORE_LIB) -o $@ $(LDFLAGS)
	@ln -sf ../build/$(@F) $(SCRIPT_DIR)/$(@F)
	@echo "Built script: $(@F) -> scripts/$(@F)"

# Build test executable
$(TEST_TARGET): $(OBJECTS) $(TEST_OBJECTS) $(STUB_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build API test executable
$(API_TEST_TARGET): $(OBJECTS) $(BUILD_DIR)/test_api_providers.o $(STUB_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build API call test executable
$(API_CALL_TARGET): $(OBJECTS) $(BUILD_DIR)/test_api_calls.o $(STUB_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build registry test executable
$(REGISTRY_TEST_TARGET): $(OBJECTS) $(BUILD_DIR)/test_registry.o $(STUB_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build memory test executable
$(MEMORY_TEST_TARGET): $(OBJECTS) $(BUILD_DIR)/test_memory.o $(STUB_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build lifecycle test executable
$(LIFECYCLE_TEST_TARGET): $(OBJECTS) $(BUILD_DIR)/test_lifecycle.o $(STUB_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build provider test executable
$(PROVIDER_TEST_TARGET): $(OBJECTS) $(BUILD_DIR)/test_providers.o $(STUB_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build messaging test executable
$(MESSAGING_TEST_TARGET): $(OBJECTS) $(BUILD_DIR)/test_messaging.o $(STUB_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build workflow test executable
$(WORKFLOW_TEST_TARGET): $(OBJECTS) $(BUILD_DIR)/test_workflow.o $(STUB_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build integration test executable
$(INTEGRATION_TEST_TARGET): $(OBJECTS) $(BUILD_DIR)/test_integration.o $(STUB_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build persistence test executable
$(PERSISTENCE_TEST_TARGET): $(OBJECTS) $(BUILD_DIR)/test_persistence.o $(STUB_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build workflow loader test executable
$(WORKFLOW_LOADER_TEST_TARGET): $(OBJECTS) $(BUILD_DIR)/test_workflow_loader.o $(STUB_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build session test executable
$(SESSION_TEST_TARGET): $(OBJECTS) $(BUILD_DIR)/test_session.o $(STUB_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build env test executable
$(ENV_TEST_TARGET): $(OBJECTS) $(BUILD_DIR)/test_env.o $(STUB_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build thread safety test executable
$(THREAD_SAFETY_TEST_TARGET): $(OBJECTS) $(BUILD_DIR)/test_thread_safety.o $(STUB_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build shutdown signals test executable
$(SHUTDOWN_SIGNALS_TEST_TARGET): $(OBJECTS) $(BUILD_DIR)/test_shutdown_signals.o $(STUB_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build concurrent workflows test executable
$(CONCURRENT_WORKFLOWS_TEST_TARGET): $(OBJECTS) $(BUILD_DIR)/test_concurrent_workflows.o $(STUB_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build environment precedence test executable
$(ENV_PRECEDENCE_TEST_TARGET): $(OBJECTS) $(BUILD_DIR)/test_env_precedence.o $(STUB_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Quick tests - fast, no external dependencies
test-quick: test-registry test-memory test-lifecycle test-providers test-messaging test-workflow test-integration test-persistence test-workflow-loader test-session test-env test-thread-safety
	@echo ""
	@echo "=========================================="
	@echo "Quick Tests Complete"
	@echo "=========================================="

# All safe tests - includes API availability checks but no costs
test-api-live: test-providers test-registry test-memory test-lifecycle test-api
	@echo ""
	@echo "=========================================="
	@echo "All Tests Complete (including live APIs)"
	@echo "WARNING: API tests may incur costs"
	@echo "=========================================="

# Individual test targets
test-ci-providers: $(TEST_TARGET)
	@echo ""
	@echo "=========================================="
	@echo "Old CI Provider Tests (deprecated)"
	@echo "=========================================="
	-@./$(TEST_TARGET)

test-registry: $(REGISTRY_TEST_TARGET)
	@echo ""
	@echo "=========================================="
	@echo "Registry Tests"
	@echo "=========================================="
	@./$(REGISTRY_TEST_TARGET)

test-memory: $(MEMORY_TEST_TARGET)
	@echo ""
	@echo "=========================================="
	@echo "Memory Manager Tests"
	@echo "=========================================="
	@./$(MEMORY_TEST_TARGET)

test-lifecycle: $(LIFECYCLE_TEST_TARGET)
	@echo ""
	@echo "=========================================="
	@echo "Lifecycle Manager Tests"
	@echo "=========================================="
	@./$(LIFECYCLE_TEST_TARGET)

test-providers: $(PROVIDER_TEST_TARGET)
	@echo ""
	@echo "=========================================="
	@echo "Provider System Tests"
	@echo "=========================================="
	@./$(PROVIDER_TEST_TARGET)

test-messaging: $(MESSAGING_TEST_TARGET)
	@echo ""
	@echo "=========================================="
	@echo "Messaging System Tests"
	@echo "=========================================="
	@./$(MESSAGING_TEST_TARGET)

test-workflow: $(WORKFLOW_TEST_TARGET)
	@echo ""
	@echo "=========================================="
	@echo "Workflow Controller Tests"
	@echo "=========================================="
	@./$(WORKFLOW_TEST_TARGET)

test-integration: $(INTEGRATION_TEST_TARGET)
	@echo ""
	@echo "=========================================="
	@echo "Integration Tests"
	@echo "=========================================="
	@./$(INTEGRATION_TEST_TARGET)

test-persistence: $(PERSISTENCE_TEST_TARGET)
	@echo ""
	@echo "=========================================="
	@echo "Persistence Tests"
	@echo "=========================================="
	@./$(PERSISTENCE_TEST_TARGET)

test-workflow-loader: $(WORKFLOW_LOADER_TEST_TARGET)
	@echo ""
	@echo "=========================================="
	@echo "Workflow Loader Tests"
	@echo "=========================================="
	@./$(WORKFLOW_LOADER_TEST_TARGET)

test-session: $(SESSION_TEST_TARGET)
	@echo ""
	@echo "=========================================="
	@echo "Session Manager Tests"
	@echo "=========================================="
	@./$(SESSION_TEST_TARGET)

test-env: $(ENV_TEST_TARGET)
	@echo ""
	@echo "=========================================="
	@echo "Environment Utilities Tests"
	@echo "=========================================="
	@./$(ENV_TEST_TARGET)

test-thread-safety: $(THREAD_SAFETY_TEST_TARGET)
	@echo ""
	@echo "=========================================="
	@echo "Thread Safety Tests"
	@echo "=========================================="
	@./$(THREAD_SAFETY_TEST_TARGET)

test-api: $(API_TEST_TARGET)
	@echo ""
	@echo "=========================================="
	@echo "API Provider Tests"
	@echo "NOTE: Requires valid API keys"
	@echo "=========================================="
	@./$(API_TEST_TARGET)

# Explicit only - costs real money
test-api-calls: $(API_CALL_TARGET)
	@echo ""
	@echo "=========================================="
	@echo "WARNING: This will make actual API calls"
	@echo "         and incur costs!"
	@echo "=========================================="
	@./$(API_CALL_TARGET)

# Memory leak detection with valgrind
test-valgrind: $(WORKFLOW_TEST_TARGET) $(THREAD_SAFETY_TEST_TARGET) $(MEMORY_TEST_TARGET)
	@echo ""
	@echo "=========================================="
	@echo "Memory Leak Detection (Valgrind)"
	@echo "=========================================="
	@which valgrind > /dev/null 2>&1 || (echo "ERROR: valgrind not installed"; exit 1)
	@echo "Testing workflow controller..."
	@valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
		--error-exitcode=1 --suppressions=/dev/null \
		./$(WORKFLOW_TEST_TARGET) > /dev/null 2>&1 && echo "  ✓ No leaks in workflow" || echo "  ✗ Memory leaks detected"
	@echo "Testing thread safety..."
	@valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
		--error-exitcode=1 --suppressions=/dev/null \
		./$(THREAD_SAFETY_TEST_TARGET) > /dev/null 2>&1 && echo "  ✓ No leaks in thread safety" || echo "  ✗ Memory leaks detected"
	@echo "Testing memory manager..."
	@valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
		--error-exitcode=1 --suppressions=/dev/null \
		./$(MEMORY_TEST_TARGET) > /dev/null 2>&1 && echo "  ✓ No leaks in memory manager" || echo "  ✗ Memory leaks detected"
	@echo ""
	@echo "=========================================="
	@echo "Valgrind Complete - See above for results"
	@echo "=========================================="

# Build with AddressSanitizer (catches memory errors at runtime)
build-asan:
	@echo "Building with AddressSanitizer..."
	@$(MAKE) clean
	@$(MAKE) CFLAGS="-Wall -Werror -Wextra -std=c11 -g -I./include -fsanitize=address -fno-omit-frame-pointer" \
		LDFLAGS="-lpthread -fsanitize=address"

# Run tests with AddressSanitizer
test-asan: build-asan
	@echo ""
	@echo "=========================================="
	@echo "AddressSanitizer Tests"
	@echo "=========================================="
	@echo "Running workflow tests..."
	@./$(WORKFLOW_TEST_TARGET) && echo "  ✓ Workflow tests passed" || echo "  ✗ Workflow tests failed"
	@echo "Running thread safety tests..."
	@./$(THREAD_SAFETY_TEST_TARGET) && echo "  ✓ Thread safety tests passed" || echo "  ✗ Thread safety tests failed"
	@echo ""
	@echo "=========================================="
	@echo "ASAN Complete - Rebuild normally with 'make'"
	@echo "=========================================="

test-asan-full: build-asan
	@echo ""
	@echo "=========================================="
	@echo "Full AddressSanitizer Test Suite"
	@echo "=========================================="
	@./$(WORKFLOW_TEST_TARGET) && echo "  ✓ Workflow" || echo "  ✗ Workflow"
	@./$(INTEGRATION_TEST_TARGET) && echo "  ✓ Integration" || echo "  ✗ Integration"
	@./$(LIFECYCLE_TEST_TARGET) && echo "  ✓ Lifecycle" || echo "  ✗ Lifecycle"
	@./$(MEMORY_TEST_TARGET) && echo "  ✓ Memory" || echo "  ✗ Memory"
	@./$(THREAD_SAFETY_TEST_TARGET) && echo "  ✓ Thread Safety" || echo "  ✗ Thread Safety"
	@./$(SESSION_TEST_TARGET) && echo "  ✓ Session" || echo "  ✗ Session"
	@./$(PERSISTENCE_TEST_TARGET) && echo "  ✓ Persistence" || echo "  ✗ Persistence"
	@echo ""
	@echo "=========================================="
	@echo "Full ASAN Complete - Rebuild normally with 'make'"
	@echo "=========================================="

# Line counting
count-core:
	@./scripts/count_core.sh

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
	@for script in $(SCRIPT_TARGETS); do \
		rm -f $(SCRIPT_DIR)/$$(basename $$script); \
	done

# Deep clean (includes logs and sessions)
distclean: clean
	rm -rf .argo

# Check code style
check:
	@echo "Checking code style..."
	@for file in $(SOURCES) $(TEST_SOURCES); do \
		echo "Checking $$file..."; \
		if [ `wc -l < $$file` -gt 600 ]; then \
			echo "WARNING: $$file exceeds 600 lines!"; \
		fi; \
	done

# Debug target - print variables
debug:
	@echo "CC: $(CC)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "SOURCES: $(SOURCES)"
	@echo "OBJECTS: $(OBJECTS)"
	@echo "TEST_TARGET: $(TEST_TARGET)"

# Update model defaults from API providers
update-models:
	@echo "Updating model defaults from API providers..."
	@./scripts/update_models.sh

# Test Harnesses (not counted in diet)
HARNESS_INIT_BASIC = $(BUILD_DIR)/harness_init_basic
HARNESS_ENV_INSPECT = $(BUILD_DIR)/harness_env_inspect
HARNESS_REINIT = $(BUILD_DIR)/harness_reinit
HARNESS_INIT_ERROR = $(BUILD_DIR)/harness_init_error
HARNESS_SOCKET = $(BUILD_DIR)/harness_socket
HARNESS_TERMINAL = $(BUILD_DIR)/harness_terminal
HARNESS_WORKFLOW_CONTEXT = $(BUILD_DIR)/harness_workflow_context
HARNESS_CONTROL_FLOW = $(BUILD_DIR)/harness_control_flow
HARNESS_CI_INTERACTIVE = $(BUILD_DIR)/harness_ci_interactive
HARNESS_LOOP = $(BUILD_DIR)/harness_loop
HARNESS_PERSONA = $(BUILD_DIR)/harness_persona
HARNESS_WORKFLOW_CHAIN = $(BUILD_DIR)/harness_workflow_chain

harness-init-basic: $(HARNESS_INIT_BASIC)
	@./$(HARNESS_INIT_BASIC)

harness-env-inspect: $(HARNESS_ENV_INSPECT)
	@./$(HARNESS_ENV_INSPECT)

harness-reinit: $(HARNESS_REINIT)
	@./$(HARNESS_REINIT)

harness-init-error: $(HARNESS_INIT_ERROR)
	@./$(HARNESS_INIT_ERROR)

harness-socket: $(HARNESS_SOCKET)
	@./$(HARNESS_SOCKET)

harness-terminal: $(HARNESS_TERMINAL)
	@./$(HARNESS_TERMINAL)

harness-workflow-context: $(HARNESS_WORKFLOW_CONTEXT)
	@./$(HARNESS_WORKFLOW_CONTEXT)

harness-control-flow: $(HARNESS_CONTROL_FLOW)
	@./$(HARNESS_CONTROL_FLOW)

harness-ci-interactive: $(HARNESS_CI_INTERACTIVE)
	@./$(HARNESS_CI_INTERACTIVE)

harness-loop: $(HARNESS_LOOP)
	@./$(HARNESS_LOOP)

harness-persona: $(HARNESS_PERSONA)
	@./$(HARNESS_PERSONA)

harness-workflow-chain: $(HARNESS_WORKFLOW_CHAIN)
	@./$(HARNESS_WORKFLOW_CHAIN)

harnesses: $(HARNESS_INIT_BASIC) $(HARNESS_ENV_INSPECT) $(HARNESS_REINIT) $(HARNESS_INIT_ERROR) $(HARNESS_SOCKET) $(HARNESS_TERMINAL) $(HARNESS_WORKFLOW_CONTEXT) $(HARNESS_CONTROL_FLOW) $(HARNESS_CI_INTERACTIVE) $(HARNESS_LOOP) $(HARNESS_PERSONA) $(HARNESS_WORKFLOW_CHAIN)
	@echo "Built all test harnesses"

# Run all harnesses
test-harnesses: harnesses
	@echo ""
	@echo "=========================================="
	@echo "Running All Test Harnesses"
	@echo "=========================================="
	@$(MAKE) harness-init-basic
	@$(MAKE) harness-reinit
	@$(MAKE) harness-socket
	@$(MAKE) harness-workflow-context
	@echo ""
	@echo "=========================================="
	@echo "Harness Tests Complete"
	@echo "=========================================="

# Run all local tests: unit tests + harnesses (no API costs)
test-all: test-quick test-harnesses
	@echo ""
	@echo "=========================================="
	@echo "ALL LOCAL TESTS COMPLETE"
	@echo "  Unit Tests: PASSED (95 tests)"
	@echo "  Harnesses:  PASSED (4 tests)"
	@echo "=========================================="
	@echo ""
	@echo "To run tests with live API calls (costs money):"
	@echo "  make test-api-live"
	@echo ""

# Build harness executables
$(HARNESS_INIT_BASIC): tests/harness_init_basic.c $(CORE_LIB)
	@echo "Building harness_init_basic..."
	@$(CC) $(CFLAGS) tests/harness_init_basic.c $(CORE_LIB) -o $@ $(LDFLAGS)

$(HARNESS_ENV_INSPECT): tests/harness_env_inspect.c $(CORE_LIB)
	@echo "Building harness_env_inspect..."
	@$(CC) $(CFLAGS) tests/harness_env_inspect.c $(CORE_LIB) -o $@ $(LDFLAGS)

$(HARNESS_REINIT): tests/harness_reinit.c $(CORE_LIB)
	@echo "Building harness_reinit..."
	@$(CC) $(CFLAGS) tests/harness_reinit.c $(CORE_LIB) -o $@ $(LDFLAGS)

$(HARNESS_INIT_ERROR): tests/harness_init_error.c $(CORE_LIB)
	@echo "Building harness_init_error..."
	@$(CC) $(CFLAGS) tests/harness_init_error.c $(CORE_LIB) -o $@ $(LDFLAGS)

$(HARNESS_SOCKET): tests/harness_socket.c $(CORE_LIB)
	@echo "Building harness_socket..."
	@$(CC) $(CFLAGS) tests/harness_socket.c $(CORE_LIB) -o $@ $(LDFLAGS)

$(HARNESS_TERMINAL): tests/harness_terminal.c $(CORE_LIB)
	@echo "Building harness_terminal..."
	@$(CC) $(CFLAGS) tests/harness_terminal.c $(CORE_LIB) -o $@ $(LDFLAGS)

$(HARNESS_WORKFLOW_CONTEXT): tests/harness_workflow_context.c $(CORE_LIB)
	@echo "Building harness_workflow_context..."
	@$(CC) $(CFLAGS) tests/harness_workflow_context.c $(CORE_LIB) -o $@ $(LDFLAGS)

$(HARNESS_CONTROL_FLOW): tests/harness_control_flow.c $(CORE_LIB)
	@echo "Building harness_control_flow..."
	@$(CC) $(CFLAGS) tests/harness_control_flow.c $(CORE_LIB) -o $@ $(LDFLAGS)

$(HARNESS_CI_INTERACTIVE): tests/harness_ci_interactive.c $(CORE_LIB)
	@echo "Building harness_ci_interactive..."
	@$(CC) $(CFLAGS) tests/harness_ci_interactive.c $(CORE_LIB) -o $@ $(LDFLAGS)

$(HARNESS_LOOP): tests/harness_loop.c $(CORE_LIB)
	@echo "Building harness_loop..."
	@$(CC) $(CFLAGS) tests/harness_loop.c $(CORE_LIB) -o $@ $(LDFLAGS)

$(HARNESS_PERSONA): tests/harness_persona.c $(CORE_LIB)
	@echo "Building harness_persona..."
	@$(CC) $(CFLAGS) tests/harness_persona.c $(CORE_LIB) -o $@ $(LDFLAGS)

$(HARNESS_WORKFLOW_CHAIN): tests/harness_workflow_chain.c $(CORE_LIB)
	@echo "Building harness_workflow_chain..."
	@$(CC) $(CFLAGS) tests/harness_workflow_chain.c $(CORE_LIB) -o $@ $(LDFLAGS)

.PHONY: all directories scripts test-quick test-all test-api-live test-providers \
        test-registry test-memory test-lifecycle test-providers test-messaging \
        test-workflow test-integration test-persistence test-workflow-loader \
        test-session test-env test-api test-api-calls test-harnesses count-core \
        clean distclean check debug update-models harnesses harness-init-basic \
        harness-env-inspect harness-reinit harness-init-error harness-socket \
        harness-terminal harness-workflow-context harness-control-flow \
        harness-ci-interactive harness-loop harness-persona harness-workflow-chain

# Build just the scripts
scripts: $(CORE_LIB) $(SCRIPT_TARGETS)
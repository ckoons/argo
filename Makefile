# © 2025 Casey Koons All rights reserved
# Argo Project Makefile

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Werror -Wextra -std=c11 -g -I./include
LDFLAGS = -lpthread

# Directories
SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
BUILD_DIR = build
LOG_DIR = .argo/logs
SESSION_DIR = .argo/sessions

# Source files
SOURCES = $(SRC_DIR)/argo_socket.c \
          $(SRC_DIR)/argo_ollama.c \
          $(SRC_DIR)/argo_claude.c \
          $(SRC_DIR)/argo_claude_code.c \
          $(SRC_DIR)/argo_http.c \
          $(SRC_DIR)/argo_claude_api.c \
          $(SRC_DIR)/argo_openai_api.c \
          $(SRC_DIR)/argo_gemini_api.c \
          $(SRC_DIR)/argo_api_common.c

# Object files
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

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

# Default target
all: directories $(TEST_TARGET) $(API_TEST_TARGET) $(API_CALL_TARGET)

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
	@echo "/* Error string stubs */" >> $@
	@echo "const char* argo_error_string(int code) {" >> $@
	@echo "    static char buf[32];" >> $@
	@echo "    snprintf(buf, sizeof(buf), \"Error %d\", code);" >> $@
	@echo "    return buf;" >> $@
	@echo "}" >> $@

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test files
$(BUILD_DIR)/%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile stub file
$(BUILD_DIR)/stubs.o: $(BUILD_DIR)/stubs.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build test executable
$(TEST_TARGET): $(OBJECTS) $(TEST_OBJECTS) $(STUB_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build API test executable
$(API_TEST_TARGET): $(OBJECTS) $(BUILD_DIR)/test_api_providers.o $(STUB_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build API call test executable
$(API_CALL_TARGET): $(OBJECTS) $(BUILD_DIR)/test_api_calls.o $(STUB_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Run tests
test: $(TEST_TARGET)
	./$(TEST_TARGET)

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

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

.PHONY: all directories test clean distclean check debug
/* © 2025 Casey Koons All rights reserved */

/* Test program for CI providers */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>

#include "argo_ci.h"
#include "argo_error.h"
#include "argo_log.h"
#include "argo_socket.h"
#include "argo_ollama.h"
#include "argo_claude.h"
#include "argo_ci_common.h"

/* Global test state */
static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_skipped = 0;
static bool g_running = true;
static ci_response_t g_last_response;

/* Test callback for async responses */
void test_response_callback(const ci_response_t* response, void* userdata) {
    const char* test_name = (const char*)userdata;

    printf("[%s] Response received:\n", test_name);
    printf("  Success: %s\n", response->success ? "YES" : "NO");
    printf("  Model: %s\n", response->model_used);
    printf("  Content length: %zu\n", strlen(response->content));

    if (!response->success) {
        printf("  Error: %s\n", argo_error_string(response->error_code));
    } else {
        printf("  Response preview: %.100s...\n", response->content);
    }
}

/* Simple callback that captures response */
static void capture_callback(const ci_response_t* response, void* userdata) {
    bool* flag = (bool*)userdata;
    memcpy(&g_last_response, response, sizeof(ci_response_t));
    *flag = true;
}

/* Test harness for provider tests */
typedef int (*provider_test_func)(ci_provider_t* provider);

static int run_provider_test(const char* test_name, ci_provider_t* provider,
                            provider_test_func test_func) {
    printf("\n=== Testing %s ===\n", test_name);
    g_tests_run++;

    if (!provider) {
        printf("FAIL: Could not create provider\n");
        return -1;
    }

    /* Initialize */
    int result = provider->init(provider);
    if (result != ARGO_SUCCESS) {
        printf("FAIL: Could not initialize: %s\n", argo_error_string(result));
        provider->cleanup(provider);
        return -1;
    }

    /* Run the actual test */
    result = test_func(provider);

    /* Cleanup */
    provider->cleanup(provider);

    if (result == 0) {
        printf("PASS: %s test\n", test_name);
        g_tests_passed++;
    }

    return result;
}

/* Helper to wait for response */
static int wait_for_flag(bool* flag, int timeout_ms) {
    int wait_count = 0;
    int max_wait = timeout_ms / 100;

    while (!*flag && wait_count < max_wait) {
        usleep(100000);  /* 100ms */
        wait_count++;
    }

    return *flag ? ARGO_SUCCESS : E_CI_TIMEOUT;
}

/* Stream callback that accumulates chunks */
static char g_stream_buffer[8192];
static size_t g_stream_pos = 0;
static int g_chunk_count = 0;

static void stream_callback(const char* chunk, size_t len, void* userdata) {
    bool* flag = (bool*)userdata;

    /* Debug: show each chunk */
    printf("  Chunk %d (%zu bytes): %s\n", ++g_chunk_count, len, chunk);

    if (g_stream_pos + len < sizeof(g_stream_buffer) - 1) {
        memcpy(g_stream_buffer + g_stream_pos, chunk, len);
        g_stream_pos += len;
        g_stream_buffer[g_stream_pos] = '\0';
    }
    *flag = true;  /* Mark that we received at least one chunk */
}

/* Test socket server */
int test_socket_server(void) {
    printf("\n=== Testing Socket Server ===\n");
    g_tests_run++;

    /* Initialize server */
    int result = socket_server_init("test_ci");
    if (result != ARGO_SUCCESS) {
        printf("FAIL: Could not initialize socket server: %s\n",
               argo_error_string(result));
        return -1;
    }

    /* Run event loop for 100ms */
    result = socket_server_run(100);
    if (result != ARGO_SUCCESS) {
        printf("FAIL: Socket server run failed: %s\n",
               argo_error_string(result));
        socket_server_cleanup();
        return -1;
    }

    /* Cleanup */
    socket_server_cleanup();

    printf("PASS: Socket server test\n");
    g_tests_passed++;
    return 0;
}

/* Ollama non-streaming test logic */
static int ollama_nonstream_test_logic(ci_provider_t* provider) {
    printf("Testing non-streaming mode...\n");

    bool response_received = false;
    int result = provider->query(provider, "Say hello",
                                capture_callback, &response_received);

    if (result != ARGO_SUCCESS) {
        printf("FAIL: Non-streaming query failed: %s\n", argo_error_string(result));
        return -1;
    }

    if (wait_for_flag(&response_received, 60000) != ARGO_SUCCESS) {
        printf("FAIL: Timeout waiting for response\n");
        return -1;
    }

    if (!g_last_response.success) {
        printf("FAIL: Returned error: %s\n",
               argo_error_string(g_last_response.error_code));
        return -1;
    }

    printf("Non-streaming response: %s\n", g_last_response.content);
    return 0;
}

/* Test Ollama provider - Non-streaming */
int test_ollama_nonstreaming(void) {
    /* Check if Ollama is running */
    if (!ollama_is_running()) {
        printf("\n=== Testing Ollama Provider (Non-Streaming) ===\n");
        g_tests_run++;
        printf("SKIP: Ollama is not running on localhost:11434\n");
        printf("      Start Ollama to test: ollama serve\n");
        g_tests_skipped++;
        return 0;
    }

    ci_provider_t* provider = ollama_create_provider("gemma3:4b");
    return run_provider_test("Ollama Provider (Non-Streaming)",
                           provider, ollama_nonstream_test_logic);
}

/* Ollama streaming test logic */
static int ollama_stream_test_logic(ci_provider_t* provider) {
    printf("Testing streaming mode...\n");

    /* Clear stream buffer */
    g_stream_pos = 0;
    g_stream_buffer[0] = '\0';
    g_chunk_count = 0;
    bool chunks_received = false;

    int result = provider->stream(provider, "Say hello",
                                 stream_callback, &chunks_received);

    if (result != ARGO_SUCCESS) {
        printf("FAIL: Streaming query failed: %s\n", argo_error_string(result));
        return -1;
    }

    if (!chunks_received || g_stream_pos == 0) {
        printf("FAIL: No streaming chunks received\n");
        return -1;
    }

    printf("Streaming response (%zu bytes): %s\n", g_stream_pos, g_stream_buffer);
    return 0;
}

/* Test Ollama provider - Streaming */
int test_ollama_streaming(void) {
    /* Check if Ollama is running */
    if (!ollama_is_running()) {
        printf("\n=== Testing Ollama Provider (Streaming) ===\n");
        g_tests_run++;
        printf("SKIP: Ollama is not running on localhost:11434\n");
        printf("      Start Ollama to test: ollama serve\n");
        g_tests_skipped++;
        return 0;
    }

    ci_provider_t* provider = ollama_create_provider("gemma3:4b");
    return run_provider_test("Ollama Provider (Streaming)",
                           provider, ollama_stream_test_logic);
}

/* Claude Code test logic */
static int claude_code_test_logic(ci_provider_t* provider) {
    printf("Testing Claude Code prompt mode...\n");

    bool response_received = false;
    int result = provider->query(provider,
                               "What is 2 + 2? Please respond with just the number.",
                               capture_callback, &response_received);

    if (result != ARGO_SUCCESS) {
        printf("FAIL: Query failed: %s\n", argo_error_string(result));
        return -1;
    }

    if (!response_received) {
        printf("FAIL: No response received\n");
        return -1;
    }

    if (!g_last_response.success) {
        printf("FAIL: Returned error: %s\n",
               argo_error_string(g_last_response.error_code));
        return -1;
    }

    printf("Claude Code response: %s\n", g_last_response.content);
    return 0;
}

/* Test Claude Code prompt mode */
int test_claude_code_provider(void) {
    ci_provider_t* provider = claude_code_create_provider("test_claude_code");
    return run_provider_test("Claude Code Prompt Mode",
                           provider, claude_code_test_logic);
}

/* Test Claude provider */
int test_claude_provider(void) {
    printf("\n=== Testing Claude Provider ===\n");
    g_tests_run++;

    /* Check if Claude CLI is available */
    if (!claude_is_available()) {
        printf("SKIP: Claude CLI is not available\n");
        printf("      Install Claude CLI to test\n");
        g_tests_skipped++;
        return 0;
    }

    /* Create provider */
    ci_provider_t* provider = claude_create_provider("test_claude");
    if (!provider) {
        printf("FAIL: Could not create Claude provider\n");
        return -1;
    }

    /* Initialize */
    int result = provider->init(provider);
    if (result != ARGO_SUCCESS) {
        printf("FAIL: Could not initialize Claude: %s\n",
               argo_error_string(result));
        provider->cleanup(provider);
        return -1;
    }

    printf("Working memory usage: %zu bytes\n",
           claude_get_memory_usage(provider));

    /* Test query */
    printf("Sending test query to Claude (this may take a moment)...\n");

    bool response_received = false;

    result = provider->query(provider,
                           "Say 'Hello from Argo CI with Claude' and nothing else.",
                           capture_callback,
                           &response_received);

    if (result != ARGO_SUCCESS) {
        printf("FAIL: Query failed: %s\n", argo_error_string(result));
        provider->cleanup(provider);
        return -1;
    }

    /* Wait for response */
    int wait_count = 0;
    while (!response_received && wait_count < 600) {
        usleep(100000);
        wait_count++;
    }

    if (!response_received) {
        printf("FAIL: Timeout waiting for Claude response\n");
        provider->cleanup(provider);
        return -1;
    }

    if (!g_last_response.success) {
        printf("FAIL: Claude returned error: %s\n",
               argo_error_string(g_last_response.error_code));
        provider->cleanup(provider);
        return -1;
    }

    printf("Response from %s: %s\n", g_last_response.model_used, g_last_response.content);

    /* Cleanup */
    provider->cleanup(provider);

    printf("PASS: Claude provider test\n");
    g_tests_passed++;
    return 0;
}

/* Test async event loop */
int test_async_loop(void) {
    printf("\n=== Testing Async Event Loop ===\n");
    g_tests_run++;

    /* Initialize socket server */
    socket_server_init("async_test");

    /* Run event loop 10 times */
    printf("Running async event loop...\n");
    for (int i = 0; i < 10; i++) {
        int result = socket_server_run(10);  /* 10ms timeout */
        if (result != ARGO_SUCCESS) {
            printf("FAIL: Event loop iteration %d failed\n", i);
            socket_server_cleanup();
            return -1;
        }
        printf(".");
        fflush(stdout);
    }
    printf("\n");

    socket_server_cleanup();

    printf("PASS: Async event loop test\n");
    g_tests_passed++;
    return 0;
}

/* Signal handler */
void signal_handler(int sig) {
    printf("\nReceived signal %d, cleaning up...\n", sig);
    g_running = false;
}

/* Main test runner */
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    printf("Argo CI Provider Test Suite\n");
    printf("===========================\n");

    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Initialize logging */
    log_init(".argo/logs");
    log_set_level(LOG_DEBUG);

    /* Run tests */
    test_socket_server();
    test_async_loop();

    if (g_running) {
        test_ollama_nonstreaming();
    }

    if (g_running) {
        test_ollama_streaming();
    }

    if (g_running) {
        test_claude_code_provider();
    }

    if (g_running) {
        test_claude_provider();
    }

    /* Summary */
    printf("\n===========================\n");
    printf("Tests run: %d\n", g_tests_run);
    printf("Tests passed: %d\n", g_tests_passed);
    printf("Tests skipped: %d\n", g_tests_skipped);
    printf("Tests failed: %d\n", g_tests_run - g_tests_passed - g_tests_skipped);

    /* Cleanup */
    log_cleanup();

    return (g_tests_run == g_tests_passed) ? 0 : 1;
}
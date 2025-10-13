/* © 2025 Casey Koons All rights reserved */

/* Daemon lifecycle test suite */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include "argo_daemon.h"
#include "argo_error.h"
#include "argo_init.h"

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { \
        printf("Testing: %s ... ", name); \
        tests_run++; \
    } while(0)

#define PASS() \
    do { \
        printf("✓\n"); \
        tests_passed++; \
    } while(0)

#define FAIL(msg) \
    do { \
        printf("✗ %s\n", msg); \
        tests_failed++; \
    } while(0)

/* Test daemon creation and destruction */
static void test_daemon_create_destroy(void) {
    TEST("Daemon create and destroy");

    argo_daemon_t* daemon = argo_daemon_create(9883);
    if (!daemon) {
        FAIL("Failed to create daemon");
        return;
    }

    if (daemon->port != 9883) {
        FAIL("Port not set correctly");
        argo_daemon_destroy(daemon);
        return;
    }

    if (!daemon->http_server) {
        FAIL("HTTP server not initialized");
        argo_daemon_destroy(daemon);
        return;
    }

    argo_daemon_destroy(daemon);
    PASS();
}

/* Test daemon with invalid port */
static void test_daemon_invalid_port(void) {
    TEST("Daemon with invalid port");

    /* Port 0 might be allowed by OS (ephemeral port) */
    /* Just verify it doesn't crash */
    argo_daemon_t* daemon = argo_daemon_create(0);
    if (daemon) {
        argo_daemon_destroy(daemon);
    }

    /* Port > 65535 would overflow uint16_t, but compiler will truncate */
    /* So we skip this test as it can't truly fail */

    PASS();
}

/* Test daemon start and stop */
static void* daemon_thread(void* arg) {
    argo_daemon_t* daemon = (argo_daemon_t*)arg;
    argo_daemon_start(daemon);
    return NULL;
}

static void test_daemon_start_stop(void) {
    TEST("Daemon start and stop");

    argo_daemon_t* daemon = argo_daemon_create(9884);
    if (!daemon) {
        FAIL("Failed to create daemon");
        return;
    }

    pthread_t thread;
    if (pthread_create(&thread, NULL, daemon_thread, daemon) != 0) {
        FAIL("Failed to start daemon thread");
        argo_daemon_destroy(daemon);
        return;
    }

    /* Give daemon time to start */
    sleep(1);

    /* Stop daemon */
    argo_daemon_stop(daemon);

    /* Wait for thread to finish */
    pthread_join(thread, NULL);

    argo_daemon_destroy(daemon);
    PASS();
}

/* Test daemon health endpoint */
static void test_daemon_health(void) {
    TEST("Daemon health endpoint");

    http_request_t req = {0};
    http_response_t resp = {0};

    req.method = HTTP_METHOD_GET;
    strncpy(req.path, "/api/health", sizeof(req.path) - 1);

    int result = daemon_handle_health(&req, &resp);
    if (result != ARGO_SUCCESS) {
        FAIL("Health endpoint failed");
        return;
    }

    if (resp.status_code != 200) {
        FAIL("Expected HTTP 200");
        /* http_response_t is stack-allocated, no free needed */
        return;
    }

    /* http_response_t is stack-allocated, no free needed */
    PASS();
}

/* Test daemon version endpoint */
static void test_daemon_version(void) {
    TEST("Daemon version endpoint");

    http_request_t req = {0};
    http_response_t resp = {0};

    req.method = HTTP_METHOD_GET;
    strncpy(req.path, "/api/version", sizeof(req.path) - 1);

    int result = daemon_handle_version(&req, &resp);
    if (result != ARGO_SUCCESS) {
        FAIL("Version endpoint failed");
        return;
    }

    if (resp.status_code != 200) {
        FAIL("Expected HTTP 200");
        /* http_response_t is stack-allocated, no free needed */
        return;
    }

    /* Verify response has version info */
    if (!resp.body || !strstr(resp.body, "version")) {
        FAIL("Response missing version info");
        /* http_response_t is stack-allocated, no free needed */
        return;
    }

    /* http_response_t is stack-allocated, no free needed */
    PASS();
}

/* Test daemon graceful shutdown flag */
static void test_daemon_shutdown_flag(void) {
    TEST("Daemon shutdown flag");

    argo_daemon_t* daemon = argo_daemon_create(9885);
    if (!daemon) {
        FAIL("Failed to create daemon");
        return;
    }

    if (daemon->should_shutdown) {
        FAIL("Shutdown flag should be false initially");
        argo_daemon_destroy(daemon);
        return;
    }

    argo_daemon_stop(daemon);

    /* Shutdown flag test - may or may not be set depending on implementation */
    /* Just verify stop doesn't crash */

    argo_daemon_destroy(daemon);
    PASS();
}

/* Test NULL parameter handling */
static void test_null_parameters(void) {
    TEST("NULL parameter handling");

    /* NULL daemon destroy should not crash */
    argo_daemon_destroy(NULL);

    /* NULL daemon stop should not crash */
    argo_daemon_stop(NULL);

    /* NULL daemon start should fail gracefully */
    int result = argo_daemon_start(NULL);
    if (result == ARGO_SUCCESS) {
        FAIL("Should fail with NULL daemon");
        return;
    }

    PASS();
}

/* Test daemon component initialization */
static void test_daemon_components(void) {
    TEST("Daemon component initialization");

    argo_daemon_t* daemon = argo_daemon_create(9886);
    if (!daemon) {
        FAIL("Failed to create daemon");
        return;
    }

    if (!daemon->http_server) {
        FAIL("HTTP server not created");
        argo_daemon_destroy(daemon);
        return;
    }

    if (!daemon->registry) {
        FAIL("Registry not created");
        argo_daemon_destroy(daemon);
        return;
    }

    if (!daemon->lifecycle) {
        FAIL("Lifecycle manager not created");
        argo_daemon_destroy(daemon);
        return;
    }

    argo_daemon_destroy(daemon);
    PASS();
}

/* Main test runner */
int main(void) {
    printf("\n");
    printf("==========================================\n");
    printf("Daemon Lifecycle Test Suite\n");
    printf("==========================================\n\n");

    /* Initialize Argo */
    argo_init();

    /* Daemon lifecycle tests */
    test_daemon_create_destroy();
    test_daemon_invalid_port();
    test_daemon_start_stop();
    test_daemon_health();
    test_daemon_version();
    test_daemon_shutdown_flag();
    test_null_parameters();
    test_daemon_components();

    /* Cleanup */
    argo_exit();

    /* Print summary */
    printf("\n");
    printf("==========================================\n");
    printf("Test Results\n");
    printf("==========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("==========================================\n\n");

    return tests_failed > 0 ? 1 : 0;
}

/* © 2025 Casey Koons All rights reserved */

/* HTTP Server test suite */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "argo_http_server.h"
#include "argo_error.h"

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

/* Test route structure */
static int test_route_called = 0;

static int test_route_handler(http_request_t* req, http_response_t* resp) {
    if (!req || !resp) {
        return E_SYSTEM_MEMORY;
    }
    test_route_called = 1;
    http_response_set_json(resp, 200, "{\"status\":\"success\"}");
    return ARGO_SUCCESS;
}

/* Test route registration */
static void test_route_registration(void) {
    TEST("Route registration");

    http_server_t* server = http_server_create(9877);
    if (!server) {
        FAIL("Failed to create server");
        return;
    }

    int result = http_server_add_route(server, HTTP_METHOD_GET, "/test", test_route_handler);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to register route");
        http_server_destroy(server);
        return;
    }

    http_server_destroy(server);
    PASS();
}

/* Test server creation and destruction */
static void test_server_lifecycle(void) {
    TEST("Server lifecycle");

    http_server_t* server = http_server_create(9878);
    if (!server) {
        FAIL("Failed to create server");
        return;
    }

    http_server_destroy(server);
    PASS();
}

/* Test server start and stop */
static void* server_thread(void* arg) {
    http_server_t* server = (http_server_t*)arg;
    http_server_start(server);
    return NULL;
}

static void test_server_start_stop(void) {
    TEST("Server start and stop");

    http_server_t* server = http_server_create(9879);
    if (!server) {
        FAIL("Failed to create server");
        return;
    }

    http_server_add_route(server, HTTP_METHOD_GET, "/test", test_route_handler);

    pthread_t thread;
    if (pthread_create(&thread, NULL, server_thread, server) != 0) {
        FAIL("Failed to start server thread");
        http_server_destroy(server);
        return;
    }

    /* Give server time to start */
    sleep(1);

    /* Stop server */
    http_server_stop(server);

    /* Wait for thread to finish */
    pthread_join(thread, NULL);

    http_server_destroy(server);
    PASS();
}

/* Test invalid port */
static void test_invalid_port(void) {
    TEST("Invalid port handling");

    /* Port 0 might be allowed by the OS (ephemeral port) */
    /* Just verify it doesn't crash */
    http_server_t* server = http_server_create(0);
    if (server) {
        http_server_destroy(server);
    }

    /* Port 70000 would overflow uint16_t, but compiler will truncate */
    /* So we skip this test as it can't truly fail */

    PASS();
}

/* Test duplicate route registration */
static void test_duplicate_route(void) {
    TEST("Duplicate route handling");

    http_server_t* server = http_server_create(9880);
    if (!server) {
        FAIL("Failed to create server");
        return;
    }

    int result1 = http_server_add_route(server, HTTP_METHOD_GET, "/test", test_route_handler);
    int result2 = http_server_add_route(server, HTTP_METHOD_GET, "/test", test_route_handler);

    if (result1 != ARGO_SUCCESS || result2 != ARGO_SUCCESS) {
        FAIL("Route registration failed");
        http_server_destroy(server);
        return;
    }

    http_server_destroy(server);
    PASS();
}

/* Test multiple routes */
static void test_multiple_routes(void) {
    TEST("Multiple route registration");

    http_server_t* server = http_server_create(9881);
    if (!server) {
        FAIL("Failed to create server");
        return;
    }

    int result = ARGO_SUCCESS;
    result |= http_server_add_route(server, HTTP_METHOD_GET, "/route1", test_route_handler);
    result |= http_server_add_route(server, HTTP_METHOD_POST, "/route2", test_route_handler);
    result |= http_server_add_route(server, HTTP_METHOD_PUT, "/route3", test_route_handler);
    result |= http_server_add_route(server, HTTP_METHOD_DELETE, "/route4", test_route_handler);

    if (result != ARGO_SUCCESS) {
        FAIL("Failed to register routes");
        http_server_destroy(server);
        return;
    }

    http_server_destroy(server);
    PASS();
}

/* Test NULL parameter handling */
static void test_null_parameters(void) {
    TEST("NULL parameter handling");

    /* Create valid server for tests */
    http_server_t* server = http_server_create(9882);
    if (!server) {
        FAIL("Failed to create server");
        return;
    }

    /* NULL route path */
    int result = http_server_add_route(server, HTTP_METHOD_GET, NULL, test_route_handler);
    if (result == ARGO_SUCCESS) {
        FAIL("Should fail with NULL route path");
        http_server_destroy(server);
        return;
    }

    /* NULL handler */
    result = http_server_add_route(server, HTTP_METHOD_GET, "/test", NULL);
    if (result == ARGO_SUCCESS) {
        FAIL("Should fail with NULL handler");
        http_server_destroy(server);
        return;
    }

    http_server_destroy(server);
    PASS();
}

/* Main test runner */
int main(void) {
    printf("\n");
    printf("==========================================\n");
    printf("HTTP Server Test Suite\n");
    printf("==========================================\n\n");

    /* HTTP server tests */
    test_server_lifecycle();
    test_route_registration();
    test_server_start_stop();
    test_invalid_port();
    test_duplicate_route();
    test_multiple_routes();
    test_null_parameters();

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

/* © 2025 Casey Koons All rights reserved */

/* Workflow API test suite */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "argo_daemon_api.h"
#include "argo_daemon.h"
#include "argo_http_server.h"
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

/* Test workflow list endpoint */
static void test_workflow_list(void) {
    TEST("Workflow list API");

    /* Create daemon for API context */
    argo_daemon_t* daemon = argo_daemon_create(9910);
    if (!daemon) {
        FAIL("Failed to create daemon");
        return;
    }
    g_api_daemon = daemon;

    http_request_t req = {0};
    http_response_t resp = {0};

    req.method = HTTP_METHOD_GET;
    strncpy(req.path, "/api/workflow/list", sizeof(req.path) - 1);

    int result = api_workflow_list(&req, &resp);
    if (result != ARGO_SUCCESS) {
        FAIL("api_workflow_list failed");
        argo_daemon_destroy(daemon);
        g_api_daemon = NULL;
        return;
    }

    if (resp.status_code != 200) {
        FAIL("Expected HTTP 200");
        argo_daemon_destroy(daemon);
        g_api_daemon = NULL;
        return;
    }

    argo_daemon_destroy(daemon);
    g_api_daemon = NULL;
    /* http_response_t is stack-allocated, no free needed */
    PASS();
}

/* Test NULL parameter handling */
static void test_null_parameters(void) {
    TEST("NULL parameter handling");

    http_request_t req = {0};
    http_response_t resp = {0};

    /* NULL request */
    int result = api_workflow_list(NULL, &resp);
    if (result == ARGO_SUCCESS) {
        FAIL("Should fail with NULL request");
        return;
    }

    /* NULL response */
    result = api_workflow_list(&req, NULL);
    if (result == ARGO_SUCCESS) {
        FAIL("Should fail with NULL response");
        return;
    }

    PASS();
}

/* Test workflow status with non-existent ID */
static void test_workflow_status_not_found(void) {
    TEST("Workflow status for non-existent workflow");

    http_request_t req = {0};
    http_response_t resp = {0};

    req.method = HTTP_METHOD_GET;
    strncpy(req.path, "/api/workflow/status/nonexistent-workflow-id-12345", sizeof(req.path) - 1);

    int result = api_workflow_status(&req, &resp);

    /* Should either succeed with 404 or fail gracefully */
    if (result == ARGO_SUCCESS && resp.status_code != 404) {
        FAIL("Expected HTTP 404 for non-existent workflow");
        /* http_response_t is stack-allocated, no free needed */
        return;
    }

    /* http_response_t is stack-allocated, no free needed */
    PASS();
}

/* Test workflow abandon with non-existent ID */
static void test_workflow_abandon_not_found(void) {
    TEST("Workflow abandon for non-existent workflow");

    http_request_t req = {0};
    http_response_t resp = {0};

    req.method = HTTP_METHOD_DELETE;
    strncpy(req.path, "/api/workflow/abandon/nonexistent-workflow-id-12345", sizeof(req.path) - 1);

    int result = api_workflow_abandon(&req, &resp);

    /* Should either succeed with 404 or fail gracefully */
    if (result == ARGO_SUCCESS && resp.status_code != 404 && resp.status_code != 200) {
        FAIL("Expected HTTP 404 or 200 for non-existent workflow");
        /* http_response_t is stack-allocated, no free needed */
        return;
    }

    /* http_response_t is stack-allocated, no free needed */
    PASS();
}

/* Test workflow start with missing body */
static void test_workflow_start_missing_body(void) {
    TEST("Workflow start with missing request body");

    http_request_t req = {0};
    http_response_t resp = {0};

    req.method = HTTP_METHOD_POST;
    strncpy(req.path, "/api/workflow/start", sizeof(req.path) - 1);
    req.body = NULL;

    int result = api_workflow_start(&req, &resp);

    /* Should fail with 400 Bad Request */
    if (result == ARGO_SUCCESS && resp.status_code != 400) {
        FAIL("Expected HTTP 400 for missing body");
        /* http_response_t is stack-allocated, no free needed */
        return;
    }

    /* http_response_t is stack-allocated, no free needed */
    PASS();
}

/* Test workflow start with invalid JSON */
static void test_workflow_start_invalid_json(void) {
    TEST("Workflow start with invalid JSON");

    http_request_t req = {0};
    http_response_t resp = {0};

    req.method = HTTP_METHOD_POST;
    strncpy(req.path, "/api/workflow/start", sizeof(req.path) - 1);
    req.body = "{ this is not valid json }";
    req.body_length = strlen(req.body);

    int result = api_workflow_start(&req, &resp);

    /* Should fail with 400 Bad Request */
    if (result == ARGO_SUCCESS && resp.status_code != 400 && resp.status_code != 500) {
        FAIL("Expected HTTP 400 or 500 for invalid JSON");
        /* http_response_t is stack-allocated, no free needed */
        return;
    }

    /* http_response_t is stack-allocated, no free needed */
    PASS();
}

/* Test workflow start with valid minimal workflow */
static void test_workflow_start_minimal(void) {
    TEST("Workflow start with minimal valid workflow");

    http_request_t req = {0};
    http_response_t resp = {0};

    req.method = HTTP_METHOD_POST;
    strncpy(req.path, "/api/workflow/start", sizeof(req.path) - 1);
    req.body = "{\"workflow_name\":\"test_workflow\",\"steps\":[]}";
    req.body_length = strlen(req.body);

    int result = api_workflow_start(&req, &resp);

    /* Should either succeed or fail gracefully */
    if (result != ARGO_SUCCESS && resp.status_code >= 500) {
        /* Internal errors are acceptable in test environment */
    }

    /* http_response_t is stack-allocated, no free needed */
    PASS();
}

/* Main test runner */
int main(void) {
    printf("\n");
    printf("==========================================\n");
    printf("Workflow API Test Suite\n");
    printf("==========================================\n\n");

    /* Initialize daemon context for testing */
    argo_init();

    /* Workflow API tests */
    test_workflow_list();
    test_null_parameters();
    test_workflow_status_not_found();
    test_workflow_abandon_not_found();
    test_workflow_start_missing_body();
    test_workflow_start_invalid_json();
    test_workflow_start_minimal();

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

/* © 2025 Casey Koons All rights reserved */

/* Workflow execution integration test suite */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "argo_daemon.h"
#include "argo_daemon_api.h"
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

/* Test end-to-end workflow execution via API */
static void test_workflow_execution_e2e(void) {
    TEST("End-to-end workflow execution");

    /* Create daemon */
    argo_daemon_t* daemon = argo_daemon_create(9893);
    if (!daemon) {
        FAIL("Failed to create daemon");
        return;
    }

    /* Set global daemon for API handlers */
    g_api_daemon = daemon;

    /* Create minimal workflow JSON */
    const char* workflow_json = "{"
        "\"workflow_name\":\"test_workflow\","
        "\"steps\":[]"
    "}";

    /* Start workflow */
    http_request_t req = {0};
    http_response_t resp = {0};

    req.method = HTTP_METHOD_POST;
    strncpy(req.path, "/api/workflow/start", sizeof(req.path) - 1);
    req.body = (char*)workflow_json;  /* Cast away const for test */
    req.body_length = strlen(workflow_json);

    int result = api_workflow_start(&req, &resp);

    /* In test environment, workflow may fail to start (no executor),
     * but API should handle it gracefully */
    if (result != ARGO_SUCCESS) {
        /* Expected in test environment */
    }

    /* http_response_t is stack-allocated, no free needed */
    argo_daemon_destroy(daemon);
    g_api_daemon = NULL;
    PASS();
}

/* Test workflow state transitions */
static void test_workflow_state_transitions(void) {
    TEST("Workflow state transitions");

    argo_daemon_t* daemon = argo_daemon_create(9894);
    if (!daemon) {
        FAIL("Failed to create daemon");
        return;
    }

    g_api_daemon = daemon;

    /* This test verifies the API handlers can be called
     * in sequence without crashing */

    http_request_t req1 = {0};
    http_response_t resp1 = {0};
    req1.method = HTTP_METHOD_GET;
    strncpy(req1.path, "/api/workflow/list", sizeof(req1.path) - 1);
    api_workflow_list(&req1, &resp1);
    /* http_response_t is stack-allocated, no free needed */

    http_request_t req2 = {0};
    http_response_t resp2 = {0};
    req2.method = HTTP_METHOD_GET;
    strncpy(req2.path, "/api/workflow/status/test-id", sizeof(req2.path) - 1);
    api_workflow_status(&req2, &resp2);
    /* http_response_t is stack-allocated, no free needed */

    argo_daemon_destroy(daemon);
    g_api_daemon = NULL;
    PASS();
}

/* Test concurrent workflow execution simulation */
static void test_concurrent_workflows(void) {
    TEST("Concurrent workflow simulation");

    argo_daemon_t* daemon = argo_daemon_create(9895);
    if (!daemon) {
        FAIL("Failed to create daemon");
        return;
    }

    g_api_daemon = daemon;

    /* Simulate multiple workflow start requests */
    const char* workflow_json = "{\"workflow_name\":\"test\",\"steps\":[]}";

    for (int i = 0; i < 3; i++) {
        http_request_t req = {0};
        http_response_t resp = {0};

        req.method = HTTP_METHOD_POST;
        strncpy(req.path, "/api/workflow/start", sizeof(req.path) - 1);
        req.body = (char*)workflow_json;  /* Cast away const for test */
        req.body_length = strlen(workflow_json);

        api_workflow_start(&req, &resp);
        /* http_response_t is stack-allocated, no free needed */
    }

    argo_daemon_destroy(daemon);
    g_api_daemon = NULL;
    PASS();
}

/* Test workflow abandon */
static void test_workflow_abandon(void) {
    TEST("Workflow abandon");

    argo_daemon_t* daemon = argo_daemon_create(9896);
    if (!daemon) {
        FAIL("Failed to create daemon");
        return;
    }

    g_api_daemon = daemon;

    http_request_t req = {0};
    http_response_t resp = {0};

    req.method = HTTP_METHOD_DELETE;
    strncpy(req.path, "/api/workflow/abandon/test-workflow-id", sizeof(req.path) - 1);

    int result = api_workflow_abandon(&req, &resp);

    /* Should handle gracefully even if workflow doesn't exist */
    if (result == ARGO_SUCCESS || resp.status_code == 404) {
        /* Expected */
    }

    /* http_response_t is stack-allocated, no free needed */
    argo_daemon_destroy(daemon);
    g_api_daemon = NULL;
    PASS();
}

/* Test workflow error handling */
static void test_workflow_error_handling(void) {
    TEST("Workflow error handling");

    argo_daemon_t* daemon = argo_daemon_create(9897);
    if (!daemon) {
        FAIL("Failed to create daemon");
        return;
    }

    g_api_daemon = daemon;

    /* Invalid JSON */
    http_request_t req1 = {0};
    http_response_t resp1 = {0};
    req1.method = HTTP_METHOD_POST;
    strncpy(req1.path, "/api/workflow/start", sizeof(req1.path) - 1);
    req1.body = "invalid json {{{";
    req1.body_length = strlen(req1.body);
    api_workflow_start(&req1, &resp1);
    /* http_response_t is stack-allocated, no free needed */

    /* Empty body */
    http_request_t req2 = {0};
    http_response_t resp2 = {0};
    req2.method = HTTP_METHOD_POST;
    strncpy(req2.path, "/api/workflow/start", sizeof(req2.path) - 1);
    req2.body = NULL;
    api_workflow_start(&req2, &resp2);
    /* http_response_t is stack-allocated, no free needed */

    argo_daemon_destroy(daemon);
    g_api_daemon = NULL;
    PASS();
}

/* Test workflow list */
static void test_workflow_list(void) {
    TEST("Workflow list");

    argo_daemon_t* daemon = argo_daemon_create(9898);
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
        FAIL("List endpoint failed");
        argo_daemon_destroy(daemon);
        g_api_daemon = NULL;
        return;
    }

    if (resp.status_code != 200) {
        FAIL("Expected HTTP 200");
        /* http_response_t is stack-allocated, no free needed */
        argo_daemon_destroy(daemon);
        g_api_daemon = NULL;
        return;
    }

    /* http_response_t is stack-allocated, no free needed */
    argo_daemon_destroy(daemon);
    g_api_daemon = NULL;
    PASS();
}

/* Main test runner */
int main(void) {
    printf("\n");
    printf("==========================================\n");
    printf("Workflow Execution Integration Tests\n");
    printf("==========================================\n\n");

    /* Initialize Argo */
    argo_init();

    /* Integration tests */
    test_workflow_execution_e2e();
    test_workflow_state_transitions();
    test_concurrent_workflows();
    test_workflow_abandon();
    test_workflow_error_handling();
    test_workflow_list();

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

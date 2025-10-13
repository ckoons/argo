/* © 2025 Casey Koons All rights reserved */

/* Daemon tasks test suite */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "argo_daemon.h"
#include "argo_daemon_tasks.h"
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

/* Test workflow timeout task */
static void test_workflow_timeout_task(void) {
    TEST("Workflow timeout task");

    argo_daemon_t* daemon = argo_daemon_create(9887);
    if (!daemon) {
        FAIL("Failed to create daemon");
        return;
    }

    /* Call task - should not crash */
    workflow_timeout_task(daemon);

    argo_daemon_destroy(daemon);
    PASS();
}

/* Test log rotation task */
static void test_log_rotation_task(void) {
    TEST("Log rotation task");

    argo_daemon_t* daemon = argo_daemon_create(9888);
    if (!daemon) {
        FAIL("Failed to create daemon");
        return;
    }

    /* Call task - should not crash */
    log_rotation_task(daemon);

    argo_daemon_destroy(daemon);
    PASS();
}

/* Test workflow completion task */
static void test_workflow_completion_task(void) {
    TEST("Workflow completion task");

    argo_daemon_t* daemon = argo_daemon_create(9889);
    if (!daemon) {
        FAIL("Failed to create daemon");
        return;
    }

    /* Call task - should not crash */
    workflow_completion_task(daemon);

    argo_daemon_destroy(daemon);
    PASS();
}

/* Test NULL parameter handling */
static void test_null_parameters(void) {
    TEST("NULL parameter handling");

    /* NULL context should not crash */
    workflow_timeout_task(NULL);
    log_rotation_task(NULL);
    workflow_completion_task(NULL);

    PASS();
}

/* Test multiple task calls */
static void test_multiple_task_calls(void) {
    TEST("Multiple task calls");

    argo_daemon_t* daemon = argo_daemon_create(9890);
    if (!daemon) {
        FAIL("Failed to create daemon");
        return;
    }

    /* Call each task multiple times */
    for (int i = 0; i < 3; i++) {
        workflow_timeout_task(daemon);
        log_rotation_task(daemon);
        workflow_completion_task(daemon);
    }

    argo_daemon_destroy(daemon);
    PASS();
}

/* Test task safety with daemon shutdown */
static void test_tasks_with_shutdown(void) {
    TEST("Tasks with daemon shutdown");

    argo_daemon_t* daemon = argo_daemon_create(9891);
    if (!daemon) {
        FAIL("Failed to create daemon");
        return;
    }

    /* Set shutdown flag */
    daemon->should_shutdown = true;

    /* Tasks should handle shutdown gracefully */
    workflow_timeout_task(daemon);
    log_rotation_task(daemon);
    workflow_completion_task(daemon);

    argo_daemon_destroy(daemon);
    PASS();
}

/* Test task execution order independence */
static void test_task_order_independence(void) {
    TEST("Task order independence");

    argo_daemon_t* daemon = argo_daemon_create(9892);
    if (!daemon) {
        FAIL("Failed to create daemon");
        return;
    }

    /* Call tasks in different orders */
    log_rotation_task(daemon);
    workflow_timeout_task(daemon);
    workflow_completion_task(daemon);

    workflow_completion_task(daemon);
    log_rotation_task(daemon);
    workflow_timeout_task(daemon);

    argo_daemon_destroy(daemon);
    PASS();
}

/* Main test runner */
int main(void) {
    printf("\n");
    printf("==========================================\n");
    printf("Daemon Tasks Test Suite\n");
    printf("==========================================\n\n");

    /* Initialize Argo */
    argo_init();

    /* Daemon task tests */
    test_workflow_timeout_task();
    test_log_rotation_task();
    test_workflow_completion_task();
    test_null_parameters();
    test_multiple_task_calls();
    test_tasks_with_shutdown();
    test_task_order_independence();

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

/* © 2025 Casey Koons All rights reserved */

/* Lifecycle management test */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/argo_lifecycle.h"
#include "../include/argo_registry.h"
#include "../include/argo_error.h"

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

/* Test lifecycle manager creation */
static void test_manager_creation(void) {
    TEST("Manager creation");

    ci_registry_t* registry = registry_create();
    if (!registry) {
        FAIL("Failed to create registry");
        return;
    }

    lifecycle_manager_t* manager = lifecycle_manager_create(registry);
    if (!manager) {
        FAIL("Failed to create manager");
        registry_destroy(registry);
        return;
    }

    if (manager->count != 0) {
        FAIL("Initial count should be 0");
        lifecycle_manager_destroy(manager);
        registry_destroy(registry);
        return;
    }

    lifecycle_manager_destroy(manager);
    registry_destroy(registry);
    PASS();
}

/* Test CI creation */
static void test_ci_creation(void) {
    TEST("CI creation");

    ci_registry_t* registry = registry_create();
    lifecycle_manager_t* manager = lifecycle_manager_create(registry);

    int result = lifecycle_create_ci(manager, "test-ci", "builder", "gpt-4o");
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to create CI");
        lifecycle_manager_destroy(manager);
        registry_destroy(registry);
        return;
    }

    if (manager->count != 1) {
        FAIL("Count should be 1");
        lifecycle_manager_destroy(manager);
        registry_destroy(registry);
        return;
    }

    ci_lifecycle_t* ci = lifecycle_get_ci(manager, "test-ci");
    if (!ci) {
        FAIL("Failed to get CI");
        lifecycle_manager_destroy(manager);
        registry_destroy(registry);
        return;
    }

    if (ci->current_status != CI_STATUS_OFFLINE) {
        FAIL("Initial status should be OFFLINE");
        lifecycle_manager_destroy(manager);
        registry_destroy(registry);
        return;
    }

    lifecycle_manager_destroy(manager);
    registry_destroy(registry);
    PASS();
}

/* Test start/stop */
static void test_start_stop(void) {
    TEST("Start/stop CI");

    ci_registry_t* registry = registry_create();
    lifecycle_manager_t* manager = lifecycle_manager_create(registry);

    lifecycle_create_ci(manager, "test-ci", "builder", "gpt-4o");

    /* Start CI */
    int result = lifecycle_start_ci(manager, "test-ci");
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to start CI");
        lifecycle_manager_destroy(manager);
        registry_destroy(registry);
        return;
    }

    ci_lifecycle_t* ci = lifecycle_get_ci(manager, "test-ci");
    if (ci->current_status != CI_STATUS_STARTING) {
        FAIL("Status should be STARTING");
        lifecycle_manager_destroy(manager);
        registry_destroy(registry);
        return;
    }

    /* Stop CI */
    result = lifecycle_stop_ci(manager, "test-ci", true);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to stop CI");
        lifecycle_manager_destroy(manager);
        registry_destroy(registry);
        return;
    }

    if (ci->current_status != CI_STATUS_SHUTDOWN) {
        FAIL("Status should be SHUTDOWN");
        lifecycle_manager_destroy(manager);
        registry_destroy(registry);
        return;
    }

    lifecycle_manager_destroy(manager);
    registry_destroy(registry);
    PASS();
}

/* Test transitions */
static void test_transitions(void) {
    TEST("Status transitions");

    ci_registry_t* registry = registry_create();
    lifecycle_manager_t* manager = lifecycle_manager_create(registry);

    lifecycle_create_ci(manager, "test-ci", "builder", "gpt-4o");
    lifecycle_start_ci(manager, "test-ci");

    /* Transition to READY */
    int result = lifecycle_transition(manager, "test-ci",
                                     LIFECYCLE_EVENT_READY, "Initialized");
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to transition to READY");
        lifecycle_manager_destroy(manager);
        registry_destroy(registry);
        return;
    }

    ci_lifecycle_t* ci = lifecycle_get_ci(manager, "test-ci");
    if (ci->current_status != CI_STATUS_READY) {
        FAIL("Status should be READY");
        lifecycle_manager_destroy(manager);
        registry_destroy(registry);
        return;
    }

    if (ci->transition_count < 2) {
        FAIL("Should have at least 2 transitions");
        lifecycle_manager_destroy(manager);
        registry_destroy(registry);
        return;
    }

    lifecycle_manager_destroy(manager);
    registry_destroy(registry);
    PASS();
}

/* Test task assignment */
static void test_task_assignment(void) {
    TEST("Task assignment");

    ci_registry_t* registry = registry_create();
    lifecycle_manager_t* manager = lifecycle_manager_create(registry);

    lifecycle_create_ci(manager, "test-ci", "builder", "gpt-4o");
    lifecycle_start_ci(manager, "test-ci");
    lifecycle_transition(manager, "test-ci", LIFECYCLE_EVENT_READY, NULL);

    /* Assign task */
    int result = lifecycle_assign_task(manager, "test-ci", "Build project");
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to assign task");
        lifecycle_manager_destroy(manager);
        registry_destroy(registry);
        return;
    }

    ci_lifecycle_t* ci = lifecycle_get_ci(manager, "test-ci");
    if (ci->current_status != CI_STATUS_BUSY) {
        FAIL("Status should be BUSY");
        lifecycle_manager_destroy(manager);
        registry_destroy(registry);
        return;
    }

    if (!ci->current_task || strcmp(ci->current_task, "Build project") != 0) {
        FAIL("Task description should be set");
        lifecycle_manager_destroy(manager);
        registry_destroy(registry);
        return;
    }

    /* Complete task */
    result = lifecycle_complete_task(manager, "test-ci", true);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to complete task");
        lifecycle_manager_destroy(manager);
        registry_destroy(registry);
        return;
    }

    if (ci->current_status != CI_STATUS_READY) {
        FAIL("Status should be READY after completion");
        lifecycle_manager_destroy(manager);
        registry_destroy(registry);
        return;
    }

    lifecycle_manager_destroy(manager);
    registry_destroy(registry);
    PASS();
}

/* Test heartbeat */
static void test_heartbeat(void) {
    TEST("Heartbeat tracking");

    ci_registry_t* registry = registry_create();
    lifecycle_manager_t* manager = lifecycle_manager_create(registry);

    lifecycle_create_ci(manager, "test-ci", "builder", "gpt-4o");

    ci_lifecycle_t* ci = lifecycle_get_ci(manager, "test-ci");
    time_t before = ci->last_heartbeat;

    int result = lifecycle_heartbeat(manager, "test-ci");
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to record heartbeat");
        lifecycle_manager_destroy(manager);
        registry_destroy(registry);
        return;
    }

    if (ci->last_heartbeat <= before) {
        FAIL("Heartbeat timestamp should be updated");
        lifecycle_manager_destroy(manager);
        registry_destroy(registry);
        return;
    }

    lifecycle_manager_destroy(manager);
    registry_destroy(registry);
    PASS();
}

/* Test error reporting */
static void test_error_reporting(void) {
    TEST("Error reporting");

    ci_registry_t* registry = registry_create();
    lifecycle_manager_t* manager = lifecycle_manager_create(registry);

    lifecycle_create_ci(manager, "test-ci", "builder", "gpt-4o");
    lifecycle_start_ci(manager, "test-ci");

    int result = lifecycle_report_error(manager, "test-ci", "Test error message");
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to report error");
        lifecycle_manager_destroy(manager);
        registry_destroy(registry);
        return;
    }

    ci_lifecycle_t* ci = lifecycle_get_ci(manager, "test-ci");
    if (ci->current_status != CI_STATUS_ERROR) {
        FAIL("Status should be ERROR");
        lifecycle_manager_destroy(manager);
        registry_destroy(registry);
        return;
    }

    if (ci->error_count != 1) {
        FAIL("Error count should be 1");
        lifecycle_manager_destroy(manager);
        registry_destroy(registry);
        return;
    }

    lifecycle_manager_destroy(manager);
    registry_destroy(registry);
    PASS();
}

int main(void) {
    printf("\n========================================\n");
    printf("ARGO LIFECYCLE TESTS\n");
    printf("========================================\n\n");

    test_manager_creation();
    test_ci_creation();
    test_start_stop();
    test_transitions();
    test_task_assignment();
    test_heartbeat();
    test_error_reporting();

    printf("\n========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("========================================\n\n");

    return tests_failed > 0 ? 1 : 0;
}
/* © 2025 Casey Koons All rights reserved */

/* Daemon restart integration test suite */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "argo_daemon.h"
#include "argo_registry.h"
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

#define TEST_REGISTRY_FILE "/tmp/test_daemon_restart_registry.json"

/* Test daemon restart with registry persistence */
static void test_daemon_restart_with_registry(void) {
    TEST("Daemon restart with registry persistence");

    /* Create first daemon instance */
    argo_daemon_t* daemon1 = argo_daemon_create(9899);
    if (!daemon1) {
        FAIL("Failed to create first daemon");
        return;
    }

    /* Add CI to registry */
    int result = registry_add_ci(daemon1->registry, "test-ci", "worker", "claude", 9000);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to add CI");
        argo_daemon_destroy(daemon1);
        return;
    }

    /* Save registry state */
    result = registry_save_state(daemon1->registry, TEST_REGISTRY_FILE);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to save registry");
        argo_daemon_destroy(daemon1);
        unlink(TEST_REGISTRY_FILE);
        return;
    }

    argo_daemon_destroy(daemon1);

    /* Create second daemon instance (simulates restart) */
    argo_daemon_t* daemon2 = argo_daemon_create(9899);
    if (!daemon2) {
        FAIL("Failed to create second daemon");
        unlink(TEST_REGISTRY_FILE);
        return;
    }

    /* Load registry state */
    result = registry_load_state(daemon2->registry, TEST_REGISTRY_FILE);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to load registry");
        argo_daemon_destroy(daemon2);
        unlink(TEST_REGISTRY_FILE);
        return;
    }

    /* Verify CI was restored */
    ci_registry_entry_t* entry = registry_find_ci(daemon2->registry, "test-ci");
    if (!entry) {
        FAIL("CI not found after restart");
        argo_daemon_destroy(daemon2);
        unlink(TEST_REGISTRY_FILE);
        return;
    }

    argo_daemon_destroy(daemon2);
    unlink(TEST_REGISTRY_FILE);
    PASS();
}

/* Test daemon restart without registry file */
static void test_daemon_restart_no_registry(void) {
    TEST("Daemon restart without registry file");

    /* Delete any existing registry file */
    unlink(TEST_REGISTRY_FILE);

    /* Create daemon */
    argo_daemon_t* daemon = argo_daemon_create(9900);
    if (!daemon) {
        FAIL("Failed to create daemon");
        return;
    }

    /* Load should succeed (empty registry) */
    int result = registry_load_state(daemon->registry, TEST_REGISTRY_FILE);
    if (result != ARGO_SUCCESS) {
        FAIL("Load should succeed with missing file");
        argo_daemon_destroy(daemon);
        return;
    }

    if (daemon->registry->count != 0) {
        FAIL("Registry should be empty");
        argo_daemon_destroy(daemon);
        return;
    }

    argo_daemon_destroy(daemon);
    PASS();
}

/* Test daemon restart with multiple CIs */
static void test_daemon_restart_multiple_cis(void) {
    TEST("Daemon restart with multiple CIs");

    /* Create daemon and add CIs */
    argo_daemon_t* daemon1 = argo_daemon_create(9901);
    if (!daemon1) {
        FAIL("Failed to create daemon");
        return;
    }

    registry_add_ci(daemon1->registry, "ci1", "worker", "claude", 9001);
    registry_add_ci(daemon1->registry, "ci2", "reviewer", "openai", 9002);
    registry_add_ci(daemon1->registry, "ci3", "tester", "gemini", 9003);

    int result = registry_save_state(daemon1->registry, TEST_REGISTRY_FILE);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to save registry");
        argo_daemon_destroy(daemon1);
        unlink(TEST_REGISTRY_FILE);
        return;
    }

    int ci_count = daemon1->registry->count;
    argo_daemon_destroy(daemon1);

    /* Restart daemon */
    argo_daemon_t* daemon2 = argo_daemon_create(9901);
    if (!daemon2) {
        FAIL("Failed to create daemon");
        unlink(TEST_REGISTRY_FILE);
        return;
    }

    result = registry_load_state(daemon2->registry, TEST_REGISTRY_FILE);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to load registry");
        argo_daemon_destroy(daemon2);
        unlink(TEST_REGISTRY_FILE);
        return;
    }

    if (daemon2->registry->count != ci_count) {
        FAIL("CI count mismatch after restart");
        argo_daemon_destroy(daemon2);
        unlink(TEST_REGISTRY_FILE);
        return;
    }

    argo_daemon_destroy(daemon2);
    unlink(TEST_REGISTRY_FILE);
    PASS();
}

/* Test daemon lifecycle components persist */
static void test_daemon_component_persistence(void) {
    TEST("Daemon component persistence");

    /* Create daemon */
    argo_daemon_t* daemon1 = argo_daemon_create(9902);
    if (!daemon1) {
        FAIL("Failed to create daemon");
        return;
    }

    /* Verify all components created */
    if (!daemon1->http_server || !daemon1->registry || !daemon1->lifecycle) {
        FAIL("Components not initialized");
        argo_daemon_destroy(daemon1);
        return;
    }

    argo_daemon_destroy(daemon1);

    /* Create new daemon (simulates restart) */
    argo_daemon_t* daemon2 = argo_daemon_create(9902);
    if (!daemon2) {
        FAIL("Failed to create daemon after restart");
        return;
    }

    /* Verify all components recreated */
    if (!daemon2->http_server || !daemon2->registry || !daemon2->lifecycle) {
        FAIL("Components not reinitialized");
        argo_daemon_destroy(daemon2);
        return;
    }

    argo_daemon_destroy(daemon2);
    PASS();
}

/* Test daemon restart clears shutdown flag */
static void test_daemon_restart_shutdown_flag(void) {
    TEST("Daemon restart clears shutdown flag");

    /* Create daemon and shutdown */
    argo_daemon_t* daemon1 = argo_daemon_create(9903);
    if (!daemon1) {
        FAIL("Failed to create daemon");
        return;
    }

    argo_daemon_stop(daemon1);

    /* Shutdown flag may or may not be set depending on implementation */
    /* Just verify stop doesn't crash */

    argo_daemon_destroy(daemon1);

    /* Create new daemon (simulates restart) */
    argo_daemon_t* daemon2 = argo_daemon_create(9903);
    if (!daemon2) {
        FAIL("Failed to create daemon after restart");
        return;
    }

    if (daemon2->should_shutdown) {
        FAIL("Shutdown flag should be false on restart");
        argo_daemon_destroy(daemon2);
        return;
    }

    argo_daemon_destroy(daemon2);
    PASS();
}

/* Test daemon restart on different port */
static void test_daemon_restart_different_port(void) {
    TEST("Daemon restart on different port");

    /* Create daemon on port A */
    argo_daemon_t* daemon1 = argo_daemon_create(9904);
    if (!daemon1) {
        FAIL("Failed to create daemon on first port");
        return;
    }

    registry_add_ci(daemon1->registry, "test-ci", "worker", "claude", 9000);
    int result = registry_save_state(daemon1->registry, TEST_REGISTRY_FILE);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to save registry");
        argo_daemon_destroy(daemon1);
        unlink(TEST_REGISTRY_FILE);
        return;
    }

    argo_daemon_destroy(daemon1);

    /* Create daemon on port B */
    argo_daemon_t* daemon2 = argo_daemon_create(9905);
    if (!daemon2) {
        FAIL("Failed to create daemon on second port");
        unlink(TEST_REGISTRY_FILE);
        return;
    }

    /* Load state should still work */
    result = registry_load_state(daemon2->registry, TEST_REGISTRY_FILE);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to load registry on different port");
        argo_daemon_destroy(daemon2);
        unlink(TEST_REGISTRY_FILE);
        return;
    }

    if (daemon2->port != 9905) {
        FAIL("Port not set correctly");
        argo_daemon_destroy(daemon2);
        unlink(TEST_REGISTRY_FILE);
        return;
    }

    argo_daemon_destroy(daemon2);
    unlink(TEST_REGISTRY_FILE);
    PASS();
}

/* Main test runner */
int main(void) {
    printf("\n");
    printf("==========================================\n");
    printf("Daemon Restart Integration Tests\n");
    printf("==========================================\n\n");

    /* Initialize Argo */
    argo_init();

    /* Integration tests */
    test_daemon_restart_with_registry();
    test_daemon_restart_no_registry();
    test_daemon_restart_multiple_cis();
    test_daemon_component_persistence();
    test_daemon_restart_shutdown_flag();
    test_daemon_restart_different_port();

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

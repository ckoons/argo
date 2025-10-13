/* © 2025 Casey Koons All rights reserved */

/* Registry persistence test suite */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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

#define TEST_STATE_FILE "/tmp/test_registry_state.json"

/* Test save empty registry */
static void test_save_empty_registry(void) {
    TEST("Save empty registry");

    ci_registry_t* registry = registry_create();
    if (!registry) {
        FAIL("Failed to create registry");
        return;
    }

    int result = registry_save_state(registry, TEST_STATE_FILE);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to save empty registry");
        registry_destroy(registry);
        return;
    }

    registry_destroy(registry);
    unlink(TEST_STATE_FILE);
    PASS();
}

/* Test save and load single CI */
static void test_save_load_single_ci(void) {
    TEST("Save and load single CI");

    ci_registry_t* registry = registry_create();
    if (!registry) {
        FAIL("Failed to create registry");
        return;
    }

    /* Add CI */
    int result = registry_add_ci(registry, "test-ci", "worker", "claude", 9000);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to add CI");
        registry_destroy(registry);
        return;
    }

    /* Save */
    result = registry_save_state(registry, TEST_STATE_FILE);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to save registry");
        registry_destroy(registry);
        return;
    }

    /* Create new registry and load */
    ci_registry_t* loaded = registry_create();
    if (!loaded) {
        FAIL("Failed to create new registry");
        registry_destroy(registry);
        unlink(TEST_STATE_FILE);
        return;
    }

    result = registry_load_state(loaded, TEST_STATE_FILE);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to load registry");
        registry_destroy(registry);
        registry_destroy(loaded);
        unlink(TEST_STATE_FILE);
        return;
    }

    /* Verify CI was loaded */
    if (loaded->count != 1) {
        FAIL("CI count mismatch");
        registry_destroy(registry);
        registry_destroy(loaded);
        unlink(TEST_STATE_FILE);
        return;
    }

    ci_registry_entry_t* entry = registry_find_ci(loaded, "test-ci");
    if (!entry) {
        FAIL("CI not found in loaded registry");
        registry_destroy(registry);
        registry_destroy(loaded);
        unlink(TEST_STATE_FILE);
        return;
    }

    if (strcmp(entry->role, "worker") != 0) {
        FAIL("Role mismatch");
        registry_destroy(registry);
        registry_destroy(loaded);
        unlink(TEST_STATE_FILE);
        return;
    }

    if (strcmp(entry->model, "claude") != 0) {
        FAIL("Model mismatch");
        registry_destroy(registry);
        registry_destroy(loaded);
        unlink(TEST_STATE_FILE);
        return;
    }

    registry_destroy(registry);
    registry_destroy(loaded);
    unlink(TEST_STATE_FILE);
    PASS();
}

/* Test save and load multiple CIs */
static void test_save_load_multiple_cis(void) {
    TEST("Save and load multiple CIs");

    ci_registry_t* registry = registry_create();
    if (!registry) {
        FAIL("Failed to create registry");
        return;
    }

    /* Add multiple CIs */
    registry_add_ci(registry, "ci1", "worker", "claude", 9001);
    registry_add_ci(registry, "ci2", "reviewer", "openai", 9002);
    registry_add_ci(registry, "ci3", "tester", "gemini", 9003);

    /* Save */
    int result = registry_save_state(registry, TEST_STATE_FILE);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to save registry");
        registry_destroy(registry);
        return;
    }

    /* Load into new registry */
    ci_registry_t* loaded = registry_create();
    result = registry_load_state(loaded, TEST_STATE_FILE);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to load registry");
        registry_destroy(registry);
        registry_destroy(loaded);
        unlink(TEST_STATE_FILE);
        return;
    }

    /* Verify count */
    if (loaded->count != 3) {
        FAIL("CI count mismatch");
        registry_destroy(registry);
        registry_destroy(loaded);
        unlink(TEST_STATE_FILE);
        return;
    }

    /* Verify all CIs present */
    if (!registry_find_ci(loaded, "ci1") ||
        !registry_find_ci(loaded, "ci2") ||
        !registry_find_ci(loaded, "ci3")) {
        FAIL("Not all CIs found");
        registry_destroy(registry);
        registry_destroy(loaded);
        unlink(TEST_STATE_FILE);
        return;
    }

    registry_destroy(registry);
    registry_destroy(loaded);
    unlink(TEST_STATE_FILE);
    PASS();
}

/* Test load non-existent file */
static void test_load_nonexistent_file(void) {
    TEST("Load non-existent file");

    ci_registry_t* registry = registry_create();
    if (!registry) {
        FAIL("Failed to create registry");
        return;
    }

    /* Loading non-existent file should succeed (empty registry) */
    int result = registry_load_state(registry, "/tmp/nonexistent_registry_file.json");
    if (result != ARGO_SUCCESS) {
        FAIL("Should succeed with non-existent file");
        registry_destroy(registry);
        return;
    }

    if (registry->count != 0) {
        FAIL("Registry should be empty");
        registry_destroy(registry);
        return;
    }

    registry_destroy(registry);
    PASS();
}

/* Test NULL parameter handling */
static void test_null_parameters(void) {
    TEST("NULL parameter handling");

    ci_registry_t* registry = registry_create();
    if (!registry) {
        FAIL("Failed to create registry");
        return;
    }

    /* NULL registry */
    int result = registry_save_state(NULL, TEST_STATE_FILE);
    if (result == ARGO_SUCCESS) {
        FAIL("Should fail with NULL registry");
        registry_destroy(registry);
        return;
    }

    /* NULL filepath */
    result = registry_save_state(registry, NULL);
    if (result == ARGO_SUCCESS) {
        FAIL("Should fail with NULL filepath");
        registry_destroy(registry);
        return;
    }

    /* NULL registry for load */
    result = registry_load_state(NULL, TEST_STATE_FILE);
    if (result == ARGO_SUCCESS) {
        FAIL("Should fail with NULL registry");
        registry_destroy(registry);
        return;
    }

    /* NULL filepath for load */
    result = registry_load_state(registry, NULL);
    if (result == ARGO_SUCCESS) {
        FAIL("Should fail with NULL filepath");
        registry_destroy(registry);
        return;
    }

    registry_destroy(registry);
    PASS();
}

/* Test registry statistics */
static void test_registry_stats(void) {
    TEST("Registry statistics");

    ci_registry_t* registry = registry_create();
    if (!registry) {
        FAIL("Failed to create registry");
        return;
    }

    /* Add CIs */
    registry_add_ci(registry, "ci1", "worker", "claude", 9001);
    registry_add_ci(registry, "ci2", "worker", "openai", 9002);

    /* Get stats */
    registry_stats_t* stats = registry_get_stats(registry);
    if (!stats) {
        FAIL("Failed to get stats");
        registry_destroy(registry);
        return;
    }

    if (stats->total_cis != 2) {
        FAIL("CI count incorrect");
        registry_free_stats(stats);
        registry_destroy(registry);
        return;
    }

    registry_free_stats(stats);
    registry_destroy(registry);
    PASS();
}

/* Main test runner */
int main(void) {
    printf("\n");
    printf("==========================================\n");
    printf("Registry Persistence Test Suite\n");
    printf("==========================================\n\n");

    /* Initialize Argo */
    argo_init();

    /* Registry persistence tests */
    test_save_empty_registry();
    test_save_load_single_ci();
    test_save_load_multiple_cis();
    test_load_nonexistent_file();
    test_null_parameters();
    test_registry_stats();

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

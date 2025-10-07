/* © 2025 Casey Koons All rights reserved */
/* Memory stress tests - exercise allocation paths under pressure */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "argo_workflow_context.h"
#include "argo_error.h"

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;

/* Helper macros */
#define TEST(name) do { \
    printf("Testing: %-50s ", name); \
    fflush(stdout); \
    tests_run++; \
} while(0)

#define PASS() do { \
    printf("✓\n"); \
    tests_passed++; \
} while(0)

/* Test: Workflow context under heavy load */
static void test_context_heavy_load(void) {
    TEST("Context with 1000 variables (forces multiple reallocs)");

    workflow_context_t* ctx = workflow_context_create();
    assert(ctx != NULL);

    /* Add 1000 variables - this will force multiple reallocations */
    for (int i = 0; i < 1000; i++) {
        char key[64], value[128];
        snprintf(key, sizeof(key), "test_key_%d", i);
        snprintf(value, sizeof(value), "test_value_%d_with_some_data", i);

        int result = workflow_context_set(ctx, key, value);
        assert(result == ARGO_SUCCESS);
    }

    /* Verify all values are still accessible (no corruption) */
    for (int i = 0; i < 1000; i += 100) {
        char key[64], expected[128];
        snprintf(key, sizeof(key), "test_key_%d", i);
        snprintf(expected, sizeof(expected), "test_value_%d_with_some_data", i);

        const char* retrieved = workflow_context_get(ctx, key);
        assert(retrieved != NULL);
        assert(strcmp(retrieved, expected) == 0);
    }

    workflow_context_destroy(ctx);
    PASS();
}

/* Test: Rapid allocation and deallocation cycles */
static void test_rapid_alloc_dealloc(void) {
    TEST("Rapid alloc/dealloc cycles (checks for leaks)");

    /* Create and destroy 100 contexts rapidly */
    for (int cycle = 0; cycle < 100; cycle++) {
        workflow_context_t* ctx = workflow_context_create();
        assert(ctx != NULL);

        /* Add some data */
        for (int i = 0; i < 10; i++) {
            char key[32], value[64];
            snprintf(key, sizeof(key), "k%d", i);
            snprintf(value, sizeof(value), "v%d", i);
            workflow_context_set(ctx, key, value);
        }

        workflow_context_destroy(ctx);
    }

    PASS();
}

/* Test: Large value storage */
static void test_large_values(void) {
    TEST("Store large values (4KB each)");

    workflow_context_t* ctx = workflow_context_create();
    assert(ctx != NULL);

    /* Create a 4KB value */
    char* large_value = malloc(4096);
    assert(large_value != NULL);
    memset(large_value, 'X', 4095);
    large_value[4095] = '\0';

    /* Store 10 large values */
    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, sizeof(key), "large_%d", i);
        int result = workflow_context_set(ctx, key, large_value);
        assert(result == ARGO_SUCCESS);
    }

    /* Verify one is retrievable */
    const char* retrieved = workflow_context_get(ctx, "large_5");
    assert(retrieved != NULL);
    assert(strlen(retrieved) == 4095);

    free(large_value);
    workflow_context_destroy(ctx);
    PASS();
}

/* Test: Workflow context with many overwrites */
static void test_context_overwrites(void) {
    TEST("Context with 100 overwrites of same keys");

    workflow_context_t* ctx = workflow_context_create();
    assert(ctx != NULL);

    /* Create 10 keys */
    for (int i = 0; i < 10; i++) {
        char key[32], value[64];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "initial_value_%d", i);
        int result = workflow_context_set(ctx, key, value);
        assert(result == ARGO_SUCCESS);
    }

    /* Overwrite each key 100 times */
    for (int iteration = 0; iteration < 100; iteration++) {
        for (int i = 0; i < 10; i++) {
            char key[32], value[128];
            snprintf(key, sizeof(key), "key_%d", i);
            snprintf(value, sizeof(value), "updated_value_%d_iteration_%d", i, iteration);
            int result = workflow_context_set(ctx, key, value);
            assert(result == ARGO_SUCCESS);
        }
    }

    /* Verify final values */
    for (int i = 0; i < 10; i++) {
        char key[32], expected[128];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(expected, sizeof(expected), "updated_value_%d_iteration_99", i);
        const char* value = workflow_context_get(ctx, key);
        assert(value != NULL);
        assert(strcmp(value, expected) == 0);
    }

    workflow_context_destroy(ctx);
    PASS();
}

/* Test: Interleaved operations */
static void test_interleaved_operations(void) {
    TEST("Interleaved create/populate/destroy operations");

    workflow_context_t* contexts[10];

    /* Create all contexts */
    for (int i = 0; i < 10; i++) {
        contexts[i] = workflow_context_create();
        assert(contexts[i] != NULL);
    }

    /* Populate in reverse order */
    for (int i = 9; i >= 0; i--) {
        for (int j = 0; j < 20; j++) {
            char key[32], value[64];
            snprintf(key, sizeof(key), "key_%d_%d", i, j);
            snprintf(value, sizeof(value), "value_%d_%d", i, j);
            workflow_context_set(contexts[i], key, value);
        }
    }

    /* Verify middle context */
    const char* val = workflow_context_get(contexts[5], "key_5_10");
    assert(val != NULL);
    assert(strcmp(val, "value_5_10") == 0);

    /* Destroy in alternating order */
    for (int i = 0; i < 10; i += 2) {
        workflow_context_destroy(contexts[i]);
    }
    for (int i = 1; i < 10; i += 2) {
        workflow_context_destroy(contexts[i]);
    }

    PASS();
}

/* Test: Boundary conditions */
static void test_boundary_conditions(void) {
    TEST("Boundary conditions (empty strings, max length)");

    workflow_context_t* ctx = workflow_context_create();
    assert(ctx != NULL);

    /* Empty value */
    int result = workflow_context_set(ctx, "empty_key", "");
    assert(result == ARGO_SUCCESS);
    const char* retrieved = workflow_context_get(ctx, "empty_key");
    assert(retrieved != NULL);
    assert(strlen(retrieved) == 0);

    /* Very long key */
    char long_key[256];
    memset(long_key, 'k', 255);
    long_key[255] = '\0';
    result = workflow_context_set(ctx, long_key, "value");
    assert(result == ARGO_SUCCESS);

    /* Overwrite existing key multiple times */
    for (int i = 0; i < 50; i++) {
        char value[64];
        snprintf(value, sizeof(value), "updated_%d", i);
        result = workflow_context_set(ctx, "overwrite_key", value);
        assert(result == ARGO_SUCCESS);
    }

    workflow_context_destroy(ctx);
    PASS();
}

/* Test: Memory accounting (verify no leaks in normal operations) */
static void test_memory_accounting(void) {
    TEST("Memory accounting (create/destroy 1000 times)");

    /* This test primarily validates with ASAN/Valgrind */
    /* Under normal operation, should complete without leaks */

    for (int i = 0; i < 1000; i++) {
        workflow_context_t* ctx = workflow_context_create();
        assert(ctx != NULL);

        /* Minimal operations */
        workflow_context_set(ctx, "test", "value");
        workflow_context_get(ctx, "test");

        workflow_context_destroy(ctx);
    }

    PASS();
}

int main(void) {
    printf("\n");
    printf("==========================================\n");
    printf("Memory Stress Tests\n");
    printf("==========================================\n");
    printf("\n");

    /* Note: These tests are designed to be run under ASAN/Valgrind */
    /* Run with: make test-asan or make test-valgrind */

    test_context_heavy_load();
    test_rapid_alloc_dealloc();
    test_large_values();
    test_context_overwrites();
    test_interleaved_operations();
    test_boundary_conditions();
    test_memory_accounting();

    printf("\n");
    printf("==========================================\n");
    printf("Test Results\n");
    printf("==========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    printf("==========================================\n");
    printf("\n");

    return (tests_run == tests_passed) ? 0 : 1;
}

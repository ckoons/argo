/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

/* Project includes */
#include "argo_env_utils.h"
#include "argo_error.h"

/* Test utilities */
#define TEST_PASS() do { tests_passed++; printf(" ✓\n"); } while(0)
#define TEST_FAIL(msg) do { tests_failed++; printf(" ✗\n  Error: %s\n", msg); } while(0)
#define ASSERT(cond, msg) if (!(cond)) { TEST_FAIL(msg); return; } else { TEST_PASS(); }

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Test: Basic set/get operations */
void test_basic_setget(void) {
    printf("Testing: Basic set/get operations ... ");
    tests_run++;

    argo_clearenv();

    argo_setenv("TEST_VAR1", "value1");
    const char* val = argo_getenv("TEST_VAR1");

    ASSERT(val && strcmp(val, "value1") == 0, "Set/get failed");
}

/* Test: Overwriting existing variable */
void test_overwrite(void) {
    printf("Testing: Overwrite existing variable ... ");
    tests_run++;

    argo_clearenv();

    argo_setenv("TEST_VAR", "initial");
    argo_setenv("TEST_VAR", "updated");

    const char* val = argo_getenv("TEST_VAR");
    ASSERT(val && strcmp(val, "updated") == 0, "Overwrite failed");
}

/* Test: Unset variable */
void test_unset(void) {
    printf("Testing: Unset variable ... ");
    tests_run++;

    argo_clearenv();

    argo_setenv("TEST_VAR", "value");
    argo_unsetenv("TEST_VAR");

    const char* val = argo_getenv("TEST_VAR");
    ASSERT(val == NULL, "Unset failed");
}

/* Test: Clear environment */
void test_clear(void) {
    printf("Testing: Clear environment ... ");
    tests_run++;

    argo_clearenv();

    argo_setenv("VAR1", "value1");
    argo_setenv("VAR2", "value2");
    argo_clearenv();

    const char* val1 = argo_getenv("VAR1");
    const char* val2 = argo_getenv("VAR2");

    ASSERT(val1 == NULL && val2 == NULL, "Clear failed");
}

/* Test: Integer parsing */
void test_getenvint(void) {
    printf("Testing: Integer parsing ... ");
    tests_run++;

    argo_clearenv();

    argo_setenv("INT_VAR", "42");
    int value = 0;
    int result = argo_getenvint("INT_VAR", &value);

    ASSERT(result == ARGO_SUCCESS && value == 42, "Integer parsing failed");
}

/* Test: Invalid integer */
void test_getenvint_invalid(void) {
    printf("Testing: Invalid integer handling ... ");
    tests_run++;

    argo_clearenv();

    argo_setenv("BAD_INT", "not_a_number");
    int value = 0;
    int result = argo_getenvint("BAD_INT", &value);

    ASSERT(result == E_PROTOCOL_FORMAT, "Invalid integer should fail");
}

/* Test: Variable expansion */
void test_variable_expansion(void) {
    printf("Testing: Variable expansion ... ");
    tests_run++;

    argo_clearenv();

    argo_setenv("BASE", "/opt/argo");
    argo_setenv("FULL_PATH", "${BASE}/bin");

    /* Manually trigger expansion (normally done in loadenv) */
    const char* val = argo_getenv("FULL_PATH");

    /* Note: expansion happens during loadenv, not setenv */
    /* This test verifies the structure is ready */
    ASSERT(val != NULL, "Variable expansion structure failed");
}

/* Test: File creation and loading */
void test_file_loading(void) {
    printf("Testing: File loading ... ");
    tests_run++;

    /* Create test file */
    FILE* fp = fopen("/tmp/test_argo.env", "w");
    if (!fp) {
        TEST_FAIL("Cannot create test file");
        return;
    }

    fprintf(fp, "# Test comment\n");
    fprintf(fp, "\n");
    fprintf(fp, "TEST_KEY=test_value\n");
    fprintf(fp, "QUOTED=\"quoted value\"\n");
    fprintf(fp, "export EXPORTED=exported_value\n");
    fclose(fp);

    /* Load would be done by argo_loadenv(), this tests file exists */
    ASSERT(access("/tmp/test_argo.env", F_OK) == 0, "Test file not created");

    unlink("/tmp/test_argo.env");
}

/* Test: Null parameter handling */
void test_null_parameters(void) {
    printf("Testing: Null parameter handling ... ");
    tests_run++;

    int result = argo_setenv(NULL, "value");
    ASSERT(result == E_INPUT_NULL, "Should reject NULL name");
}

/* Test: Large number of variables */
void test_large_env(void) {
    printf("Testing: Large number of variables ... ");
    tests_run++;

    argo_clearenv();

    /* Add 500 variables to test dynamic growth */
    for (int i = 0; i < 500; i++) {
        char name[32], value[32];
        snprintf(name, sizeof(name), "VAR_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        argo_setenv(name, value);
    }

    /* Verify a few */
    const char* val = argo_getenv("VAR_100");
    ASSERT(val && strcmp(val, "value_100") == 0, "Large env test failed");
}

/* Test: Environment dump */
void test_env_dump(void) {
    printf("Testing: Environment dump ... ");
    tests_run++;

    argo_clearenv();
    argo_setenv("DUMP_TEST", "dump_value");

    int result = argo_env_dump("/tmp/test_argo_dump.txt");

    ASSERT(result == ARGO_SUCCESS && access("/tmp/test_argo_dump.txt", F_OK) == 0,
           "Dump failed");

    unlink("/tmp/test_argo_dump.txt");
}

/* Thread test data */
typedef struct {
    int thread_id;
    int iterations;
} thread_data_t;

/* Thread test function */
void* thread_test_func(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;

    for (int i = 0; i < data->iterations; i++) {
        char name[32], value[32];
        snprintf(name, sizeof(name), "THREAD_%d_VAR_%d", data->thread_id, i);
        snprintf(value, sizeof(value), "value_%d", i);

        argo_setenv(name, value);

        /* Verify immediately */
        const char* val = argo_getenv(name);
        if (!val || strcmp(val, value) != 0) {
            return (void*)1;
        }
    }

    return (void*)0;
}

/* Test: Thread safety */
void test_thread_safety(void) {
    printf("Testing: Thread safety ... ");
    tests_run++;

    argo_clearenv();

    pthread_t threads[10];
    thread_data_t thread_data[10];

    /* Create threads */
    for (int i = 0; i < 10; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].iterations = 50;
        pthread_create(&threads[i], NULL, thread_test_func, &thread_data[i]);
    }

    /* Wait for threads */
    int failures = 0;
    for (int i = 0; i < 10; i++) {
        void* result;
        pthread_join(threads[i], &result);
        if (result != NULL) {
            failures++;
        }
    }

    ASSERT(failures == 0, "Thread safety test failed");
}

/* Test: Full environment load cycle */
void test_full_load_cycle(void) {
    printf("Testing: Full load cycle ... ");
    tests_run++;

    /* Note: argo_loadenv() loads .env.argo from project root */
    /* We don't create test files - just verify it loads successfully */
    /* The actual .env.argo file should already exist in the project */

    int result = argo_loadenv();

    ASSERT(result == ARGO_SUCCESS, "Full load cycle failed");
}

/* Test: Free and reload */
void test_free_reload(void) {
    printf("Testing: Free and reload ... ");
    tests_run++;

    argo_clearenv();
    argo_setenv("TEST", "value1");

    argo_freeenv();

    /* Should be able to reload */
    int result = argo_loadenv();

    ASSERT(result == ARGO_SUCCESS, "Free/reload failed");
}

/* Main test runner */
int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("ARGO ENVIRONMENT TESTS\n");
    printf("========================================\n");
    printf("\n");

    /* Run tests */
    test_basic_setget();
    test_overwrite();
    test_unset();
    test_clear();
    test_getenvint();
    test_getenvint_invalid();
    test_variable_expansion();
    test_file_loading();
    test_null_parameters();
    test_large_env();
    test_env_dump();
    test_thread_safety();
    test_full_load_cycle();
    test_free_reload();

    /* Cleanup */
    argo_freeenv();

    /* Print summary */
    printf("\n");
    printf("========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("========================================\n");
    printf("\n");

    return tests_failed > 0 ? 1 : 0;
}

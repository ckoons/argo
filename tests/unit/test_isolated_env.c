/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/* Project includes */
#include "argo_env.h"
#include "argo_error.h"

/* Test utilities */
#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1; \
        } \
    } while(0)

#define TEST_PASS(message) \
    do { \
        printf("PASS: %s\n", message); \
        return 0; \
    } while(0)

/* Test: Create and destroy environment */
static int test_env_create_destroy(void) {
    argo_env_t* env = argo_env_create();
    TEST_ASSERT(env != NULL, "Should create environment");
    TEST_ASSERT(argo_env_size(env) == 0, "Should be empty initially");

    argo_env_destroy(env);
    TEST_PASS("Create and destroy works");
}

/* Test: Set and get variables */
static int test_env_set_get(void) {
    argo_env_t* env = argo_env_create();
    TEST_ASSERT(env != NULL, "Should create environment");

    /* Set variable */
    int result = argo_env_set(env, "TEST_KEY", "test_value");
    TEST_ASSERT(result == ARGO_SUCCESS, "Should set variable");
    TEST_ASSERT(argo_env_size(env) == 1, "Should have 1 variable");

    /* Get variable */
    const char* value = argo_env_get(env, "TEST_KEY");
    TEST_ASSERT(value != NULL, "Should find variable");
    TEST_ASSERT(strcmp(value, "test_value") == 0, "Value should match");

    argo_env_destroy(env);
    TEST_PASS("Set and get works");
}

/* Test: Update existing variable */
static int test_env_update(void) {
    argo_env_t* env = argo_env_create();
    TEST_ASSERT(env != NULL, "Should create environment");

    /* Set initial value */
    argo_env_set(env, "PATH", "/usr/bin");
    TEST_ASSERT(argo_env_size(env) == 1, "Should have 1 variable");

    /* Update value */
    argo_env_set(env, "PATH", "/usr/bin:/bin");
    TEST_ASSERT(argo_env_size(env) == 1, "Should still have 1 variable");

    /* Verify updated value */
    const char* value = argo_env_get(env, "PATH");
    TEST_ASSERT(strcmp(value, "/usr/bin:/bin") == 0, "Should have updated value");

    argo_env_destroy(env);
    TEST_PASS("Update existing variable works");
}

/* Test: Multiple variables */
static int test_env_multiple(void) {
    argo_env_t* env = argo_env_create();
    TEST_ASSERT(env != NULL, "Should create environment");

    /* Set multiple variables */
    argo_env_set(env, "VAR1", "value1");
    argo_env_set(env, "VAR2", "value2");
    argo_env_set(env, "VAR3", "value3");
    TEST_ASSERT(argo_env_size(env) == 3, "Should have 3 variables");

    /* Verify all values */
    TEST_ASSERT(strcmp(argo_env_get(env, "VAR1"), "value1") == 0, "VAR1 should match");
    TEST_ASSERT(strcmp(argo_env_get(env, "VAR2"), "value2") == 0, "VAR2 should match");
    TEST_ASSERT(strcmp(argo_env_get(env, "VAR3"), "value3") == 0, "VAR3 should match");

    argo_env_destroy(env);
    TEST_PASS("Multiple variables work");
}

/* Test: Missing variable */
static int test_env_missing(void) {
    argo_env_t* env = argo_env_create();
    TEST_ASSERT(env != NULL, "Should create environment");

    argo_env_set(env, "EXISTS", "yes");

    /* Get missing variable */
    const char* value = argo_env_get(env, "DOES_NOT_EXIST");
    TEST_ASSERT(value == NULL, "Missing variable should return NULL");

    argo_env_destroy(env);
    TEST_PASS("Missing variable handling works");
}

/* Test: Convert to envp */
static int test_env_to_envp(void) {
    argo_env_t* env = argo_env_create();
    TEST_ASSERT(env != NULL, "Should create environment");

    /* Add variables */
    argo_env_set(env, "VAR1", "value1");
    argo_env_set(env, "VAR2", "value2");

    /* Convert to envp */
    char** envp = argo_env_to_envp(env);
    TEST_ASSERT(envp != NULL, "Should create envp array");

    /* Verify format */
    int found1 = 0, found2 = 0;
    for (int i = 0; envp[i] != NULL; i++) {
        if (strcmp(envp[i], "VAR1=value1") == 0) found1 = 1;
        if (strcmp(envp[i], "VAR2=value2") == 0) found2 = 1;
    }
    TEST_ASSERT(found1, "Should find VAR1=value1");
    TEST_ASSERT(found2, "Should find VAR2=value2");

    argo_env_free_envp(envp);
    argo_env_destroy(env);
    TEST_PASS("Convert to envp works");
}

/* Test: Empty environment to envp */
static int test_env_empty_envp(void) {
    argo_env_t* env = argo_env_create();
    TEST_ASSERT(env != NULL, "Should create environment");

    /* Convert empty env to envp */
    char** envp = argo_env_to_envp(env);
    TEST_ASSERT(envp != NULL, "Should create envp array");
    TEST_ASSERT(envp[0] == NULL, "Empty env should have NULL-only array");

    argo_env_free_envp(envp);
    argo_env_destroy(env);
    TEST_PASS("Empty environment to envp works");
}

/* Test: Spawn process with environment */
static int test_env_spawn(void) {
    argo_env_t* env = argo_env_create();
    TEST_ASSERT(env != NULL, "Should create environment");

    /* Set test environment */
    argo_env_set(env, "TEST_VAR", "test_value");

    /* Spawn /usr/bin/env to print environment */
    pid_t pid;
    char* argv[] = {"/usr/bin/env", NULL};
    int result = argo_spawn_with_env("/usr/bin/env", argv, env, &pid);
    TEST_ASSERT(result == ARGO_SUCCESS, "Should spawn process");
    TEST_ASSERT(pid > 0, "Should have valid PID");

    /* Wait for child */
    int status;
    waitpid(pid, &status, 0);
    TEST_ASSERT(WIFEXITED(status), "Child should exit normally");

    argo_env_destroy(env);
    TEST_PASS("Spawn with environment works");
}

/* Test: Null handling */
static int test_env_null_handling(void) {
    /* Null env to size */
    TEST_ASSERT(argo_env_size(NULL) == 0, "NULL env should have 0 size");

    /* Null env to get */
    TEST_ASSERT(argo_env_get(NULL, "KEY") == NULL, "NULL env should return NULL");

    /* Null key to get */
    argo_env_t* env = argo_env_create();
    TEST_ASSERT(argo_env_get(env, NULL) == NULL, "NULL key should return NULL");

    /* Null env to set */
    TEST_ASSERT(argo_env_set(NULL, "KEY", "VALUE") == E_INPUT_NULL, "NULL env should fail");

    /* Null key to set */
    TEST_ASSERT(argo_env_set(env, NULL, "VALUE") == E_INPUT_NULL, "NULL key should fail");

    /* Null value to set */
    TEST_ASSERT(argo_env_set(env, "KEY", NULL) == E_INPUT_NULL, "NULL value should fail");

    /* Null env to destroy (should not crash) */
    argo_env_destroy(NULL);

    argo_env_destroy(env);
    TEST_PASS("Null handling works");
}

/* Test: Values with spaces and special chars */
static int test_env_special_values(void) {
    argo_env_t* env = argo_env_create();
    TEST_ASSERT(env != NULL, "Should create environment");

    /* Test spaces */
    argo_env_set(env, "PATH", "/usr/bin:/bin:/usr/local/bin");
    const char* val1 = argo_env_get(env, "PATH");
    TEST_ASSERT(strcmp(val1, "/usr/bin:/bin:/usr/local/bin") == 0, "Should handle colons");

    /* Test spaces */
    argo_env_set(env, "MESSAGE", "hello world");
    const char* val2 = argo_env_get(env, "MESSAGE");
    TEST_ASSERT(strcmp(val2, "hello world") == 0, "Should handle spaces");

    /* Test empty value */
    argo_env_set(env, "EMPTY", "");
    const char* val3 = argo_env_get(env, "EMPTY");
    TEST_ASSERT(val3 != NULL, "Empty value should exist");
    TEST_ASSERT(val3[0] == '\0', "Empty value should be empty string");

    argo_env_destroy(env);
    TEST_PASS("Special values work");
}

/* Main test runner */
int main(void) {
    int failed = 0;

    printf("Running isolated environment tests...\n\n");

    failed += test_env_create_destroy();
    failed += test_env_set_get();
    failed += test_env_update();
    failed += test_env_multiple();
    failed += test_env_missing();
    failed += test_env_to_envp();
    failed += test_env_empty_envp();
    failed += test_env_spawn();
    failed += test_env_null_handling();
    failed += test_env_special_values();

    printf("\n");
    if (failed == 0) {
        printf("All isolated environment tests passed!\n");
        return 0;
    } else {
        printf("%d isolated environment tests failed\n", failed);
        return 1;
    }
}

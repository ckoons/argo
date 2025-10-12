/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Project includes */
#include "argo_config.h"
#include "argo_error.h"
#include "argo_env_utils.h"
#include "argo_limits.h"

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

/* Setup test environment */
static int setup_test_config(void) {
    const char* home = getenv("HOME");
    if (!home) return 1;

    char config_dir[ARGO_PATH_MAX];
    char config_file[ARGO_PATH_MAX];

    /* Create test config directory */
    snprintf(config_dir, sizeof(config_dir), "%s/.argo/config", home);
    mkdir(config_dir, ARGO_DIR_PERMISSIONS);

    /* Write test config file */
    snprintf(config_file, sizeof(config_file), "%s/test.conf", config_dir);
    FILE* fp = fopen(config_file, "w");
    if (!fp) return 1;

    fprintf(fp, "# Test configuration\n");
    fprintf(fp, "test_key=test_value\n");
    fprintf(fp, "daemon_port=9876\n");
    fprintf(fp, "quoted_value=\"value with spaces\"\n");
    fprintf(fp, "\n");
    fprintf(fp, "# Comment line\n");
    fprintf(fp, "empty_value=\n");
    fclose(fp);

    return 0;
}

/* Cleanup test environment */
static void cleanup_test_config(void) {
    const char* home = getenv("HOME");
    if (!home) return;

    char config_file[ARGO_PATH_MAX];
    snprintf(config_file, sizeof(config_file), "%s/.argo/config/test.conf", home);
    unlink(config_file);

    argo_config_cleanup();
}

/* Test: Config loading */
static int test_config_load(void) {
    if (setup_test_config() != 0) {
        return 1;
    }

    int result = argo_config();
    TEST_ASSERT(result == ARGO_SUCCESS, "Config load should succeed");

    cleanup_test_config();
    TEST_PASS("Config loading works");
}

/* Test: Config get */
static int test_config_get(void) {
    if (setup_test_config() != 0) {
        return 1;
    }

    int result = argo_config();
    TEST_ASSERT(result == ARGO_SUCCESS, "Config load should succeed");

    const char* value = argo_config_get("test_key");
    TEST_ASSERT(value != NULL, "Should find test_key");
    TEST_ASSERT(strcmp(value, "test_value") == 0, "Value should match");

    const char* port = argo_config_get("daemon_port");
    TEST_ASSERT(port != NULL, "Should find daemon_port");
    TEST_ASSERT(strcmp(port, "9876") == 0, "Port value should match");

    cleanup_test_config();
    TEST_PASS("Config get works");
}

/* Test: Quoted values */
static int test_config_quotes(void) {
    if (setup_test_config() != 0) {
        return 1;
    }

    int result = argo_config();
    TEST_ASSERT(result == ARGO_SUCCESS, "Config load should succeed");

    const char* value = argo_config_get("quoted_value");
    TEST_ASSERT(value != NULL, "Should find quoted_value");
    TEST_ASSERT(strcmp(value, "value with spaces") == 0, "Quotes should be stripped");

    cleanup_test_config();
    TEST_PASS("Quoted value handling works");
}

/* Test: Empty values */
static int test_config_empty(void) {
    if (setup_test_config() != 0) {
        return 1;
    }

    int result = argo_config();
    TEST_ASSERT(result == ARGO_SUCCESS, "Config load should succeed");

    const char* value = argo_config_get("empty_value");
    TEST_ASSERT(value != NULL, "Should find empty_value");
    TEST_ASSERT(value[0] == '\0', "Empty value should be empty string");

    cleanup_test_config();
    TEST_PASS("Empty value handling works");
}

/* Test: Missing key */
static int test_config_missing(void) {
    if (setup_test_config() != 0) {
        return 1;
    }

    int result = argo_config();
    TEST_ASSERT(result == ARGO_SUCCESS, "Config load should succeed");

    const char* value = argo_config_get("nonexistent_key");
    TEST_ASSERT(value == NULL, "Missing key should return NULL");

    cleanup_test_config();
    TEST_PASS("Missing key handling works");
}

/* Test: Config reload */
static int test_config_reload(void) {
    if (setup_test_config() != 0) {
        return 1;
    }

    /* Initial load */
    int result = argo_config();
    TEST_ASSERT(result == ARGO_SUCCESS, "Initial load should succeed");

    const char* value1 = argo_config_get("test_key");
    TEST_ASSERT(value1 != NULL, "Should find test_key");

    /* Modify config file */
    const char* home = getenv("HOME");
    char config_file[ARGO_PATH_MAX];
    snprintf(config_file, sizeof(config_file), "%s/.argo/config/test.conf", home);

    FILE* fp = fopen(config_file, "w");
    if (fp) {
        fprintf(fp, "test_key=new_value\n");
        fclose(fp);
    }

    /* Reload */
    result = argo_config_reload();
    TEST_ASSERT(result == ARGO_SUCCESS, "Reload should succeed");

    const char* value2 = argo_config_get("test_key");
    TEST_ASSERT(value2 != NULL, "Should find test_key after reload");
    TEST_ASSERT(strcmp(value2, "new_value") == 0, "Value should be updated");

    cleanup_test_config();
    TEST_PASS("Config reload works");
}

/* Test: Multiple calls (idempotent) */
static int test_config_idempotent(void) {
    if (setup_test_config() != 0) {
        return 1;
    }

    int result1 = argo_config();
    TEST_ASSERT(result1 == ARGO_SUCCESS, "First load should succeed");

    int result2 = argo_config();
    TEST_ASSERT(result2 == ARGO_SUCCESS, "Second load should succeed (idempotent)");

    const char* value = argo_config_get("test_key");
    TEST_ASSERT(value != NULL, "Should find test_key");

    cleanup_test_config();
    TEST_PASS("Multiple config calls work");
}

/* Main test runner */
int main(void) {
    int failed = 0;

    printf("Running config tests...\n\n");

    failed += test_config_load();
    failed += test_config_get();
    failed += test_config_quotes();
    failed += test_config_empty();
    failed += test_config_missing();
    failed += test_config_reload();
    failed += test_config_idempotent();

    printf("\n");
    if (failed == 0) {
        printf("All config tests passed!\n");
        return 0;
    } else {
        printf("%d config tests failed\n", failed);
        return 1;
    }
}

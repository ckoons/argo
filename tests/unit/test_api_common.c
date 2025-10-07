/* © 2025 Casey Koons All rights reserved */

/* API common utilities test suite */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/argo_api_common.h"
#include "../include/argo_error.h"
#include "../include/argo_memory.h"

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

/* Test buffer allocation */
static void test_allocate_response_buffer(void) {
    TEST("Response buffer allocation");

    char* buffer = NULL;
    size_t capacity = 0;

    int result = api_allocate_response_buffer(&buffer, &capacity, 1024);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to allocate buffer");
        return;
    }

    if (!buffer) {
        FAIL("Buffer is NULL");
        return;
    }

    if (capacity < 1024) {
        FAIL("Capacity too small");
        free(buffer);
        return;
    }

    free(buffer);
    PASS();
}

/* Test buffer reallocation (growth) */
static void test_buffer_reallocation(void) {
    TEST("Buffer reallocation");

    char* buffer = malloc(100);
    if (!buffer) {
        FAIL("Initial malloc failed");
        return;
    }
    size_t capacity = 100;

    int result = api_allocate_response_buffer(&buffer, &capacity, 1024);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to reallocate buffer");
        free(buffer);
        return;
    }

    if (capacity < 1024) {
        FAIL("Capacity not grown");
        free(buffer);
        return;
    }

    free(buffer);
    PASS();
}

/* Test buffer allocation with NULL parameters */
static void test_buffer_allocation_null_params(void) {
    TEST("Buffer allocation NULL parameter handling");

    char* buffer = NULL;
    size_t capacity = 0;

    /* NULL buffer pointer */
    int result = api_allocate_response_buffer(NULL, &capacity, 1024);
    if (result == ARGO_SUCCESS) {
        FAIL("Should fail with NULL buffer pointer");
        return;
    }

    /* NULL capacity pointer */
    result = api_allocate_response_buffer(&buffer, NULL, 1024);
    if (result == ARGO_SUCCESS) {
        FAIL("Should fail with NULL capacity pointer");
        if (buffer) free(buffer);
        return;
    }

    PASS();
}

/* Test memory augmentation */
static void test_memory_augmentation(void) {
    TEST("Memory augmentation with prompt");

    /* Create a simple memory digest */
    ci_memory_digest_t* digest = memory_digest_create(4096);
    if (!digest) {
        FAIL("Failed to create memory digest");
        return;
    }

    /* Add a memory item */
    memory_add_item(digest, MEMORY_TYPE_FACT, "Previous context information", "test-ci");

    const char* original_prompt = "Current task";
    char* augmented = NULL;

    int result = api_augment_prompt_with_memory(digest, original_prompt, &augmented);

    memory_digest_destroy(digest);

    if (result != ARGO_SUCCESS) {
        FAIL("Memory augmentation failed");
        return;
    }

    if (!augmented) {
        FAIL("Augmented prompt is NULL");
        return;
    }

    /* Should contain both memory and original prompt */
    if (!strstr(augmented, "Previous context") || !strstr(augmented, "Current task")) {
        FAIL("Augmented prompt missing content");
        free(augmented);
        return;
    }

    free(augmented);
    PASS();
}

/* Test memory augmentation with NULL digest */
static void test_memory_augmentation_no_digest(void) {
    TEST("Memory augmentation without digest");

    const char* original_prompt = "Current task";
    char* augmented = NULL;

    int result = api_augment_prompt_with_memory(NULL, original_prompt, &augmented);

    if (result != ARGO_SUCCESS) {
        FAIL("Should succeed with NULL digest");
        return;
    }

    if (!augmented) {
        FAIL("Augmented prompt is NULL");
        return;
    }

    /* Should just be a copy of original */
    if (strcmp(augmented, original_prompt) != 0) {
        FAIL("Augmented prompt differs from original");
        free(augmented);
        return;
    }

    free(augmented);
    PASS();
}

/* Test memory augmentation with NULL parameters */
static void test_memory_augmentation_null_params(void) {
    TEST("Memory augmentation NULL parameter handling");

    ci_memory_digest_t* digest = memory_digest_create(4096);
    if (!digest) {
        FAIL("Failed to create memory digest");
        return;
    }

    char* augmented = NULL;

    /* NULL prompt */
    int result = api_augment_prompt_with_memory(digest, NULL, &augmented);
    if (result == ARGO_SUCCESS) {
        FAIL("Should fail with NULL prompt");
        memory_digest_destroy(digest);
        if (augmented) free(augmented);
        return;
    }

    /* NULL output pointer */
    result = api_augment_prompt_with_memory(digest, "test", NULL);
    if (result == ARGO_SUCCESS) {
        FAIL("Should fail with NULL output pointer");
        memory_digest_destroy(digest);
        return;
    }

    memory_digest_destroy(digest);
    PASS();
}

/* Test buffer allocation with zero size */
static void test_buffer_allocation_zero_size(void) {
    TEST("Buffer allocation with zero size");

    char* buffer = NULL;
    size_t capacity = 0;

    /* Zero size should still succeed (no-op) */
    int result = api_allocate_response_buffer(&buffer, &capacity, 0);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed with zero size");
        if (buffer) free(buffer);
        return;
    }

    free(buffer);
    PASS();
}

/* Test buffer already sufficient */
static void test_buffer_already_sufficient(void) {
    TEST("Buffer already sufficient size");

    char* buffer = malloc(2048);
    if (!buffer) {
        FAIL("Initial malloc failed");
        return;
    }
    size_t capacity = 2048;

    /* Request smaller size - should succeed */
    int result = api_allocate_response_buffer(&buffer, &capacity, 1024);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed when buffer already sufficient");
        free(buffer);
        return;
    }

    /* Capacity should be at least as large as requested */
    if (capacity < 1024) {
        FAIL("Capacity less than requested");
        free(buffer);
        return;
    }

    free(buffer);
    PASS();
}

/* Main test runner */
int main(void) {
    printf("\n");
    printf("==========================================\n");
    printf("API Common Utilities Test Suite\n");
    printf("==========================================\n\n");

    /* Buffer allocation tests */
    test_allocate_response_buffer();
    test_buffer_reallocation();
    test_buffer_allocation_null_params();
    test_buffer_allocation_zero_size();
    test_buffer_already_sufficient();

    /* Memory augmentation tests */
    test_memory_augmentation();
    test_memory_augmentation_no_digest();
    test_memory_augmentation_null_params();

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

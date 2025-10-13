/* © 2025 Casey Koons All rights reserved */

/* Claude memory management test suite */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "argo_claude_memory.h"
#include "argo_claude_internal.h"
#include "argo_error.h"

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

#define TEST_SESSION_PATH "/tmp/test_claude_session.mmap"

/* Test working memory setup */
static void test_working_memory_setup(void) {
    TEST("Working memory setup");

    claude_context_t ctx = {0};
    strncpy(ctx.session_path, TEST_SESSION_PATH, sizeof(ctx.session_path) - 1);

    int result = setup_working_memory(&ctx, "test-ci");
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to setup working memory");
        return;
    }

    if (!ctx.working_memory) {
        FAIL("Working memory not allocated");
        cleanup_working_memory(&ctx);
        unlink(TEST_SESSION_PATH);
        return;
    }

    if (ctx.memory_size == 0) {
        FAIL("Memory size not set");
        cleanup_working_memory(&ctx);
        unlink(TEST_SESSION_PATH);
        return;
    }

    cleanup_working_memory(&ctx);
    unlink(TEST_SESSION_PATH);
    PASS();
}

/* Test working memory persistence */
static void test_working_memory_persistence(void) {
    TEST("Working memory persistence");

    /* Setup initial memory */
    claude_context_t ctx1 = {0};
    strncpy(ctx1.session_path, TEST_SESSION_PATH, sizeof(ctx1.session_path) - 1);

    int result = setup_working_memory(&ctx1, "test-ci");
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to setup working memory");
        return;
    }

    /* Update turn count */
    claude_memory_update_turn(&ctx1);
    result = save_working_memory(&ctx1);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to save working memory");
        cleanup_working_memory(&ctx1);
        unlink(TEST_SESSION_PATH);
        return;
    }

    cleanup_working_memory(&ctx1);

    /* Load into new context */
    claude_context_t ctx2 = {0};
    strncpy(ctx2.session_path, TEST_SESSION_PATH, sizeof(ctx2.session_path) - 1);

    result = setup_working_memory(&ctx2, "test-ci");
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to reload working memory");
        unlink(TEST_SESSION_PATH);
        return;
    }

    result = load_working_memory(&ctx2);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to validate loaded memory");
        cleanup_working_memory(&ctx2);
        unlink(TEST_SESSION_PATH);
        return;
    }

    cleanup_working_memory(&ctx2);
    unlink(TEST_SESSION_PATH);
    PASS();
}

/* Test context building */
static void test_build_context_with_memory(void) {
    TEST("Build context with memory");

    claude_context_t ctx = {0};
    strncpy(ctx.session_path, TEST_SESSION_PATH, sizeof(ctx.session_path) - 1);

    int result = setup_working_memory(&ctx, "test-ci");
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to setup working memory");
        return;
    }

    const char* prompt = "Test prompt";
    char* context = build_context_with_memory(&ctx, prompt);
    if (!context) {
        FAIL("Failed to build context");
        cleanup_working_memory(&ctx);
        unlink(TEST_SESSION_PATH);
        return;
    }

    /* Context should contain prompt */
    if (!strstr(context, prompt)) {
        FAIL("Context missing prompt");
        free(context);
        cleanup_working_memory(&ctx);
        unlink(TEST_SESSION_PATH);
        return;
    }

    free(context);
    cleanup_working_memory(&ctx);
    unlink(TEST_SESSION_PATH);
    PASS();
}

/* Test turn count update */
static void test_turn_count_update(void) {
    TEST("Turn count update");

    claude_context_t ctx = {0};
    strncpy(ctx.session_path, TEST_SESSION_PATH, sizeof(ctx.session_path) - 1);

    int result = setup_working_memory(&ctx, "test-ci");
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to setup working memory");
        return;
    }

    /* Get initial turn count */
    working_memory_t* mem = (working_memory_t*)ctx.working_memory;
    uint32_t initial_turn = mem->turn_count;

    /* Update turn */
    claude_memory_update_turn(&ctx);

    /* Verify turn count increased */
    if (mem->turn_count != initial_turn + 1) {
        FAIL("Turn count not incremented");
        cleanup_working_memory(&ctx);
        unlink(TEST_SESSION_PATH);
        return;
    }

    cleanup_working_memory(&ctx);
    unlink(TEST_SESSION_PATH);
    PASS();
}

/* Test NULL parameter handling */
static void test_null_parameters(void) {
    TEST("NULL parameter handling");

    /* NULL context for setup */
    int result = setup_working_memory(NULL, "test-ci");
    if (result == ARGO_SUCCESS) {
        FAIL("Should fail with NULL context");
        return;
    }

    claude_context_t ctx = {0};

    /* NULL ci_name for setup */
    result = setup_working_memory(&ctx, NULL);
    if (result == ARGO_SUCCESS) {
        FAIL("Should fail with NULL ci_name");
        return;
    }

    /* NULL context for build */
    char* context = build_context_with_memory(NULL, "prompt");
    if (context != NULL) {
        FAIL("Should return NULL with NULL context");
        free(context);
        return;
    }

    /* NULL prompt for build */
    context = build_context_with_memory(&ctx, NULL);
    if (context != NULL) {
        FAIL("Should return NULL with NULL prompt");
        free(context);
        return;
    }

    /* NULL context for cleanup (should not crash) */
    cleanup_working_memory(NULL);

    /* NULL context for turn update (should not crash) */
    claude_memory_update_turn(NULL);

    PASS();
}

/* Test memory save and load */
static void test_memory_save_load(void) {
    TEST("Memory save and load");

    claude_context_t ctx = {0};
    strncpy(ctx.session_path, TEST_SESSION_PATH, sizeof(ctx.session_path) - 1);

    int result = setup_working_memory(&ctx, "test-ci");
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to setup working memory");
        return;
    }

    /* Update and save */
    claude_memory_update_turn(&ctx);
    result = save_working_memory(&ctx);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to save memory");
        cleanup_working_memory(&ctx);
        unlink(TEST_SESSION_PATH);
        return;
    }

    /* Load */
    result = load_working_memory(&ctx);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to load memory");
        cleanup_working_memory(&ctx);
        unlink(TEST_SESSION_PATH);
        return;
    }

    cleanup_working_memory(&ctx);
    unlink(TEST_SESSION_PATH);
    PASS();
}

/* Test cleanup */
static void test_cleanup(void) {
    TEST("Memory cleanup");

    claude_context_t ctx = {0};
    strncpy(ctx.session_path, TEST_SESSION_PATH, sizeof(ctx.session_path) - 1);

    int result = setup_working_memory(&ctx, "test-ci");
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to setup working memory");
        return;
    }

    /* Cleanup should not crash */
    cleanup_working_memory(&ctx);

    /* Cleanup again should not crash */
    cleanup_working_memory(&ctx);

    unlink(TEST_SESSION_PATH);
    PASS();
}

/* Main test runner */
int main(void) {
    printf("\n");
    printf("==========================================\n");
    printf("Claude Memory Management Test Suite\n");
    printf("==========================================\n\n");

    /* Claude memory tests */
    test_working_memory_setup();
    test_working_memory_persistence();
    test_build_context_with_memory();
    test_turn_count_update();
    test_null_parameters();
    test_memory_save_load();
    test_cleanup();

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

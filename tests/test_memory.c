/* © 2025 Casey Koons All rights reserved */

/* Memory manager test - verify context digest management */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/argo_memory.h"
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

/* Test digest lifecycle */
static void test_digest_lifecycle(void) {
    TEST("Digest lifecycle");

    ci_memory_digest_t* digest = memory_digest_create(8000);
    if (!digest) {
        FAIL("Failed to create digest");
        return;
    }

    if (digest->max_allowed_size != 4000) {
        FAIL("Max size should be 50% of context (4000)");
        memory_digest_destroy(digest);
        return;
    }

    memory_digest_destroy(digest);
    PASS();
}

/* Test adding memory items */
static void test_add_items(void) {
    TEST("Add memory items");

    ci_memory_digest_t* digest = memory_digest_create(8000);

    int result = memory_add_item(digest, MEMORY_TYPE_FACT,
                                "Project uses C11", "TestCI");
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to add item");
        memory_digest_destroy(digest);
        return;
    }

    if (digest->selected_count != 1) {
        FAIL("Item count should be 1");
        memory_digest_destroy(digest);
        return;
    }

    memory_digest_destroy(digest);
    PASS();
}

/* Test breadcrumbs */
static void test_breadcrumbs(void) {
    TEST("Add breadcrumbs");

    ci_memory_digest_t* digest = memory_digest_create(8000);

    memory_add_breadcrumb(digest, "Remember to check tests");
    memory_add_breadcrumb(digest, "Consider edge cases");

    if (digest->breadcrumb_count != 2) {
        FAIL("Should have 2 breadcrumbs");
        memory_digest_destroy(digest);
        return;
    }

    memory_digest_destroy(digest);
    PASS();
}

/* Test size calculation */
static void test_size_calculation(void) {
    TEST("Size calculation");

    ci_memory_digest_t* digest = memory_digest_create(8000);

    memory_add_item(digest, MEMORY_TYPE_FACT, "Small fact", "TestCI");
    memory_add_item(digest, MEMORY_TYPE_DECISION, "Big decision here", "TestCI");

    size_t size = memory_calculate_size(digest);
    if (size == 0) {
        FAIL("Size should be > 0");
        memory_digest_destroy(digest);
        return;
    }

    if (!memory_check_size_limit(digest)) {
        FAIL("Should be within size limit");
        memory_digest_destroy(digest);
        return;
    }

    memory_digest_destroy(digest);
    PASS();
}

/* Test sunset/sunrise protocol */
static void test_sunset_sunrise(void) {
    TEST("Sunset/sunrise protocol");

    ci_memory_digest_t* digest = memory_digest_create(8000);

    int result = memory_set_sunset_notes(digest, "Work in progress on registry");
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to set sunset notes");
        memory_digest_destroy(digest);
        return;
    }

    result = memory_set_sunrise_brief(digest, "Continue with memory manager");
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to set sunrise brief");
        memory_digest_destroy(digest);
        return;
    }

    if (!digest->sunset_notes || !digest->sunrise_brief) {
        FAIL("Notes should be set");
        memory_digest_destroy(digest);
        return;
    }

    memory_digest_destroy(digest);
    PASS();
}

/* Test relevance scoring */
static void test_relevance(void) {
    TEST("Relevance scoring");

    ci_memory_digest_t* digest = memory_digest_create(8000);

    memory_add_item(digest, MEMORY_TYPE_SUCCESS, "Pattern works well", "TestCI");

    memory_item_t* item = digest->selected[0];
    if (item->relevance.score != 1.0f) {
        FAIL("Initial relevance should be 1.0");
        memory_digest_destroy(digest);
        return;
    }

    memory_update_relevance(item, 0.8f);
    if (item->relevance.score != 0.8f) {
        FAIL("Relevance should be 0.8");
        memory_digest_destroy(digest);
        return;
    }

    memory_digest_destroy(digest);
    PASS();
}

/* Test memory suggestion */
static void test_suggestion(void) {
    TEST("Memory suggestion");

    ci_memory_digest_t* digest = memory_digest_create(8000);

    memory_add_item(digest, MEMORY_TYPE_FACT, "Fact 1", "TestCI");
    memory_add_item(digest, MEMORY_TYPE_DECISION, "Decision 1", "TestCI");
    memory_add_item(digest, MEMORY_TYPE_FACT, "Fact 2", "TestCI");

    int found = memory_suggest_by_type(digest, MEMORY_TYPE_FACT, 5);
    if (found != 2) {
        FAIL("Should find 2 facts");
        memory_digest_destroy(digest);
        return;
    }

    if (digest->suggestion_count != 2) {
        FAIL("Suggestion count should be 2");
        memory_digest_destroy(digest);
        return;
    }

    memory_digest_destroy(digest);
    PASS();
}

/* Test JSON serialization */
static void test_json_serialization(void) {
    TEST("JSON serialization");

    ci_memory_digest_t* digest = memory_digest_create(8000);
    strcpy(digest->session_id, "test-session");
    strcpy(digest->ci_name, "TestCI");

    memory_add_breadcrumb(digest, "Test breadcrumb");

    char* json = memory_digest_to_json(digest);
    if (!json) {
        FAIL("Failed to serialize to JSON");
        memory_digest_destroy(digest);
        return;
    }

    if (strstr(json, "test-session") == NULL) {
        FAIL("JSON should contain session ID");
        free(json);
        memory_digest_destroy(digest);
        return;
    }

    free(json);
    memory_digest_destroy(digest);
    PASS();
}

/* Test validation */
static void test_validation(void) {
    TEST("Digest validation");

    ci_memory_digest_t* digest = memory_digest_create(8000);

    memory_add_item(digest, MEMORY_TYPE_FACT, "Valid item", "TestCI");

    int result = memory_validate_digest(digest);
    if (result != ARGO_SUCCESS) {
        FAIL("Validation should pass");
        memory_digest_destroy(digest);
        return;
    }

    memory_digest_destroy(digest);
    PASS();
}

int main(void) {
    printf("\n========================================\n");
    printf("ARGO MEMORY TESTS\n");
    printf("========================================\n\n");

    test_digest_lifecycle();
    test_add_items();
    test_breadcrumbs();
    test_size_calculation();
    test_sunset_sunrise();
    test_relevance();
    test_suggestion();
    test_json_serialization();
    test_validation();

    printf("\n========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("========================================\n\n");

    return tests_failed > 0 ? 1 : 0;
}
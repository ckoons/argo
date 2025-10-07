/* © 2025 Casey Koons All rights reserved */

/* Persistence test - verify file I/O for memory, registry, and workflow */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../include/argo_memory.h"
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
        printf("✗ (%s)\n", msg); \
        tests_failed++; \
    } while(0)

/* Test memory digest file persistence */
static void test_memory_file_persistence(void) {
    TEST("Memory digest file persistence");

    const char* test_file = "/tmp/argo_test_memory.json";

    /* Create memory digest with content */
    ci_memory_digest_t* digest = memory_digest_create(8000);
    if (!digest) {
        FAIL("Failed to create digest");
        return;
    }

    /* Add various content */
    memory_set_sunset_notes(digest, "Session ended: all tests passing");
    memory_set_sunrise_brief(digest, "New session: continue development");
    memory_add_breadcrumb(digest, "Completed feature X");
    memory_add_breadcrumb(digest, "Started feature Y");
    memory_add_item(digest, MEMORY_TYPE_DECISION, "Use approach A", "Argo");
    memory_add_item(digest, MEMORY_TYPE_FACT, "System uses C11", "Maia");

    /* Save to file */
    int result = memory_save_to_file(digest, test_file);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to save memory to file");
        memory_digest_destroy(digest);
        return;
    }

    /* Verify file exists */
    if (access(test_file, F_OK) != 0) {
        FAIL("Memory file was not created");
        memory_digest_destroy(digest);
        return;
    }

    /* Load from file */
    ci_memory_digest_t* loaded = memory_load_from_file(test_file, 8000);
    if (!loaded) {
        FAIL("Failed to load memory from file");
        memory_digest_destroy(digest);
        unlink(test_file);
        return;
    }

    /* Verify content was preserved */
    if (!loaded->sunset_notes || strcmp(loaded->sunset_notes, "Session ended: all tests passing") != 0) {
        FAIL("Sunset notes not preserved");
        memory_digest_destroy(digest);
        memory_digest_destroy(loaded);
        unlink(test_file);
        return;
    }

    if (!loaded->sunrise_brief || strcmp(loaded->sunrise_brief, "New session: continue development") != 0) {
        FAIL("Sunrise brief not preserved");
        memory_digest_destroy(digest);
        memory_digest_destroy(loaded);
        unlink(test_file);
        return;
    }

    if (loaded->breadcrumb_count != 2) {
        FAIL("Breadcrumb count mismatch");
        memory_digest_destroy(digest);
        memory_digest_destroy(loaded);
        unlink(test_file);
        return;
    }

    /* Cleanup */
    memory_digest_destroy(digest);
    memory_digest_destroy(loaded);
    unlink(test_file);

    PASS();
}

/* Test registry state persistence */
static void test_registry_state_persistence(void) {
    TEST("Registry state persistence");

    const char* test_file = "/tmp/argo_test_registry.json";

    /* Create registry with CIs */
    ci_registry_t* registry = registry_create();
    if (!registry) {
        FAIL("Failed to create registry");
        return;
    }

    /* Add CIs */
    registry_add_ci(registry, "Argo", "builder", "claude", 9000);
    registry_add_ci(registry, "Maia", "requirements", "gpt-4", 9001);
    registry_add_ci(registry, "Atlas", "analysis", "claude", 9002);

    /* Set some as busy */
    ci_registry_entry_t* argo = registry_find_ci(registry, "Argo");
    if (argo) argo->status = CI_STATUS_BUSY;

    /* Save state */
    int result = registry_save_state(registry, test_file);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to save registry state");
        registry_destroy(registry);
        return;
    }

    /* Verify file exists */
    if (access(test_file, F_OK) != 0) {
        FAIL("Registry file was not created");
        registry_destroy(registry);
        return;
    }

    /* Create new registry and load state */
    ci_registry_t* loaded_registry = registry_create();
    result = registry_load_state(loaded_registry, test_file);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to load registry state");
        registry_destroy(registry);
        registry_destroy(loaded_registry);
        unlink(test_file);
        return;
    }

    /* Verify CIs were restored */
    if (loaded_registry->count != 3) {
        FAIL("CI count mismatch");
        registry_destroy(registry);
        registry_destroy(loaded_registry);
        unlink(test_file);
        return;
    }

    ci_registry_entry_t* loaded_argo = registry_find_ci(loaded_registry, "Argo");
    if (!loaded_argo) {
        FAIL("Argo not found in loaded registry");
        registry_destroy(registry);
        registry_destroy(loaded_registry);
        unlink(test_file);
        return;
    }

    if (strcmp(loaded_argo->role, "builder") != 0) {
        FAIL("Argo role not preserved");
        registry_destroy(registry);
        registry_destroy(loaded_registry);
        unlink(test_file);
        return;
    }

    if (loaded_argo->status != CI_STATUS_BUSY) {
        FAIL("Argo status not preserved");
        registry_destroy(registry);
        registry_destroy(loaded_registry);
        unlink(test_file);
        return;
    }

    /* Cleanup */
    registry_destroy(registry);
    registry_destroy(loaded_registry);
    unlink(test_file);

    PASS();
}

/* Test persistence error handling */
static void test_persistence_error_handling(void) {
    TEST("Persistence error handling");

    /* Try to save to invalid path */
    ci_memory_digest_t* digest = memory_digest_create(8000);
    int result = memory_save_to_file(digest, "/nonexistent/path/file.json");
    if (result == ARGO_SUCCESS) {
        FAIL("Should fail with invalid path");
        memory_digest_destroy(digest);
        return;
    }

    /* Try to load from nonexistent file */
    ci_memory_digest_t* loaded = memory_load_from_file("/tmp/nonexistent_argo_file.json", 8000);
    if (loaded != NULL) {
        FAIL("Should return NULL for nonexistent file");
        memory_digest_destroy(digest);
        memory_digest_destroy(loaded);
        return;
    }

    /* Try to save NULL digest */
    result = memory_save_to_file(NULL, "/tmp/test.json");
    if (result == ARGO_SUCCESS) {
        FAIL("Should fail with NULL digest");
        memory_digest_destroy(digest);
        return;
    }

    memory_digest_destroy(digest);
    PASS();
}

/* Test registry save/load roundtrip */
static void test_registry_roundtrip(void) {
    TEST("Registry save/load roundtrip");

    const char* test_file = "/tmp/argo_test_registry_roundtrip.json";

    /* Create and populate registry */
    ci_registry_t* registry = registry_create();
    registry_add_ci(registry, "Alpha", "builder", "claude", 9100);
    registry_add_ci(registry, "Beta", "coordinator", "gpt-4", 9101);
    registry_add_ci(registry, "Gamma", "requirements", "llama3:70b", 9102);
    registry_add_ci(registry, "Delta", "analysis", "claude", 9103);

    /* Set various statuses */
    ci_registry_entry_t* ci;
    ci = registry_find_ci(registry, "Alpha");
    if (ci) ci->status = CI_STATUS_BUSY;
    ci = registry_find_ci(registry, "Beta");
    if (ci) ci->status = CI_STATUS_BUSY;
    ci = registry_find_ci(registry, "Gamma");
    if (ci) ci->status = CI_STATUS_READY;
    ci = registry_find_ci(registry, "Delta");
    if (ci) ci->status = CI_STATUS_OFFLINE;

    /* Save */
    int result = registry_save_state(registry, test_file);
    if (result != ARGO_SUCCESS) {
        FAIL("Save failed");
        registry_destroy(registry);
        return;
    }

    /* Load into new registry */
    ci_registry_t* loaded = registry_create();
    result = registry_load_state(loaded, test_file);
    if (result != ARGO_SUCCESS) {
        FAIL("Load failed");
        registry_destroy(registry);
        registry_destroy(loaded);
        unlink(test_file);
        return;
    }

    /* Verify all CIs and their states */
    if (loaded->count != 4) {
        FAIL("Count mismatch");
        registry_destroy(registry);
        registry_destroy(loaded);
        unlink(test_file);
        return;
    }

    ci_registry_entry_t* loaded_ci;
    loaded_ci = registry_find_ci(loaded, "Alpha");
    if (!loaded_ci || loaded_ci->status != CI_STATUS_BUSY) {
        FAIL("Alpha state mismatch");
        registry_destroy(registry);
        registry_destroy(loaded);
        unlink(test_file);
        return;
    }

    loaded_ci = registry_find_ci(loaded, "Delta");
    if (!loaded_ci || loaded_ci->status != CI_STATUS_OFFLINE) {
        FAIL("Delta state mismatch");
        registry_destroy(registry);
        registry_destroy(loaded);
        unlink(test_file);
        return;
    }

    /* Cleanup */
    registry_destroy(registry);
    registry_destroy(loaded);
    unlink(test_file);

    PASS();
}

int main(void) {
    printf("\n========================================\n");
    printf("ARGO PERSISTENCE TESTS\n");
    printf("========================================\n\n");

    test_memory_file_persistence();
    test_registry_state_persistence();
    test_persistence_error_handling();
    test_registry_roundtrip();

    printf("\n========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("========================================\n\n");

    return tests_failed > 0 ? 1 : 0;
}

/* © 2025 Casey Koons All rights reserved */

/* Registry test - verify CI registration and discovery */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
        printf("✗ %s\n", msg); \
        tests_failed++; \
    } while(0)

/* Test registry creation and destruction */
static void test_registry_lifecycle(void) {
    TEST("Registry lifecycle");

    ci_registry_t* registry = registry_create();
    if (!registry) {
        FAIL("Failed to create registry");
        return;
    }

    if (registry->count != 0) {
        FAIL("New registry should be empty");
        registry_destroy(registry);
        return;
    }

    if (!registry->initialized) {
        FAIL("Registry should be initialized");
        registry_destroy(registry);
        return;
    }

    registry_destroy(registry);
    PASS();
}

/* Test adding CIs */
static void test_add_ci(void) {
    TEST("Add CI to registry");

    ci_registry_t* registry = registry_create();
    if (!registry) {
        FAIL("Failed to create registry");
        return;
    }

    int result = registry_add_ci(registry, "TestCI", "builder", "llama3:70b", 9000);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to add CI");
        registry_destroy(registry);
        return;
    }

    if (registry->count != 1) {
        FAIL("Registry count should be 1");
        registry_destroy(registry);
        return;
    }

    registry_destroy(registry);
    PASS();
}

/* Test finding CIs */
static void test_find_ci(void) {
    TEST("Find CI by name");

    ci_registry_t* registry = registry_create();
    registry_add_ci(registry, "Argo", "builder", "llama3:70b", 9000);
    registry_add_ci(registry, "Maia", "requirements", "claude", 9020);

    ci_registry_entry_t* entry = registry_find_ci(registry, "Argo");
    if (!entry) {
        FAIL("Should find Argo");
        registry_destroy(registry);
        return;
    }

    if (strcmp(entry->name, "Argo") != 0) {
        FAIL("Found wrong CI");
        registry_destroy(registry);
        return;
    }

    entry = registry_find_ci(registry, "NonExistent");
    if (entry) {
        FAIL("Should not find nonexistent CI");
        registry_destroy(registry);
        return;
    }

    registry_destroy(registry);
    PASS();
}

/* Test role-based finding */
static void test_find_by_role(void) {
    TEST("Find CI by role");

    ci_registry_t* registry = registry_create();
    registry_add_ci(registry, "Argo", "builder", "llama3:70b", 9000);
    registry_add_ci(registry, "Maia", "requirements", "claude", 9020);
    registry_add_ci(registry, "Iris", "analysis", "gpt-4", 9030);

    ci_registry_entry_t* entry = registry_find_by_role(registry, "builder");
    if (!entry || strcmp(entry->name, "Argo") != 0) {
        FAIL("Should find builder");
        registry_destroy(registry);
        return;
    }

    int count = 0;
    ci_registry_entry_t** results = registry_find_all_by_role(registry, "builder", &count);
    if (count != 1 || !results) {
        FAIL("Should find 1 builder");
        free(results);
        registry_destroy(registry);
        return;
    }
    free(results);

    registry_destroy(registry);
    PASS();
}

/* Test port allocation */
static void test_port_allocation(void) {
    TEST("Port allocation by role");

    ci_registry_t* registry = registry_create();

    int port = registry_allocate_port(registry, "builder");
    if (port != 9000) {
        FAIL("First builder port should be 9000");
        registry_destroy(registry);
        return;
    }

    registry_add_ci(registry, "Argo", "builder", "llama3:70b", port);

    port = registry_allocate_port(registry, "builder");
    if (port != 9001) {
        FAIL("Second builder port should be 9001");
        registry_destroy(registry);
        return;
    }

    port = registry_allocate_port(registry, "requirements");
    if (port != 9020) {
        FAIL("First requirements port should be 9020");
        registry_destroy(registry);
        return;
    }

    registry_destroy(registry);
    PASS();
}

/* Test status management */
static void test_status_management(void) {
    TEST("Status management");

    ci_registry_t* registry = registry_create();
    registry_add_ci(registry, "Argo", "builder", "llama3:70b", 9000);

    ci_registry_entry_t* entry = registry_find_ci(registry, "Argo");
    if (entry->status != CI_STATUS_OFFLINE) {
        FAIL("New CI should be offline");
        registry_destroy(registry);
        return;
    }

    int result = registry_update_status(registry, "Argo", CI_STATUS_READY);
    if (result != ARGO_SUCCESS) {
        FAIL("Failed to update status");
        registry_destroy(registry);
        return;
    }

    entry = registry_find_ci(registry, "Argo");
    if (entry->status != CI_STATUS_READY) {
        FAIL("Status should be READY");
        registry_destroy(registry);
        return;
    }

    registry_destroy(registry);
    PASS();
}

/* Test statistics */
static void test_statistics(void) {
    TEST("Registry statistics");

    ci_registry_t* registry = registry_create();
    registry_add_ci(registry, "Argo", "builder", "llama3:70b", 9000);
    registry_add_ci(registry, "Maia", "requirements", "claude", 9020);
    registry_update_status(registry, "Argo", CI_STATUS_READY);

    registry_stats_t* stats = registry_get_stats(registry);
    if (!stats) {
        FAIL("Failed to get stats");
        registry_destroy(registry);
        return;
    }

    if (stats->total_cis != 2) {
        FAIL("Should have 2 total CIs");
        registry_free_stats(stats);
        registry_destroy(registry);
        return;
    }

    if (stats->online_cis != 1) {
        FAIL("Should have 1 online CI");
        registry_free_stats(stats);
        registry_destroy(registry);
        return;
    }

    registry_free_stats(stats);
    registry_destroy(registry);
    PASS();
}

/* Test message creation */
static void test_message_creation(void) {
    TEST("Message creation");

    ci_message_t* msg = message_create("Argo", "Maia", "request", "Hello");
    if (!msg) {
        FAIL("Failed to create message");
        return;
    }

    if (strcmp(msg->from, "Argo") != 0 || strcmp(msg->to, "Maia") != 0) {
        FAIL("Message fields incorrect");
        message_destroy(msg);
        return;
    }

    char* json = message_to_json(msg);
    if (!json) {
        FAIL("Failed to convert to JSON");
        message_destroy(msg);
        return;
    }

    free(json);
    message_destroy(msg);
    PASS();
}

int main(void) {
    printf("\n========================================\n");
    printf("ARGO REGISTRY TESTS\n");
    printf("========================================\n\n");

    test_registry_lifecycle();
    test_add_ci();
    test_find_ci();
    test_find_by_role();
    test_port_allocation();
    test_status_management();
    test_statistics();
    test_message_creation();

    printf("\n========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_failed);
    printf("========================================\n\n");

    return tests_failed > 0 ? 1 : 0;
}
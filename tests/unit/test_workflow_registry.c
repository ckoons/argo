/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Project includes */
#include "argo_workflow_registry.h"
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

/* Test: Create and destroy registry */
static int test_registry_create_destroy(void) {
    workflow_registry_t* reg = workflow_registry_create();
    TEST_ASSERT(reg != NULL, "Should create registry");
    TEST_ASSERT(workflow_registry_count(reg, (workflow_state_t)-1) == 0,
                "Should be empty initially");

    workflow_registry_destroy(reg);
    TEST_PASS("Create and destroy works");
}

/* Test: Add workflow */
static int test_registry_add(void) {
    workflow_registry_t* reg = workflow_registry_create();
    TEST_ASSERT(reg != NULL, "Should create registry");

    workflow_entry_t entry = {
        .workflow_id = "test-123",
        .workflow_name = "test_workflow",
        .state = WORKFLOW_STATE_PENDING,
        .executor_pid = 0,
        .start_time = time(NULL),
        .end_time = 0,
        .exit_code = 0,
        .current_step = 0,
        .total_steps = 5
    };
    strncpy(entry.workflow_id, "test-123", sizeof(entry.workflow_id) - 1);
    strncpy(entry.workflow_name, "test_workflow", sizeof(entry.workflow_name) - 1);

    int result = workflow_registry_add(reg, &entry);
    TEST_ASSERT(result == ARGO_SUCCESS, "Should add workflow");
    TEST_ASSERT(workflow_registry_count(reg, (workflow_state_t)-1) == 1,
                "Should have 1 workflow");

    workflow_registry_destroy(reg);
    TEST_PASS("Add workflow works");
}

/* Test: Find workflow */
static int test_registry_find(void) {
    workflow_registry_t* reg = workflow_registry_create();
    TEST_ASSERT(reg != NULL, "Should create registry");

    workflow_entry_t entry = {0};
    strncpy(entry.workflow_id, "find-123", sizeof(entry.workflow_id) - 1);
    strncpy(entry.workflow_name, "find_test", sizeof(entry.workflow_name) - 1);
    entry.state = WORKFLOW_STATE_RUNNING;
    entry.start_time = time(NULL);

    workflow_registry_add(reg, &entry);

    /* Find existing */
    const workflow_entry_t* found = workflow_registry_find(reg, "find-123");
    TEST_ASSERT(found != NULL, "Should find workflow");
    TEST_ASSERT(strcmp(found->workflow_id, "find-123") == 0, "ID should match");
    TEST_ASSERT(strcmp(found->workflow_name, "find_test") == 0, "Name should match");
    TEST_ASSERT(found->state == WORKFLOW_STATE_RUNNING, "State should match");

    /* Find missing */
    const workflow_entry_t* missing = workflow_registry_find(reg, "nonexistent");
    TEST_ASSERT(missing == NULL, "Should not find nonexistent workflow");

    workflow_registry_destroy(reg);
    TEST_PASS("Find workflow works");
}

/* Test: Update state */
static int test_registry_update_state(void) {
    workflow_registry_t* reg = workflow_registry_create();
    TEST_ASSERT(reg != NULL, "Should create registry");

    workflow_entry_t entry = {0};
    strncpy(entry.workflow_id, "update-123", sizeof(entry.workflow_id) - 1);
    entry.state = WORKFLOW_STATE_RUNNING;
    entry.start_time = time(NULL);

    workflow_registry_add(reg, &entry);

    /* Update to completed */
    int result = workflow_registry_update_state(reg, "update-123",
                                                 WORKFLOW_STATE_COMPLETED);
    TEST_ASSERT(result == ARGO_SUCCESS, "Should update state");

    /* Verify */
    const workflow_entry_t* found = workflow_registry_find(reg, "update-123");
    TEST_ASSERT(found->state == WORKFLOW_STATE_COMPLETED, "State should be updated");
    TEST_ASSERT(found->end_time > 0, "End time should be set");

    workflow_registry_destroy(reg);
    TEST_PASS("Update state works");
}

/* Test: Update progress */
static int test_registry_update_progress(void) {
    workflow_registry_t* reg = workflow_registry_create();
    TEST_ASSERT(reg != NULL, "Should create registry");

    workflow_entry_t entry = {0};
    strncpy(entry.workflow_id, "progress-123", sizeof(entry.workflow_id) - 1);
    entry.state = WORKFLOW_STATE_RUNNING;
    entry.current_step = 1;
    entry.total_steps = 5;

    workflow_registry_add(reg, &entry);

    /* Update progress */
    int result = workflow_registry_update_progress(reg, "progress-123", 3);
    TEST_ASSERT(result == ARGO_SUCCESS, "Should update progress");

    /* Verify */
    const workflow_entry_t* found = workflow_registry_find(reg, "progress-123");
    TEST_ASSERT(found->current_step == 3, "Progress should be updated");

    workflow_registry_destroy(reg);
    TEST_PASS("Update progress works");
}

/* Test: List workflows */
static int test_registry_list(void) {
    workflow_registry_t* reg = workflow_registry_create();
    TEST_ASSERT(reg != NULL, "Should create registry");

    /* Add multiple workflows */
    for (int i = 0; i < 3; i++) {
        workflow_entry_t entry = {0};
        snprintf(entry.workflow_id, sizeof(entry.workflow_id), "list-%d", i);
        snprintf(entry.workflow_name, sizeof(entry.workflow_name), "workflow_%d", i);
        entry.state = WORKFLOW_STATE_RUNNING;
        entry.start_time = time(NULL);

        workflow_registry_add(reg, &entry);
    }

    /* List all */
    workflow_entry_t* entries = NULL;
    int count = 0;
    int result = workflow_registry_list(reg, &entries, &count);
    TEST_ASSERT(result == ARGO_SUCCESS, "Should list workflows");
    TEST_ASSERT(count == 3, "Should have 3 workflows");
    TEST_ASSERT(entries != NULL, "Should return entries array");

    free(entries);
    workflow_registry_destroy(reg);
    TEST_PASS("List workflows works");
}

/* Test: Count by state */
static int test_registry_count_by_state(void) {
    workflow_registry_t* reg = workflow_registry_create();
    TEST_ASSERT(reg != NULL, "Should create registry");

    /* Add workflows in different states */
    workflow_entry_t entry1 = {0};
    strncpy(entry1.workflow_id, "count-1", sizeof(entry1.workflow_id) - 1);
    entry1.state = WORKFLOW_STATE_RUNNING;
    workflow_registry_add(reg, &entry1);

    workflow_entry_t entry2 = {0};
    strncpy(entry2.workflow_id, "count-2", sizeof(entry2.workflow_id) - 1);
    entry2.state = WORKFLOW_STATE_RUNNING;
    workflow_registry_add(reg, &entry2);

    workflow_entry_t entry3 = {0};
    strncpy(entry3.workflow_id, "count-3", sizeof(entry3.workflow_id) - 1);
    entry3.state = WORKFLOW_STATE_COMPLETED;
    workflow_registry_add(reg, &entry3);

    /* Count by state */
    int running = workflow_registry_count(reg, WORKFLOW_STATE_RUNNING);
    TEST_ASSERT(running == 2, "Should have 2 running workflows");

    int completed = workflow_registry_count(reg, WORKFLOW_STATE_COMPLETED);
    TEST_ASSERT(completed == 1, "Should have 1 completed workflow");

    int all = workflow_registry_count(reg, (workflow_state_t)-1);
    TEST_ASSERT(all == 3, "Should have 3 total workflows");

    workflow_registry_destroy(reg);
    TEST_PASS("Count by state works");
}

/* Test: State conversion */
static int test_state_conversion(void) {
    /* Test state to string */
    TEST_ASSERT(strcmp(workflow_state_to_string(WORKFLOW_STATE_PENDING), "pending") == 0,
                "Pending conversion");
    TEST_ASSERT(strcmp(workflow_state_to_string(WORKFLOW_STATE_RUNNING), "running") == 0,
                "Running conversion");
    TEST_ASSERT(strcmp(workflow_state_to_string(WORKFLOW_STATE_COMPLETED), "completed") == 0,
                "Completed conversion");
    TEST_ASSERT(strcmp(workflow_state_to_string(WORKFLOW_STATE_FAILED), "failed") == 0,
                "Failed conversion");
    TEST_ASSERT(strcmp(workflow_state_to_string(WORKFLOW_STATE_ABANDONED), "abandoned") == 0,
                "Abandoned conversion");

    /* Test string to state */
    TEST_ASSERT(workflow_state_from_string("pending") == WORKFLOW_STATE_PENDING,
                "Parse pending");
    TEST_ASSERT(workflow_state_from_string("running") == WORKFLOW_STATE_RUNNING,
                "Parse running");
    TEST_ASSERT(workflow_state_from_string("completed") == WORKFLOW_STATE_COMPLETED,
                "Parse completed");
    TEST_ASSERT(workflow_state_from_string("failed") == WORKFLOW_STATE_FAILED,
                "Parse failed");
    TEST_ASSERT(workflow_state_from_string("abandoned") == WORKFLOW_STATE_ABANDONED,
                "Parse abandoned");

    TEST_PASS("State conversion works");
}

/* Test: Save to JSON */
static int test_registry_save(void) {
    workflow_registry_t* reg = workflow_registry_create();
    TEST_ASSERT(reg != NULL, "Should create registry");

    /* Add workflow */
    workflow_entry_t entry = {0};
    strncpy(entry.workflow_id, "save-123", sizeof(entry.workflow_id) - 1);
    strncpy(entry.workflow_name, "save_test", sizeof(entry.workflow_name) - 1);
    entry.state = WORKFLOW_STATE_COMPLETED;
    entry.start_time = 1701234567;
    entry.end_time = 1701234789;
    entry.exit_code = 0;
    entry.current_step = 5;
    entry.total_steps = 5;

    workflow_registry_add(reg, &entry);

    /* Save to temp file */
    const char* path = "/tmp/test_workflow_registry.json";
    int result = workflow_registry_save(reg, path);
    TEST_ASSERT(result == ARGO_SUCCESS, "Should save registry");

    /* Verify file exists */
    FILE* fp = fopen(path, "r");
    TEST_ASSERT(fp != NULL, "File should exist");
    fclose(fp);

    /* Cleanup */
    unlink(path);
    workflow_registry_destroy(reg);
    TEST_PASS("Save to JSON works");
}

/* Test: Prune old workflows */
static int test_registry_prune(void) {
    workflow_registry_t* reg = workflow_registry_create();
    TEST_ASSERT(reg != NULL, "Should create registry");

    time_t now = time(NULL);
    time_t old_time = now - (10 * 24 * 60 * 60);  /* 10 days ago */

    /* Add old completed workflow */
    workflow_entry_t old_entry = {0};
    strncpy(old_entry.workflow_id, "old-123", sizeof(old_entry.workflow_id) - 1);
    old_entry.state = WORKFLOW_STATE_COMPLETED;
    old_entry.start_time = old_time;
    old_entry.end_time = old_time + 100;
    workflow_registry_add(reg, &old_entry);

    /* Add recent completed workflow */
    workflow_entry_t recent_entry = {0};
    strncpy(recent_entry.workflow_id, "recent-123", sizeof(recent_entry.workflow_id) - 1);
    recent_entry.state = WORKFLOW_STATE_COMPLETED;
    recent_entry.start_time = now - 100;
    recent_entry.end_time = now;
    workflow_registry_add(reg, &recent_entry);

    /* Add running workflow */
    workflow_entry_t running_entry = {0};
    strncpy(running_entry.workflow_id, "running-123", sizeof(running_entry.workflow_id) - 1);
    running_entry.state = WORKFLOW_STATE_RUNNING;
    running_entry.start_time = now;
    workflow_registry_add(reg, &running_entry);

    TEST_ASSERT(workflow_registry_count(reg, (workflow_state_t)-1) == 3,
                "Should have 3 workflows");

    /* Prune workflows older than 7 days */
    time_t cutoff = now - (7 * 24 * 60 * 60);
    int pruned = workflow_registry_prune(reg, cutoff);
    TEST_ASSERT(pruned == 1, "Should prune 1 workflow");
    TEST_ASSERT(workflow_registry_count(reg, (workflow_state_t)-1) == 2,
                "Should have 2 workflows left");

    /* Verify old one is gone, recent and running remain */
    TEST_ASSERT(workflow_registry_find(reg, "old-123") == NULL, "Old should be gone");
    TEST_ASSERT(workflow_registry_find(reg, "recent-123") != NULL, "Recent should remain");
    TEST_ASSERT(workflow_registry_find(reg, "running-123") != NULL, "Running should remain");

    workflow_registry_destroy(reg);
    TEST_PASS("Prune old workflows works");
}

/* Test: Duplicate workflow ID */
static int test_registry_duplicate(void) {
    workflow_registry_t* reg = workflow_registry_create();
    TEST_ASSERT(reg != NULL, "Should create registry");

    workflow_entry_t entry = {0};
    strncpy(entry.workflow_id, "dup-123", sizeof(entry.workflow_id) - 1);
    entry.state = WORKFLOW_STATE_RUNNING;

    /* Add first time - should succeed */
    int result1 = workflow_registry_add(reg, &entry);
    TEST_ASSERT(result1 == ARGO_SUCCESS, "First add should succeed");

    /* Add second time - should fail */
    int result2 = workflow_registry_add(reg, &entry);
    TEST_ASSERT(result2 == E_DUPLICATE, "Duplicate should fail");

    workflow_registry_destroy(reg);
    TEST_PASS("Duplicate workflow ID handling works");
}

/* Main test runner */
int main(void) {
    int failed = 0;

    printf("Running workflow registry tests...\n\n");

    failed += test_registry_create_destroy();
    failed += test_registry_add();
    failed += test_registry_find();
    failed += test_registry_update_state();
    failed += test_registry_update_progress();
    failed += test_registry_list();
    failed += test_registry_count_by_state();
    failed += test_state_conversion();
    failed += test_registry_save();
    failed += test_registry_prune();
    failed += test_registry_duplicate();

    printf("\n");
    if (failed == 0) {
        printf("All workflow registry tests passed!\n");
        return 0;
    } else {
        printf("%d workflow registry tests failed\n", failed);
        return 1;
    }
}

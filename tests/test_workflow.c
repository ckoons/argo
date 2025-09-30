/* © 2025 Casey Koons All rights reserved */

/* Test suite for workflow controller */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Project includes */
#include "argo_workflow.h"
#include "argo_merge.h"
#include "argo_registry.h"
#include "argo_lifecycle.h"
#include "argo_error.h"

/* Test helpers */
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        printf("Testing: %-50s", name); \
        tests_run++; \
    } while(0)

#define PASS() \
    do { \
        printf(" ✓\n"); \
        tests_passed++; \
    } while(0)

#define FAIL(msg) \
    do { \
        printf(" ✗\n  %s\n", msg); \
    } while(0)

/* Test workflow creation and lifecycle */
static void test_workflow_lifecycle(void) {
    TEST("Workflow lifecycle");

    ci_registry_t* registry = registry_create();
    lifecycle_manager_t* lifecycle = lifecycle_manager_create(registry);

    workflow_controller_t* workflow = workflow_create(registry, lifecycle, "test-workflow");
    if (!workflow) {
        registry_destroy(registry);
        lifecycle_manager_destroy(lifecycle);
        FAIL("Failed to create workflow");
        return;
    }

    assert(workflow->current_phase == WORKFLOW_INIT);
    assert(workflow->state == WORKFLOW_STATE_IDLE);
    assert(workflow->total_tasks == 0);

    workflow_destroy(workflow);
    lifecycle_manager_destroy(lifecycle);
    registry_destroy(registry);
    PASS();
}

/* Test workflow start */
static void test_workflow_start(void) {
    TEST("Workflow start");

    ci_registry_t* registry = registry_create();
    lifecycle_manager_t* lifecycle = lifecycle_manager_create(registry);
    workflow_controller_t* workflow = workflow_create(registry, lifecycle, "test-workflow");

    int result = workflow_start(workflow, "main");
    assert(result == ARGO_SUCCESS);
    assert(workflow->state == WORKFLOW_STATE_RUNNING);
    assert(strcmp(workflow->base_branch, "main") == 0);

    workflow_destroy(workflow);
    lifecycle_manager_destroy(lifecycle);
    registry_destroy(registry);
    PASS();
}

/* Test task creation */
static void test_task_creation(void) {
    TEST("Task creation");

    ci_registry_t* registry = registry_create();
    lifecycle_manager_t* lifecycle = lifecycle_manager_create(registry);
    workflow_controller_t* workflow = workflow_create(registry, lifecycle, "test-workflow");

    ci_task_t* task = workflow_create_task(workflow, "Write tests", WORKFLOW_TEST);
    assert(task != NULL);
    assert(strcmp(task->description, "Write tests") == 0);
    assert(task->phase == WORKFLOW_TEST);
    assert(task->completed == 0);
    assert(workflow->total_tasks == 1);

    workflow_destroy(workflow);
    lifecycle_manager_destroy(lifecycle);
    registry_destroy(registry);
    PASS();
}

/* Test task assignment */
static void test_task_assignment(void) {
    TEST("Task assignment");

    ci_registry_t* registry = registry_create();
    lifecycle_manager_t* lifecycle = lifecycle_manager_create(registry);

    /* Add a CI */
    registry_add_ci(registry, "Alice", "builder", "claude", 9001);
    registry_update_status(registry, "Alice", CI_STATUS_READY);

    workflow_controller_t* workflow = workflow_create(registry, lifecycle, "test-workflow");
    ci_task_t* task = workflow_create_task(workflow, "Build feature", WORKFLOW_DEVELOP);

    int result = workflow_assign_task(workflow, task->id, "Alice");
    assert(result == ARGO_SUCCESS);
    assert(strcmp(task->assigned_to, "Alice") == 0);
    assert(task->assigned_at > 0);

    workflow_destroy(workflow);
    lifecycle_manager_destroy(lifecycle);
    registry_destroy(registry);
    PASS();
}

/* Test task completion */
static void test_task_completion(void) {
    TEST("Task completion");

    ci_registry_t* registry = registry_create();
    lifecycle_manager_t* lifecycle = lifecycle_manager_create(registry);
    workflow_controller_t* workflow = workflow_create(registry, lifecycle, "test-workflow");

    ci_task_t* task = workflow_create_task(workflow, "Run tests", WORKFLOW_TEST);
    int result = workflow_complete_task(workflow, task->id);

    assert(result == ARGO_SUCCESS);
    assert(task->completed == 1);
    assert(task->completed_at > 0);
    assert(workflow->completed_tasks == 1);

    workflow_destroy(workflow);
    lifecycle_manager_destroy(lifecycle);
    registry_destroy(registry);
    PASS();
}

/* Test phase advancement */
static void test_phase_advancement(void) {
    TEST("Phase advancement");

    ci_registry_t* registry = registry_create();
    lifecycle_manager_t* lifecycle = lifecycle_manager_create(registry);
    workflow_controller_t* workflow = workflow_create(registry, lifecycle, "test-workflow");

    workflow_start(workflow, "main");

    /* Can't advance with incomplete tasks */
    ci_task_t* task = workflow_create_task(workflow, "Initialize", WORKFLOW_INIT);
    assert(workflow_can_advance(workflow) == 0);

    /* Complete task and advance */
    workflow_complete_task(workflow, task->id);
    assert(workflow_can_advance(workflow) == 1);

    int result = workflow_advance_phase(workflow);
    assert(result == ARGO_SUCCESS);
    assert(workflow->current_phase == WORKFLOW_PLAN);

    workflow_destroy(workflow);
    lifecycle_manager_destroy(lifecycle);
    registry_destroy(registry);
    PASS();
}

/* Test auto-assignment based on roles */
static void test_auto_assignment(void) {
    TEST("Auto-assignment by role");

    ci_registry_t* registry = registry_create();
    lifecycle_manager_t* lifecycle = lifecycle_manager_create(registry);

    /* Add CIs with different roles */
    registry_add_ci(registry, "Alice", "builder", "claude", 9001);
    registry_add_ci(registry, "Bob", "requirements", "gpt4", 9002);
    registry_update_status(registry, "Alice", CI_STATUS_READY);
    registry_update_status(registry, "Bob", CI_STATUS_READY);

    workflow_controller_t* workflow = workflow_create(registry, lifecycle, "test-workflow");

    /* Create tasks for different phases */
    ci_task_t* task1 = workflow_create_task(workflow, "Write specs", WORKFLOW_PLAN);
    ci_task_t* task2 = workflow_create_task(workflow, "Code feature", WORKFLOW_DEVELOP);

    /* Auto-assign */
    int result = workflow_auto_assign_tasks(workflow);
    assert(result == ARGO_SUCCESS);

    /* Verify both tasks were assigned */
    assert(strlen(task1->assigned_to) > 0);
    assert(strlen(task2->assigned_to) > 0);

    /* Verify correct role matching */
    ci_registry_entry_t* ci1 = registry_find_ci(registry, task1->assigned_to);
    ci_registry_entry_t* ci2 = registry_find_ci(registry, task2->assigned_to);

    assert(ci1 != NULL);
    assert(ci2 != NULL);
    assert(strcmp(ci1->role, "requirements") == 0);  /* PLAN needs requirements */
    assert(strcmp(ci2->role, "builder") == 0);  /* DEVELOP needs builder */

    workflow_destroy(workflow);
    lifecycle_manager_destroy(lifecycle);
    registry_destroy(registry);
    PASS();
}

/* Test merge negotiation creation */
static void test_merge_negotiation_lifecycle(void) {
    TEST("Merge negotiation lifecycle");

    merge_negotiation_t* negotiation = merge_negotiation_create("feature-a", "feature-b");
    if (!negotiation) {
        FAIL("Failed to create negotiation");
        return;
    }

    assert(strcmp(negotiation->branch_a, "feature-a") == 0);
    assert(strcmp(negotiation->branch_b, "feature-b") == 0);
    assert(negotiation->conflict_count == 0);
    assert(negotiation->completed == 0);

    merge_negotiation_destroy(negotiation);
    PASS();
}

/* Test adding conflicts */
static void test_merge_conflicts(void) {
    TEST("Adding merge conflicts");

    merge_negotiation_t* negotiation = merge_negotiation_create("feature-a", "feature-b");

    merge_conflict_t* conflict = merge_add_conflict(negotiation,
                                                   "main.c",
                                                   45, 67,
                                                   "int x = 1;",
                                                   "int x = 2;");
    assert(conflict != NULL);
    assert(strcmp(conflict->file, "main.c") == 0);
    assert(conflict->line_start == 45);
    assert(conflict->line_end == 67);
    assert(negotiation->conflict_count == 1);

    merge_negotiation_destroy(negotiation);
    PASS();
}

/* Test conflict proposals */
static void test_conflict_proposals(void) {
    TEST("Conflict resolution proposals");

    merge_negotiation_t* negotiation = merge_negotiation_create("feature-a", "feature-b");
    merge_conflict_t* conflict = merge_add_conflict(negotiation,
                                                   "main.c", 45, 67,
                                                   "version A", "version B");

    /* CI proposes resolution */
    int result = merge_propose_resolution(negotiation, "Alice", conflict,
                                         "int x = 3;", 85);
    assert(result == ARGO_SUCCESS);
    assert(negotiation->proposal_count == 1);

    /* Another CI proposes with higher confidence */
    result = merge_propose_resolution(negotiation, "Bob", conflict,
                                     "int x = 4;", 95);
    assert(result == ARGO_SUCCESS);
    assert(negotiation->proposal_count == 2);

    /* Best proposal should be Bob's */
    merge_proposal_t* best = merge_select_best_proposal(negotiation);
    assert(best != NULL);
    assert(strcmp(best->ci_name, "Bob") == 0);
    assert(best->confidence == 95);

    merge_negotiation_destroy(negotiation);
    PASS();
}

/* Test negotiation completion */
static void test_negotiation_completion(void) {
    TEST("Negotiation completion check");

    merge_negotiation_t* negotiation = merge_negotiation_create("feature-a", "feature-b");

    /* Add two conflicts */
    merge_conflict_t* c1 = merge_add_conflict(negotiation, "file1.c", 1, 10, "a", "b");
    merge_conflict_t* c2 = merge_add_conflict(negotiation, "file2.c", 20, 30, "x", "y");

    /* Not complete yet */
    assert(merge_is_complete(negotiation) == 0);

    /* Resolve first conflict */
    merge_propose_resolution(negotiation, "Alice", c1, "resolved1", 80);
    assert(merge_is_complete(negotiation) == 0);

    /* Resolve second conflict */
    merge_propose_resolution(negotiation, "Bob", c2, "resolved2", 90);
    assert(merge_is_complete(negotiation) == 1);

    merge_negotiation_destroy(negotiation);
    PASS();
}

/* Test conflict JSON serialization */
static void test_conflict_json(void) {
    TEST("Conflict JSON serialization");

    merge_conflict_t conflict;
    memset(&conflict, 0, sizeof(conflict));
    strncpy(conflict.file, "test.c", sizeof(conflict.file) - 1);
    conflict.line_start = 10;
    conflict.line_end = 20;
    conflict.content_a = strdup("version A");
    conflict.content_b = strdup("version B");

    char* json = merge_conflict_to_json(&conflict);
    assert(json != NULL);
    assert(strstr(json, "\"file\":") != NULL);
    assert(strstr(json, "\"test.c\"") != NULL);
    assert(strstr(json, "\"line_start\": 10") != NULL);

    free(json);
    free(conflict.content_a);
    free(conflict.content_b);
    PASS();
}

int main(void) {
    printf("========================================\n");
    printf("ARGO WORKFLOW TESTS\n");
    printf("========================================\n\n");

    /* Workflow tests */
    test_workflow_lifecycle();
    test_workflow_start();
    test_task_creation();
    test_task_assignment();
    test_task_completion();
    test_phase_advancement();
    test_auto_assignment();

    /* Merge negotiation tests */
    test_merge_negotiation_lifecycle();
    test_merge_conflicts();
    test_conflict_proposals();
    test_negotiation_completion();
    test_conflict_json();

    /* Print summary */
    printf("\n========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    printf("========================================\n");

    return (tests_run == tests_passed) ? 0 : 1;
}

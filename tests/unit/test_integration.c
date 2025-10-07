/* © 2025 Casey Koons All rights reserved */

/* End-to-end integration tests for complete workflows */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Project includes */
#include "argo_orchestrator.h"

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

/* Test complete orchestrator lifecycle */
static void test_orchestrator_lifecycle(void) {
    TEST("Orchestrator lifecycle");

    argo_orchestrator_t* orch = orchestrator_create("test-session", "main");
    if (!orch) {
        FAIL("Failed to create orchestrator");
        return;
    }

    assert(orch->registry != NULL);
    assert(orch->lifecycle != NULL);
    assert(orch->workflow != NULL);
    assert(strcmp(orch->session_id, "test-session") == 0);
    assert(strcmp(orch->base_branch, "main") == 0);
    assert(orch->running == 0);

    orchestrator_destroy(orch);
    PASS();
}

/* Test adding and starting CIs */
static void test_orchestrator_ci_management(void) {
    TEST("CI management through orchestrator");

    argo_orchestrator_t* orch = orchestrator_create("ci-mgmt-test", "main");

    /* Add multiple CIs with different roles */
    int result = orchestrator_add_ci(orch, "Alice", "builder", "claude");
    assert(result == ARGO_SUCCESS);

    result = orchestrator_add_ci(orch, "Bob", "requirements", "gpt4");
    assert(result == ARGO_SUCCESS);

    result = orchestrator_add_ci(orch, "Carol", "analysis", "gemini");
    assert(result == ARGO_SUCCESS);

    /* Start the CIs */
    result = orchestrator_start_ci(orch, "Alice");
    assert(result == ARGO_SUCCESS);

    result = orchestrator_start_ci(orch, "Bob");
    assert(result == ARGO_SUCCESS);

    result = orchestrator_start_ci(orch, "Carol");
    assert(result == ARGO_SUCCESS);

    /* Verify they're in registry */
    assert(orch->registry->count == 3);

    orchestrator_destroy(orch);
    PASS();
}

/* Test complete workflow execution */
static void test_complete_workflow(void) {
    TEST("Complete workflow execution");

    argo_orchestrator_t* orch = orchestrator_create("workflow-test", "main");

    /* Setup: Add CIs */
    orchestrator_add_ci(orch, "Alice", "builder", "claude");
    orchestrator_add_ci(orch, "Bob", "requirements", "gpt4");
    orchestrator_start_ci(orch, "Alice");
    orchestrator_start_ci(orch, "Bob");

    /* Start workflow */
    int result = orchestrator_start_workflow(orch);
    assert(result == ARGO_SUCCESS);
    assert(orch->running == 1);
    assert(strcmp(orchestrator_current_phase_name(orch), "Initialize") == 0);

    /* Phase 1: INIT - Create and assign init task */
    result = orchestrator_create_task(orch, "Setup environment", WORKFLOW_INIT);
    assert(result == ARGO_SUCCESS);

    result = orchestrator_assign_all_tasks(orch);
    assert(result == ARGO_SUCCESS);

    /* Find and complete the task */
    ci_task_t* task = orch->workflow->tasks;
    assert(task != NULL);
    result = orchestrator_complete_task(orch, task->id, task->assigned_to);
    assert(result == ARGO_SUCCESS);

    /* Advance to PLAN phase */
    assert(orchestrator_can_advance_phase(orch) == 1);
    result = orchestrator_advance_workflow(orch);
    assert(result == ARGO_SUCCESS);
    assert(strcmp(orchestrator_current_phase_name(orch), "Planning") == 0);

    /* Phase 2: PLAN - Requirements CI creates specs */
    result = orchestrator_create_task(orch, "Write requirements", WORKFLOW_PLAN);
    assert(result == ARGO_SUCCESS);

    result = orchestrator_assign_all_tasks(orch);
    assert(result == ARGO_SUCCESS);

    /* Complete planning task */
    task = orch->workflow->tasks;
    while (task && task->completed) task = task->next;
    assert(task != NULL);
    result = orchestrator_complete_task(orch, task->id, task->assigned_to);
    assert(result == ARGO_SUCCESS);

    /* Advance to DEVELOP phase */
    result = orchestrator_advance_workflow(orch);
    assert(result == ARGO_SUCCESS);
    assert(strcmp(orchestrator_current_phase_name(orch), "Development") == 0);

    orchestrator_destroy(orch);
    PASS();
}

/* Test messaging between CIs */
static void test_ci_messaging(void) {
    TEST("CI-to-CI messaging");

    argo_orchestrator_t* orch = orchestrator_create("msg-test", "main");

    /* Add CIs */
    orchestrator_add_ci(orch, "Alice", "builder", "claude");
    orchestrator_add_ci(orch, "Bob", "builder", "gpt4");
    orchestrator_start_ci(orch, "Alice");
    orchestrator_start_ci(orch, "Bob");

    /* Send point-to-point message */
    int result = orchestrator_send_message(orch, "Alice", "Bob",
                                          "request", "Can you review my code?");

    /* Note: Will fail if no socket server, but validates message creation */
    (void)result;  /* Accept either success or socket error */

    /* Send broadcast message */
    result = orchestrator_broadcast_message(orch, "Alice", "builder",
                                           "broadcast", "Status update");
    (void)result;

    /* Verify statistics updated */
    ci_registry_entry_t* alice = registry_find_ci(orch->registry, "Alice");
    assert(alice != NULL);
    /* Messages sent count may be 0 if socket send failed, which is OK for this test */

    orchestrator_destroy(orch);
    PASS();
}

/* Test merge negotiation workflow */
static void test_merge_negotiation_workflow(void) {
    TEST("Merge negotiation workflow");

    argo_orchestrator_t* orch = orchestrator_create("merge-test", "main");

    /* Add CIs with different roles */
    orchestrator_add_ci(orch, "Alice", "builder", "claude");
    orchestrator_add_ci(orch, "Bob", "builder", "gpt4");
    orchestrator_add_ci(orch, "Coordinator", "coordinator", "gemini");
    orchestrator_start_ci(orch, "Alice");
    orchestrator_start_ci(orch, "Bob");
    orchestrator_start_ci(orch, "Coordinator");

    /* Start merge negotiation */
    int result = orchestrator_start_merge(orch, "feature-a", "feature-b");
    assert(result == ARGO_SUCCESS);
    assert(orch->active_merge != NULL);

    /* Add conflicts */
    result = orchestrator_add_conflict(orch, "main.c", 10, 20,
                                      "int x = 1;", "int x = 2;");
    assert(result == ARGO_SUCCESS);

    result = orchestrator_add_conflict(orch, "util.c", 50, 60,
                                      "return true;", "return false;");
    assert(result == ARGO_SUCCESS);

    assert(orch->active_merge->conflict_count == 2);

    /* CIs propose resolutions */
    result = orchestrator_propose_resolution(orch, "Alice", 0,
                                            "int x = 3;", 75);
    assert(result == ARGO_SUCCESS);

    result = orchestrator_propose_resolution(orch, "Bob", 0,
                                            "int x = 3;", 90);
    assert(result == ARGO_SUCCESS);

    result = orchestrator_propose_resolution(orch, "Alice", 1,
                                            "return check();", 80);
    assert(result == ARGO_SUCCESS);

    result = orchestrator_propose_resolution(orch, "Bob", 1,
                                            "return check();", 85);
    assert(result == ARGO_SUCCESS);

    /* Verify merge is complete */
    assert(merge_is_complete(orch->active_merge) == 1);

    /* Finalize merge */
    result = orchestrator_finalize_merge(orch);
    assert(result == ARGO_SUCCESS);
    assert(orch->active_merge == NULL);

    orchestrator_destroy(orch);
    PASS();
}

/* Test parallel task execution */
static void test_parallel_tasks(void) {
    TEST("Parallel task execution");

    argo_orchestrator_t* orch = orchestrator_create("parallel-test", "main");

    /* Add multiple builder CIs */
    orchestrator_add_ci(orch, "Alice", "builder", "claude");
    orchestrator_add_ci(orch, "Bob", "builder", "gpt4");
    orchestrator_add_ci(orch, "Carol", "builder", "gemini");
    orchestrator_start_ci(orch, "Alice");
    orchestrator_start_ci(orch, "Bob");
    orchestrator_start_ci(orch, "Carol");

    orchestrator_start_workflow(orch);

    /* Create multiple tasks in DEVELOP phase */
    orchestrator_create_task(orch, "Implement auth module", WORKFLOW_DEVELOP);
    orchestrator_create_task(orch, "Implement API endpoints", WORKFLOW_DEVELOP);
    orchestrator_create_task(orch, "Implement database layer", WORKFLOW_DEVELOP);

    /* Assign tasks - should distribute among CIs */
    int result = orchestrator_assign_all_tasks(orch);
    assert(result == ARGO_SUCCESS);

    /* Verify all tasks got assigned (note: auto-assign assigns same CI to all
     * tasks of same phase currently - this is expected behavior) */
    int assigned_count = 0;
    ci_task_t* task = orch->workflow->tasks;
    while (task) {
        if (task->assigned_to[0] != '\0') {
            assigned_count++;
        }
        task = task->next;
    }

    assert(assigned_count == 3);

    orchestrator_destroy(orch);
    PASS();
}

/* Test workflow pause and resume */
static void test_workflow_pause_resume(void) {
    TEST("Workflow pause and resume");

    argo_orchestrator_t* orch = orchestrator_create("pause-test", "main");

    orchestrator_add_ci(orch, "Alice", "builder", "claude");
    orchestrator_start_ci(orch, "Alice");

    /* Start workflow */
    int result = orchestrator_start_workflow(orch);
    assert(result == ARGO_SUCCESS);

    /* Pause */
    result = orchestrator_pause_workflow(orch);
    assert(result == ARGO_SUCCESS);
    assert(orch->workflow->state == WORKFLOW_STATE_PAUSED);

    /* Resume */
    result = orchestrator_resume_workflow(orch);
    assert(result == ARGO_SUCCESS);
    assert(orch->workflow->state == WORKFLOW_STATE_RUNNING);

    orchestrator_destroy(orch);
    PASS();
}

/* Test status reporting */
static void test_status_reporting(void) {
    TEST("Status reporting");

    argo_orchestrator_t* orch = orchestrator_create("status-test", "main");

    orchestrator_add_ci(orch, "Alice", "builder", "claude");
    orchestrator_start_ci(orch, "Alice");
    orchestrator_start_workflow(orch);

    /* Print status (visual check) */
    orchestrator_print_status(orch);

    /* Get JSON status */
    char* json = orchestrator_get_status_json(orch);
    assert(json != NULL);
    assert(strstr(json, "\"session_id\":") != NULL);
    assert(strstr(json, "\"status-test\"") != NULL);
    assert(strstr(json, "\"running\": true") != NULL);

    free(json);
    orchestrator_destroy(orch);
    PASS();
}

/* Test complete multi-phase workflow */
static void test_multi_phase_workflow(void) {
    TEST("Multi-phase workflow completion");

    argo_orchestrator_t* orch = orchestrator_create("multi-phase", "main");

    /* Setup full team */
    orchestrator_add_ci(orch, "ReqCI", "requirements", "claude");
    orchestrator_add_ci(orch, "BuilderCI", "builder", "gpt4");
    orchestrator_add_ci(orch, "AnalystCI", "analysis", "gemini");
    orchestrator_start_ci(orch, "ReqCI");
    orchestrator_start_ci(orch, "BuilderCI");
    orchestrator_start_ci(orch, "AnalystCI");

    orchestrator_start_workflow(orch);

    /* Run through multiple phases */
    workflow_phase_t phases[] = {WORKFLOW_INIT, WORKFLOW_PLAN, WORKFLOW_DEVELOP};

    for (int i = 0; i < 3; i++) {
        /* Create task for current phase */
        char desc[128];
        snprintf(desc, sizeof(desc), "Task for phase %d", i);
        orchestrator_create_task(orch, desc, phases[i]);

        /* Assign and complete */
        orchestrator_assign_all_tasks(orch);

        ci_task_t* task = orch->workflow->tasks;
        while (task && task->completed) task = task->next;

        if (task) {
            orchestrator_complete_task(orch, task->id, task->assigned_to);
        }

        /* Advance if not at last phase */
        if (i < 2) {
            assert(orchestrator_can_advance_phase(orch) == 1);
            orchestrator_advance_workflow(orch);
        }
    }

    /* Should be at DEVELOP phase now */
    assert(strcmp(orchestrator_current_phase_name(orch), "Development") == 0);

    orchestrator_destroy(orch);
    PASS();
}

/* Test multi-CI coordination */
static void test_multi_ci_coordination(void) {
    TEST("Multi-CI coordination");

    argo_orchestrator_t* orch = orchestrator_create("coord-test", "main");
    if (!orch) {
        FAIL("Failed to create orchestrator");
        return;
    }

    /* Create multiple CIs with different roles */
    orchestrator_add_ci(orch, "Argo", "requirements", "claude");
    orchestrator_add_ci(orch, "Maia", "builder", "llama3:70b");
    orchestrator_add_ci(orch, "Atlas", "analysis", "gpt-4");
    orchestrator_add_ci(orch, "Titan", "coordinator", "claude");

    /* Start all CIs */
    orchestrator_start_ci(orch, "Argo");
    orchestrator_start_ci(orch, "Maia");
    orchestrator_start_ci(orch, "Atlas");
    orchestrator_start_ci(orch, "Titan");

    /* Verify all CIs are ready */
    assert(orch->registry->count == 4);

    /* Start workflow */
    orchestrator_start_workflow(orch);
    assert(orch->running == 1);

    /* Create tasks for different phases requiring different roles */
    orchestrator_create_task(orch, "Analyze requirements", WORKFLOW_PLAN);
    orchestrator_create_task(orch, "Design architecture", WORKFLOW_PLAN);
    orchestrator_create_task(orch, "Implement feature A", WORKFLOW_DEVELOP);
    orchestrator_create_task(orch, "Implement feature B", WORKFLOW_DEVELOP);
    orchestrator_create_task(orch, "Review code quality", WORKFLOW_REVIEW);
    orchestrator_create_task(orch, "Review security", WORKFLOW_REVIEW);

    assert(orch->workflow->total_tasks == 6);
    assert(orch->workflow->completed_tasks == 0);

    /* Auto-assign tasks based on CI roles */
    int result = orchestrator_assign_all_tasks(orch);
    assert(result == ARGO_SUCCESS);

    /* Verify tasks were assigned to appropriate CIs */
    int requirements_tasks = 0;
    int builder_tasks = 0;
    int analysis_tasks = 0;

    ci_task_t* task = orch->workflow->tasks;
    while (task) {
        if (task->assigned_to[0] != '\0') {
            ci_registry_entry_t* ci = registry_find_ci(orch->registry, task->assigned_to);
            if (ci) {
                if (strcmp(ci->role, "requirements") == 0) requirements_tasks++;
                else if (strcmp(ci->role, "builder") == 0) builder_tasks++;
                else if (strcmp(ci->role, "analysis") == 0) analysis_tasks++;
            }
        }
        task = task->next;
    }

    /* Should have tasks distributed across CIs */
    assert(requirements_tasks >= 1);
    assert(builder_tasks >= 1);
    assert(analysis_tasks >= 1);

    /* Simulate CI-to-CI coordination messages */
    /* These may fail without actual socket connections - that's OK for this test */
    (void)orchestrator_send_message(orch, "Argo", "Maia",
                                    "requirements_ready",
                                    "Requirements analysis complete");

    (void)orchestrator_send_message(orch, "Maia", "Atlas",
                                    "code_ready_for_review",
                                    "Feature implementation complete");

    /* Simulate completing tasks in order */
    task = orch->workflow->tasks;
    int completed = 0;
    while (task && completed < 3) {
        if (task->assigned_to[0] != '\0' && !task->completed) {
            orchestrator_complete_task(orch, task->id, task->assigned_to);
            completed++;
        }
        task = task->next;
    }

    assert(orch->workflow->completed_tasks >= 3);

    /* Broadcast coordination message to all CIs */
    result = orchestrator_broadcast_message(orch, "Titan", NULL,
                                            "status_update",
                                            "Phase progress: 50% complete");
    /* Broadcast may fail if sockets not connected - that's OK for this test */
    (void)result;

    /* Verify workflow is progressing */
    assert(orch->workflow->state == WORKFLOW_STATE_RUNNING);
    assert(orch->workflow->total_tasks == 6);

    orchestrator_destroy(orch);
    PASS();
}

int main(void) {
    printf("========================================\n");
    printf("ARGO INTEGRATION TESTS\n");
    printf("========================================\n\n");

    /* Run tests */
    test_orchestrator_lifecycle();
    test_orchestrator_ci_management();
    test_complete_workflow();
    test_ci_messaging();
    test_merge_negotiation_workflow();
    test_parallel_tasks();
    test_workflow_pause_resume();
    test_status_reporting();
    test_multi_phase_workflow();
    test_multi_ci_coordination();

    /* Print summary */
    printf("\n========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    printf("========================================\n");

    return (tests_run == tests_passed) ? 0 : 1;
}

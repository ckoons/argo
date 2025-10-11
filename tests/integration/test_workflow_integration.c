/* © 2025 Casey Koons All rights reserved */
/* Full integration tests for workflow execution */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define JSMN_HEADER
#include "jsmn.h"

#include "argo_workflow.h"
#include "argo_workflow_executor.h"
#include "argo_workflow_context.h"
#include "argo_error.h"
#include "argo_log.h"
#include "argo_provider.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_START(name) \
    printf("\n=== INTEGRATION TEST: %s ===\n", name);

#define TEST_ASSERT(condition, message) \
    if (!(condition)) { \
        printf("FAIL: %s\n", message); \
        tests_failed++; \
        return -1; \
    } else { \
        printf("PASS: %s\n", message); \
        tests_passed++; \
    }

#define TEST_END() \
    printf("=== TEST COMPLETE ===\n\n");

/* Test: Display step with escape sequences */
static int test_display_step(void) {
    TEST_START("Display Step");

    workflow_controller_t* wf = workflow_create();
    TEST_ASSERT(wf != NULL, "Workflow created");

    int result = workflow_load_json(wf, "workflows/templates/test_basic_steps.json");
    TEST_ASSERT(result == ARGO_SUCCESS, "Workflow JSON loaded");

    /* Manually set context variables for testing */
    workflow_context_set(wf->context, "name", "Casey");
    workflow_context_set(wf->context, "age", "70");

    /* Execute workflow (should complete all display steps) */
    result = workflow_execute_all_steps(wf);
    TEST_ASSERT(result == ARGO_SUCCESS, "Workflow executed successfully");
    TEST_ASSERT(wf->step_count == 4, "Executed correct number of steps");

    workflow_destroy(wf);
    TEST_END();
    return 0;
}

/* Test: Variable substitution in display messages */
static int test_variable_substitution_integration(void) {
    TEST_START("Variable Substitution Integration");

    workflow_controller_t* wf = workflow_create();
    TEST_ASSERT(wf != NULL, "Workflow created");

    int result = workflow_load_json(wf, "workflows/templates/test_variable_flow.json");
    TEST_ASSERT(result == ARGO_SUCCESS, "Workflow JSON loaded");

    /* Set variables that should be substituted */
    workflow_context_set(wf->context, "name", "TestUser");
    workflow_context_set(wf->context, "age", "42");

    /* Execute workflow */
    result = workflow_execute_all_steps(wf);
    TEST_ASSERT(result == ARGO_SUCCESS, "Workflow executed");
    TEST_ASSERT(wf->step_count == 5, "All steps executed");

    /* Verify variables are still in context */
    const char* name = workflow_context_get(wf->context, "name");
    TEST_ASSERT(name != NULL, "name variable exists");
    TEST_ASSERT(strcmp(name, "TestUser") == 0, "name variable correct");

    workflow_destroy(wf);
    TEST_END();
    return 0;
}

/* Test: Step execution order */
static int test_step_execution_order(void) {
    TEST_START("Step Execution Order");

    workflow_controller_t* wf = workflow_create();
    TEST_ASSERT(wf != NULL, "Workflow created");

    /* Load a workflow with sequential steps */
    int result = workflow_load_json(wf, "workflows/templates/test_basic_steps.json");
    TEST_ASSERT(result == ARGO_SUCCESS, "Workflow loaded");

    /* Verify starts at step 1 */
    TEST_ASSERT(strcmp(wf->current_step_id, "1") == 0, "Starts at step 1");

    /* Execute first step */
    result = workflow_execute_current_step(wf);
    TEST_ASSERT(result == ARGO_SUCCESS, "Step 1 executed");
    TEST_ASSERT(strcmp(wf->current_step_id, "2") == 0, "Moved to step 2");

    /* Execute second step */
    result = workflow_execute_current_step(wf);
    TEST_ASSERT(result == ARGO_SUCCESS, "Step 2 executed");
    TEST_ASSERT(strcmp(wf->current_step_id, "3") == 0, "Moved to step 3");

    workflow_destroy(wf);
    TEST_END();
    return 0;
}

/* Test: Context persistence across steps */
static int test_context_persistence(void) {
    TEST_START("Context Persistence");

    workflow_controller_t* wf = workflow_create();
    TEST_ASSERT(wf != NULL, "Workflow created");

    int result = workflow_load_json(wf, "workflows/templates/test_basic_steps.json");
    TEST_ASSERT(result == ARGO_SUCCESS, "Workflow loaded");

    /* Set variable before execution */
    workflow_context_set(wf->context, "test_var", "initial");
    const char* val1 = workflow_context_get(wf->context, "test_var");
    TEST_ASSERT(strcmp(val1, "initial") == 0, "Variable set before execution");

    /* Execute one step */
    result = workflow_execute_current_step(wf);
    TEST_ASSERT(result == ARGO_SUCCESS, "Step executed");

    /* Verify variable persists */
    const char* val2 = workflow_context_get(wf->context, "test_var");
    TEST_ASSERT(val2 != NULL, "Variable still exists");
    TEST_ASSERT(strcmp(val2, "initial") == 0, "Variable value unchanged");

    /* Update variable */
    workflow_context_set(wf->context, "test_var", "updated");

    /* Execute another step */
    result = workflow_execute_current_step(wf);
    TEST_ASSERT(result == ARGO_SUCCESS, "Step executed");

    /* Verify updated value persists */
    const char* val3 = workflow_context_get(wf->context, "test_var");
    TEST_ASSERT(strcmp(val3, "updated") == 0, "Updated value persists");

    workflow_destroy(wf);
    TEST_END();
    return 0;
}

/* Test: Exit step handling */
static int test_exit_step(void) {
    TEST_START("Exit Step Handling");

    workflow_controller_t* wf = workflow_create();
    TEST_ASSERT(wf != NULL, "Workflow created");

    int result = workflow_load_json(wf, "workflows/templates/test_basic_steps.json");
    TEST_ASSERT(result == ARGO_SUCCESS, "Workflow loaded");

    /* Run to completion */
    result = workflow_execute_all_steps(wf);
    TEST_ASSERT(result == ARGO_SUCCESS, "Workflow completed");
    TEST_ASSERT(strcmp(wf->current_step_id, "EXIT") == 0, "Reached EXIT");

    workflow_destroy(wf);
    TEST_END();
    return 0;
}

/* Main test runner */
int main(void) {
    printf("\n");
    printf("=========================================\n");
    printf("Workflow Integration Test Suite\n");
    printf("=========================================\n");

    /* Change to project root directory */
    if (chdir("/Users/cskoons/projects/github/argo") != 0) {
        printf("ERROR: Could not change to project directory\n");
        return 1;
    }

    /* Initialize logging */
    log_init(NULL);
    log_set_level(LOG_ERROR);

    /* Run integration tests */
    test_display_step();
    test_variable_substitution_integration();
    test_step_execution_order();
    test_context_persistence();
    test_exit_step();

    /* Summary */
    printf("\n");
    printf("=========================================\n");
    printf("Integration Test Summary\n");
    printf("=========================================\n");
    printf("Tests Passed: %d\n", tests_passed);
    printf("Tests Failed: %d\n", tests_failed);
    printf("=========================================\n");

    if (tests_failed > 0) {
        printf("RESULT: FAILED\n");
        return 1;
    } else {
        printf("RESULT: SUCCESS\n");
        return 0;
    }
}

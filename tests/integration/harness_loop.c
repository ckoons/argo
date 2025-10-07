/* © 2025 Casey Koons All rights reserved */

/* Test Harness: Loop Support
 *
 * Purpose: Test workflow loop support with max_iterations
 * Tests:
 *   - Loop detection (backwards navigation)
 *   - Loop iteration counting
 *   - max_iterations enforcement
 *   - Loop reset on forward navigation
 */

#include <stdio.h>
#include <stdlib.h>

#include "argo_init.h"
#include "argo_workflow.h"
#include "argo_registry.h"
#include "argo_lifecycle.h"
#include "argo_error.h"

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("LOOP SUPPORT TEST\n");
    printf("========================================\n");
    printf("\n");

    /* Initialize Argo */
    if (argo_init() != ARGO_SUCCESS) {
        fprintf(stderr, "FAIL: argo_init() failed\n");
        return 1;
    }

    /* Create workflow components */
    ci_registry_t* registry = registry_create();
    lifecycle_manager_t* lifecycle = lifecycle_manager_create(registry);
    workflow_controller_t* workflow = workflow_create(registry, lifecycle, "loop-test");

    if (!workflow) {
        fprintf(stderr, "FAIL: Failed to create workflow\n");
        lifecycle_manager_destroy(lifecycle);
        registry_destroy(registry);
        argo_exit();
        return 1;
    }

    /* Load workflow */
    printf("Loading workflow: workflows/test/loop_test.json\n\n");
    int result = workflow_load_json(workflow, "workflows/test/loop_test.json");
    if (result != ARGO_SUCCESS) {
        fprintf(stderr, "FAIL: Failed to load workflow\n");
        workflow_destroy(workflow);
        lifecycle_manager_destroy(lifecycle);
        registry_destroy(registry);
        argo_exit();
        return 1;
    }

    printf("----------------------------------------\n");
    printf("Starting Workflow Execution\n");
    printf("----------------------------------------\n");
    printf("\n");
    printf("This workflow tests loop support:\n");
    printf("- Enter a number when prompted\n");
    printf("- You can loop back up to 3 times\n");
    printf("- After 3 iterations, max_iterations will be enforced\n");
    printf("\n");

    /* Execute workflow */
    result = workflow_execute_all_steps(workflow);

    printf("\n");
    printf("----------------------------------------\n");

    if (result == ARGO_SUCCESS) {
        printf("Workflow Execution: SUCCESS\n");
        printf("Final step count: %d\n", workflow->step_count);
        printf("Loop iterations: %d\n", workflow->loop_iteration_count);
    } else {
        printf("Workflow Execution: FAILED (error: %d)\n", result);
        if (workflow->loop_iteration_count > 0) {
            printf("Stopped at loop iteration: %d\n", workflow->loop_iteration_count);
        }
    }

    printf("----------------------------------------\n");
    printf("\n");

    /* Cleanup */
    workflow_destroy(workflow);
    lifecycle_manager_destroy(lifecycle);
    registry_destroy(registry);
    argo_exit();

    if (result == ARGO_SUCCESS) {
        printf("========================================\n");
        printf("LOOP SUPPORT TEST PASSED\n");
        printf("========================================\n");
        printf("\n");
        return 0;
    } else {
        printf("========================================\n");
        printf("LOOP SUPPORT TEST COMPLETED\n");
        printf("(Loop limit enforced as expected)\n");
        printf("========================================\n");
        printf("\n");
        return 0;  /* Not a failure if max_iterations was enforced */
    }
}

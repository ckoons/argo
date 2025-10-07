/* © 2025 Casey Koons All rights reserved */

/* Test Harness: CI Interactive Steps
 *
 * Purpose: Test workflow with CI-interactive steps
 * Tests:
 *   - ci_ask step (AI-assisted prompts)
 *   - ci_analyze step (AI analysis)
 *   - ci_ask_series step (multi-question interview)
 *   - ci_present step (AI-formatted presentation)
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
    printf("CI INTERACTIVE TEST\n");
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
    workflow_controller_t* workflow = workflow_create(registry, lifecycle, "ci-interactive-test");

    if (!workflow) {
        fprintf(stderr, "FAIL: Failed to create workflow\n");
        lifecycle_manager_destroy(lifecycle);
        registry_destroy(registry);
        argo_exit();
        return 1;
    }

    /* Note: CI provider can be set if available, but steps work without it */
    /* workflow->provider = some_ci_provider; */

    /* Load workflow */
    printf("Loading workflow: workflows/test/ci_interactive_test.json\n\n");
    int result = workflow_load_json(workflow, "workflows/test/ci_interactive_test.json");
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

    /* Execute workflow */
    result = workflow_execute_all_steps(workflow);

    printf("\n");
    printf("----------------------------------------\n");

    if (result == ARGO_SUCCESS) {
        printf("Workflow Execution: SUCCESS\n");
    } else {
        printf("Workflow Execution: FAILED (error: %d)\n", result);
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
        printf("CI INTERACTIVE TEST PASSED\n");
        printf("========================================\n");
        printf("\n");
        return 0;
    } else {
        printf("========================================\n");
        printf("CI INTERACTIVE TEST FAILED\n");
        printf("========================================\n");
        printf("\n");
        return 1;
    }
}

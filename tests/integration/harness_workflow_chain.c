/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Project includes */
#include "argo_workflow.h"
#include "argo_registry.h"
#include "argo_lifecycle.h"
#include "argo_error.h"
#include "argo_log.h"

#define PARENT_WORKFLOW_PATH "workflows/test/parent_workflow.json"
#define CHILD_WORKFLOW_PATH "workflows/test/child_validator.json"

int main(void) {
    printf("========================================\n");
    printf("ARGO WORKFLOW CHAINING TEST HARNESS\n");
    printf("========================================\n");

    /* Create registry and lifecycle manager */
    ci_registry_t* registry = registry_create();
    if (!registry) {
        printf("Failed to create registry\n");
        return 1;
    }

    lifecycle_manager_t* lifecycle = lifecycle_manager_create(registry);
    if (!lifecycle) {
        printf("Failed to create lifecycle manager\n");
        registry_destroy(registry);
        return 1;
    }

    /* Test 1: Load child workflow directly */
    printf("\n========================================\n");
    printf("TEST 1: Load and verify child workflow\n");
    printf("========================================\n");

    workflow_controller_t* child = workflow_create(registry, lifecycle, "child_test");
    if (!child) {
        printf("Failed to create child workflow\n");
        lifecycle_manager_destroy(lifecycle);
        registry_destroy(registry);
        return 1;
    }

    int result = workflow_load_json(child, CHILD_WORKFLOW_PATH);
    if (result != ARGO_SUCCESS) {
        printf("Failed to load child workflow (error: %d)\n", result);
        workflow_destroy(child);
        lifecycle_manager_destroy(lifecycle);
        registry_destroy(registry);
        return 1;
    }

    printf("✓ Child workflow loaded successfully\n");
    printf("  Steps: %d\n", child->token_count);
    workflow_destroy(child);

    /* Test 2: Load parent workflow */
    printf("\n========================================\n");
    printf("TEST 2: Load and verify parent workflow\n");
    printf("========================================\n");

    workflow_controller_t* parent = workflow_create(registry, lifecycle, "parent_test");
    if (!parent) {
        printf("Failed to create parent workflow\n");
        lifecycle_manager_destroy(lifecycle);
        registry_destroy(registry);
        return 1;
    }

    result = workflow_load_json(parent, PARENT_WORKFLOW_PATH);
    if (result != ARGO_SUCCESS) {
        printf("Failed to load parent workflow (error: %d)\n", result);
        workflow_destroy(parent);
        lifecycle_manager_destroy(lifecycle);
        registry_destroy(registry);
        return 1;
    }

    printf("✓ Parent workflow loaded successfully\n");
    printf("  Steps: %d\n", parent->token_count);
    printf("  Initial recursion depth: %d\n", parent->recursion_depth);

    /* Test 3: Verify recursion depth initialization */
    printf("\n========================================\n");
    printf("TEST 3: Verify recursion tracking\n");
    printf("========================================\n");

    if (parent->recursion_depth != 0) {
        printf("ERROR: Initial recursion depth should be 0, got %d\n", parent->recursion_depth);
        workflow_destroy(parent);
        lifecycle_manager_destroy(lifecycle);
        registry_destroy(registry);
        return 1;
    }

    printf("✓ Recursion depth correctly initialized to 0\n");

    /* Cleanup */
    workflow_destroy(parent);

    /* Note: Interactive execution would require stdin input */
    printf("\n========================================\n");
    printf("To run the full workflow chain, use:\n");
    printf("  echo \"test data\" | build/harness_workflow_chain\n");
    printf("========================================\n");

    lifecycle_manager_destroy(lifecycle);
    registry_destroy(registry);

    printf("\n✓ Workflow chaining test complete\n");
    return 0;
}

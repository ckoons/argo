/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Project includes */
#include "argo_workflow.h"
#include "argo_workflow_persona.h"
#include "argo_registry.h"
#include "argo_lifecycle.h"
#include "argo_error.h"
#include "argo_log.h"

#define WORKFLOW_PATH "workflows/test/persona_test.json"

/* Test persona registry parsing */
static int test_persona_registry(workflow_controller_t* workflow) {
    printf("\n========================================\n");
    printf("PERSONA REGISTRY TEST\n");
    printf("========================================\n");

    if (!workflow->personas) {
        printf("ERROR: Persona registry is NULL\n");
        return -1;
    }

    printf("Personas loaded: %d\n", workflow->personas->count);

    /* Test finding each persona */
    const char* persona_names[] = {"maia", "alex", "kai"};
    for (int i = 0; i < 3; i++) {
        workflow_persona_t* p = persona_registry_find(workflow->personas, persona_names[i]);
        if (!p) {
            printf("ERROR: Persona '%s' not found\n", persona_names[i]);
            return -1;
        }
        printf("\nPersona: %s\n", p->name);
        printf("  Role: %s\n", p->role);
        printf("  Style: %s\n", p->style);
        printf("  Greeting: %s\n", p->greeting);
    }

    /* Test default persona */
    workflow_persona_t* default_p = persona_registry_get_default(workflow->personas);
    if (!default_p) {
        printf("ERROR: Default persona not found\n");
        return -1;
    }
    printf("\nDefault persona: %s\n", default_p->name);

    printf("\n✓ Persona registry test passed\n");
    return 0;
}

int main(void) {
    printf("========================================\n");
    printf("ARGO PERSONA WORKFLOW TEST HARNESS\n");
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

    /* Create workflow */
    workflow_controller_t* workflow = workflow_create(registry, lifecycle, "persona_test");
    if (!workflow) {
        printf("Failed to create workflow\n");
        lifecycle_manager_destroy(lifecycle);
        registry_destroy(registry);
        return 1;
    }

    /* Load JSON workflow */
    printf("\nLoading workflow from: %s\n", WORKFLOW_PATH);
    int result = workflow_load_json(workflow, WORKFLOW_PATH);
    if (result != ARGO_SUCCESS) {
        printf("Failed to load workflow (error: %d)\n", result);
        workflow_destroy(workflow);
        lifecycle_manager_destroy(lifecycle);
        registry_destroy(registry);
        return 1;
    }

    /* Test persona registry */
    result = test_persona_registry(workflow);
    if (result != 0) {
        workflow_destroy(workflow);
        lifecycle_manager_destroy(lifecycle);
        registry_destroy(registry);
        return 1;
    }

    /* Note: Interactive execution would require stdin input */
    printf("\n========================================\n");
    printf("To run the interactive workflow, use:\n");
    printf("  echo -e \"test feature\\nC\\nyes\" | build/harness_persona\n");
    printf("========================================\n");

    /* Cleanup */
    workflow_destroy(workflow);
    lifecycle_manager_destroy(lifecycle);
    registry_destroy(registry);

    printf("\n✓ Persona system test complete\n");
    return 0;
}

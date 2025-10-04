/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "arc_commands.h"
#include "arc_context.h"
#include "argo_workflow_registry.h"
#include "argo_init.h"
#include "argo_error.h"

#define WORKFLOW_REGISTRY_PATH ".argo/workflows/registry/active_workflow_registry.json"

/* arc switch command handler */
int arc_cmd_switch(int argc, char** argv) {
    if (argc < 1 || !argv[0]) {
        fprintf(stderr, "Error: workflow name required\n");
        fprintf(stderr, "Usage: arc switch [workflow_name]\n");
        return ARC_EXIT_ERROR;
    }

    const char* workflow_name = argv[0];

    /* Initialize argo (needed to access registry) */
    int result = argo_init();
    if (result != ARGO_SUCCESS) {
        fprintf(stderr, "Error: Failed to initialize argo\n");
        return ARC_EXIT_ERROR;
    }

    /* Load workflow registry */
    workflow_registry_t* registry = workflow_registry_create(WORKFLOW_REGISTRY_PATH);
    if (!registry) {
        fprintf(stderr, "Error: Failed to create workflow registry\n");
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    result = workflow_registry_load(registry);
    if (result != ARGO_SUCCESS) {
        fprintf(stderr, "Error: Failed to load workflow registry\n");
        workflow_registry_destroy(registry);
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    /* Validate workflow exists */
    workflow_instance_t* workflow = workflow_registry_get_workflow(registry, workflow_name);
    if (!workflow) {
        fprintf(stderr, "Error: Workflow not found: %s\n", workflow_name);
        workflow_registry_destroy(registry);
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    /* Update last_active timestamp */
    workflow->last_active = time(NULL);
    workflow_registry_save(registry);

    /* Set context */
    arc_context_set(workflow_name);

    /* Print confirmation (not filtered by wrapper) */
    fprintf(stderr, "Switched to workflow: %s\n", workflow_name);

    /* Cleanup */
    workflow_registry_destroy(registry);
    argo_exit();

    return ARC_EXIT_SUCCESS;
}

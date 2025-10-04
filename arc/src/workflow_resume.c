/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arc_commands.h"
#include "arc_context.h"
#include "argo_workflow_registry.h"
#include "argo_init.h"
#include "argo_error.h"

#define WORKFLOW_REGISTRY_PATH ".argo/workflows/registry/active_workflow_registry.json"

/* arc workflow resume command handler */
int arc_workflow_resume(int argc, char** argv) {
    const char* workflow_name = NULL;

    /* Get workflow name from arg or context */
    if (argc >= 1) {
        workflow_name = argv[0];
    } else {
        workflow_name = arc_context_get();
        if (!workflow_name) {
            fprintf(stderr, "Error: No active workflow context\n");
            fprintf(stderr, "Usage: arc workflow resume [workflow_name]\n");
            fprintf(stderr, "   or: arc switch [workflow_name] && arc workflow resume\n");
            return ARC_EXIT_ERROR;
        }
    }

    /* Initialize argo */
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

    /* Get workflow */
    workflow_instance_t* wf = workflow_registry_get_workflow(registry, workflow_name);
    if (!wf) {
        fprintf(stderr, "Error: Workflow not found: %s\n", workflow_name);
        workflow_registry_destroy(registry);
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    /* Check if already running */
    if (wf->status == WORKFLOW_STATUS_ACTIVE) {
        fprintf(stderr, "Workflow already running: %s\n", workflow_name);
        workflow_registry_destroy(registry);
        argo_exit();
        return ARC_EXIT_SUCCESS;
    }

    /* TODO: Signal argo orchestrator to resume from checkpoint */
    /* For now, just update registry status */

    /* Update status to active */
    result = workflow_registry_set_status(registry, workflow_name, WORKFLOW_STATUS_ACTIVE);
    if (result != ARGO_SUCCESS) {
        fprintf(stderr, "Error: Failed to update workflow status\n");
        workflow_registry_destroy(registry);
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    /* Print confirmation */
    fprintf(stderr, "Resuming workflow: %s\n", workflow_name);
    fprintf(stderr, "\nNote: Workflow execution control not yet implemented.\n");
    fprintf(stderr, "      Status updated in registry only.\n");

    /* Cleanup */
    workflow_registry_destroy(registry);
    argo_exit();

    return ARC_EXIT_SUCCESS;
}

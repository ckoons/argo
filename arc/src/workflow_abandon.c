/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arc_commands.h"
#include "arc_context.h"
#include "argo_workflow_registry.h"
#include "argo_orchestrator_api.h"
#include "argo_init.h"
#include "argo_error.h"

#define WORKFLOW_REGISTRY_PATH ".argo/workflows/registry/active_workflow_registry.json"

/* Get confirmation from user */
static int get_confirmation(const char* workflow_name) {
    char response[10];
    fprintf(stderr, "Abandon workflow '%s'? (y/N): ", workflow_name);
    if (fgets(response, sizeof(response), stdin) == NULL) {
        return 0;
    }
    return (response[0] == 'y' || response[0] == 'Y');
}

/* arc workflow abandon command handler */
int arc_workflow_abandon(int argc, char** argv) {
    const char* workflow_name = NULL;

    /* Get workflow name from arg or context */
    if (argc >= 1) {
        workflow_name = argv[0];
    } else {
        workflow_name = arc_context_get();
        if (!workflow_name) {
            fprintf(stderr, "Error: No active workflow context\n");
            fprintf(stderr, "Usage: arc workflow abandon [workflow_name]\n");
            fprintf(stderr, "   or: arc switch [workflow_name] && arc workflow abandon\n");
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
        fprintf(stderr, "  Try: arc workflow list\n");
        workflow_registry_destroy(registry);
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    /* Get confirmation */
    if (!get_confirmation(workflow_name)) {
        fprintf(stderr, "Abandon cancelled.\n");
        workflow_registry_destroy(registry);
        argo_exit();
        return ARC_EXIT_SUCCESS;
    }

    /* Kill workflow process */
    result = workflow_exec_abandon(workflow_name, registry);
    if (result != ARGO_SUCCESS) {
        fprintf(stderr, "Error: Failed to terminate workflow process\n");
        workflow_registry_destroy(registry);
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    /* Remove workflow from registry */
    result = workflow_registry_remove_workflow(registry, workflow_name);
    if (result != ARGO_SUCCESS) {
        fprintf(stderr, "Error: Failed to remove workflow from registry\n");
        workflow_registry_destroy(registry);
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    /* Clear context if this was the active workflow */
    const char* current_context = arc_context_get();
    if (current_context && strcmp(current_context, workflow_name) == 0) {
        arc_context_clear();
    }

    /* Print confirmation */
    fprintf(stderr, "Abandoned workflow: %s\n", workflow_name);
    fprintf(stderr, "Logs preserved: ~/.argo/logs/%s.log\n", workflow_name);

    /* Cleanup */
    workflow_registry_destroy(registry);
    argo_exit();

    return ARC_EXIT_SUCCESS;
}

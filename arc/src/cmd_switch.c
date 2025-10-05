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
#include "argo_output.h"

#define WORKFLOW_REGISTRY_PATH ".argo/workflows/registry/active_workflow_registry.json"

/* arc switch command handler - sets active workflow and attaches */
int arc_cmd_switch(int argc, char** argv) {
    if (argc < 1 || !argv[0]) {
        LOG_USER_ERROR("workflow_id required\n");
        LOG_USER_INFO("Usage: arc switch <workflow_id>\n");
        return ARC_EXIT_ERROR;
    }

    const char* workflow_id = argv[0];

    /* Initialize argo */
    int result = argo_init();
    if (result != ARGO_SUCCESS) {
        LOG_USER_ERROR("Failed to initialize argo\n");
        return ARC_EXIT_ERROR;
    }

    /* Load workflow registry */
    workflow_registry_t* registry = workflow_registry_create(WORKFLOW_REGISTRY_PATH);
    if (!registry) {
        LOG_USER_ERROR("Failed to create workflow registry\n");
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    result = workflow_registry_load(registry);
    if (result != ARGO_SUCCESS) {
        LOG_USER_ERROR("Failed to load workflow registry\n");
        workflow_registry_destroy(registry);
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    /* Verify workflow exists */
    workflow_instance_t* wf = workflow_registry_get_workflow(registry, workflow_id);
    if (!wf) {
        LOG_USER_ERROR("Workflow not found: %s\n", workflow_id);
        LOG_USER_INFO("Try: arc list\n");
        workflow_registry_destroy(registry);
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    /* Update last_active timestamp */
    wf->last_active = time(NULL);
    workflow_registry_save(registry);

    /* Set ARGO_ACTIVE_WORKFLOW environment variable */
    setenv("ARGO_ACTIVE_WORKFLOW", workflow_id, 1);

    LOG_USER_SUCCESS("Switched to workflow: %s\n", workflow_id);
    LOG_USER_INFO("(Active workflow set for this terminal)\n");

    /* Cleanup registry */
    workflow_registry_destroy(registry);
    argo_exit();

    /* Now attach to the workflow */
    LOG_USER_INFO("\n");
    return arc_workflow_attach(0, NULL);  /* No args, uses ARGO_ACTIVE_WORKFLOW */
}

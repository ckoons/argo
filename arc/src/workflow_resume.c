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
#include "argo_output.h"

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
            LOG_USER_ERROR("No active workflow context\n");
            LOG_USER_INFO("Usage: arc workflow resume [workflow_name]\n");
            LOG_USER_INFO("   or: arc switch [workflow_name] && arc workflow resume\n");
            return ARC_EXIT_ERROR;
        }
    }

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

    /* Get workflow */
    workflow_instance_t* wf = workflow_registry_get_workflow(registry, workflow_name);
    if (!wf) {
        LOG_USER_ERROR("Workflow not found: %s\n", workflow_name);
        LOG_USER_INFO("  Try: arc workflow list\n");
        workflow_registry_destroy(registry);
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    /* Check if already running */
    if (wf->status == WORKFLOW_STATUS_ACTIVE) {
        LOG_USER_INFO("Workflow already running: %s\n", workflow_name);
        workflow_registry_destroy(registry);
        argo_exit();
        return ARC_EXIT_SUCCESS;
    }

    /* Send resume signal to workflow process */
    result = workflow_exec_resume(workflow_name, registry);
    if (result != ARGO_SUCCESS) {
        LOG_USER_ERROR("Failed to resume workflow\n");
        workflow_registry_destroy(registry);
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    /* Update status to active */
    result = workflow_registry_set_status(registry, workflow_name, WORKFLOW_STATUS_ACTIVE);
    if (result != ARGO_SUCCESS) {
        LOG_USER_ERROR("Failed to update workflow status\n");
        workflow_registry_destroy(registry);
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    /* Print confirmation */
    LOG_USER_SUCCESS("Resumed workflow: %s\n", workflow_name);

    /* Cleanup */
    workflow_registry_destroy(registry);
    argo_exit();

    return ARC_EXIT_SUCCESS;
}

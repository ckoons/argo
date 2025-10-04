/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arc_commands.h"
#include "arc_context.h"
#include "arc_error.h"
#include "argo_workflow_registry.h"
#include "argo_init.h"
#include "argo_error.h"

#define WORKFLOW_REGISTRY_PATH ".argo/workflows/registry/active_workflow_registry.json"

/* arc workflow start command handler */
int arc_workflow_start(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Error: template and instance name required\n");
        fprintf(stderr, "Usage: arc workflow start [template] [instance]\n");
        return ARC_EXIT_ERROR;
    }

    const char* template_name = argv[0];
    const char* instance_name = argv[1];
    const char* branch = (argc >= 3) ? argv[2] : "main";

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
    if (result != ARGO_SUCCESS && result != E_SYSTEM_FILE) {
        fprintf(stderr, "Error: Failed to load workflow registry\n");
        workflow_registry_destroy(registry);
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    /* Add workflow to registry */
    result = workflow_registry_add_workflow(registry, template_name, instance_name, branch);
    if (result == E_DUPLICATE) {
        fprintf(stderr, "Error: Workflow already exists: %s_%s\n", template_name, instance_name);
        fprintf(stderr, "  Try: arc workflow list\n");
        fprintf(stderr, "  Or use: arc workflow abandon %s_%s\n", template_name, instance_name);
        workflow_registry_destroy(registry);
        argo_exit();
        return ARC_EXIT_ERROR;
    } else if (result != ARGO_SUCCESS) {
        char context[128];
        snprintf(context, sizeof(context), "Creating workflow %s_%s", template_name, instance_name);
        arc_report_error(result, context, "Check that the .argo directory is writable");
        workflow_registry_destroy(registry);
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    /* Create workflow ID */
    char workflow_id[128];
    snprintf(workflow_id, sizeof(workflow_id), "%s_%s", template_name, instance_name);

    /* TODO: Start workflow execution via argo orchestrator */
    /* For now, workflow is created in registry but not actually executing */

    /* Print confirmation */
    fprintf(stderr, "Started workflow: %s\n", workflow_id);
    fprintf(stderr, "Logs: ~/.argo/logs/%s.log\n", workflow_id);
    fprintf(stderr, "\nNote: Workflow execution not yet implemented.\n");
    fprintf(stderr, "      Workflow created in registry only.\n");

    /* Cleanup */
    workflow_registry_destroy(registry);
    argo_exit();

    return ARC_EXIT_SUCCESS;
}

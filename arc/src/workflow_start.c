/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arc_commands.h"
#include "arc_context.h"
#include "arc_error.h"
#include "argo_workflow_registry.h"
#include "argo_workflow_templates.h"
#include "argo_orchestrator_api.h"
#include "argo_init.h"
#include "argo_error.h"
#include "argo_output.h"

#define WORKFLOW_REGISTRY_PATH ".argo/workflows/registry/active_workflow_registry.json"

/* arc workflow start command handler */
int arc_workflow_start(int argc, char** argv) {
    if (argc < 2) {
        LOG_USER_ERROR("template and instance name required\n");
        LOG_USER_INFO("Usage: arc workflow start [template] [instance] [branch] [--env <env>]\n");
        return ARC_EXIT_ERROR;
    }

    const char* template_name = argv[0];
    const char* instance_name = argv[1];
    const char* branch = (argc >= 3) ? argv[2] : "main";

    /* Get environment for workflow creation (defaults to 'dev') */
    const char* environment = arc_get_environment_for_creation(argc, argv);

    /* Initialize argo */
    int result = argo_init();
    if (result != ARGO_SUCCESS) {
        LOG_USER_ERROR("Failed to initialize argo\n");
        return ARC_EXIT_ERROR;
    }

    /* Validate template exists */
    workflow_template_collection_t* templates = workflow_templates_create();
    if (!templates) {
        LOG_USER_ERROR("Failed to create template collection\n");
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    result = workflow_templates_discover(templates);
    if (result != ARGO_SUCCESS) {
        LOG_USER_ERROR("Failed to discover templates\n");
        workflow_templates_destroy(templates);
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    workflow_template_t* template = workflow_templates_find(templates, template_name);
    if (!template) {
        LOG_USER_ERROR("Template not found: %s\n", template_name);
        LOG_USER_INFO("  Try: arc workflow list template\n");
        workflow_templates_destroy(templates);
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    /* Validate template file */
    result = workflow_template_validate(template->path);
    if (result != ARGO_SUCCESS) {
        LOG_USER_ERROR("Template is invalid: %s\n", template_name);
        LOG_USER_INFO("  Path: %s\n", template->path);
        workflow_templates_destroy(templates);
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    workflow_templates_destroy(templates);

    /* Load workflow registry */
    workflow_registry_t* registry = workflow_registry_create(WORKFLOW_REGISTRY_PATH);
    if (!registry) {
        LOG_USER_ERROR("Failed to create workflow registry\n");
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    result = workflow_registry_load(registry);
    if (result != ARGO_SUCCESS && result != E_SYSTEM_FILE) {
        LOG_USER_ERROR("Failed to load workflow registry\n");
        workflow_registry_destroy(registry);
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    /* Add workflow to registry */
    result = workflow_registry_add_workflow(registry, template_name, instance_name, branch, environment);
    if (result == E_DUPLICATE) {
        LOG_USER_ERROR("Workflow already exists: %s_%s\n", template_name, instance_name);
        LOG_USER_INFO("  Try: arc workflow list\n");
        LOG_USER_INFO("  Or use: arc workflow abandon %s_%s\n", template_name, instance_name);
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

    /* Start workflow execution */
    result = workflow_exec_start(workflow_id, template->path, branch, registry);
    if (result != ARGO_SUCCESS) {
        LOG_USER_ERROR("Failed to start workflow execution\n");
        workflow_registry_remove_workflow(registry, workflow_id);
        workflow_registry_destroy(registry);
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    /* Print confirmation */
    LOG_USER_SUCCESS("Started workflow: %s (environment: %s)\n", workflow_id, environment);
    LOG_USER_INFO("Logs: ~/.argo/logs/%s.log\n", workflow_id);

    /* Cleanup */
    workflow_registry_destroy(registry);
    argo_exit();

    return ARC_EXIT_SUCCESS;
}

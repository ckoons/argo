/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "arc_commands.h"
#include "arc_context.h"
#include "arc_constants.h"
#include "argo_workflow_registry.h"
#include "argo_workflow_templates.h"
#include "argo_init.h"
#include "argo_error.h"
#include "argo_output.h"

#define WORKFLOW_REGISTRY_PATH ".argo/workflows/registry/active_workflow_registry.json"

/* Format time duration */
static void format_duration(time_t seconds, char* buffer, size_t size) {
    if (seconds < SECONDS_PER_MINUTE) {
        snprintf(buffer, size, "%lds", (long)seconds);
    } else if (seconds < SECONDS_PER_HOUR) {
        snprintf(buffer, size, "%ldm %lds", (long)(seconds / SECONDS_PER_MINUTE), (long)(seconds % SECONDS_PER_MINUTE));
    } else {
        snprintf(buffer, size, "%ldh %ldm", (long)(seconds / SECONDS_PER_HOUR), (long)((seconds % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE));
    }
}

/* List active workflows */
static int list_active_workflows(workflow_registry_t* registry, const char* environment) {
    const char* context = arc_context_get();
    workflow_instance_t* workflows;
    int count;

    int result = workflow_registry_list_filtered(registry, environment, &workflows, &count);
    if (result != ARGO_SUCCESS) {
        return ARC_EXIT_ERROR;
    }

    if (count == 0) {
        LOG_USER_STATUS("\nNo active workflows.\n");
        LOG_USER_STATUS("Use 'arc workflow start' to create a workflow.\n\n");
        return ARC_EXIT_SUCCESS;
    }

    LOG_USER_STATUS("\nACTIVE WORKFLOWS:\n");
    LOG_USER_STATUS("%-8s %-30s %-16s %-12s %-8s %-12s\n",
           "CONTEXT", "NAME", "TEMPLATE", "INSTANCE", "STATUS", "TIME");
    LOG_USER_STATUS("------------------------------------------------------------------------\n");

    time_t now = time(NULL);
    for (int i = 0; i < count; i++) {
        workflow_instance_t* wf = &workflows[i];
        char duration[32];
        format_duration(now - wf->created_at, duration, sizeof(duration));

        const char* mark = (context && strcmp(context, wf->id) == 0) ? "*" : " ";

        LOG_USER_STATUS("%-8s %-30s %-16s %-12s %-8s %-12s\n",
               mark,
               wf->id,
               wf->template_name,
               wf->instance_name,
               workflow_status_string(wf->status),
               duration);
    }

    LOG_USER_STATUS("\n");
    return ARC_EXIT_SUCCESS;
}

/* List templates */
static int list_templates(void) {
    /* Discover templates */
    workflow_template_collection_t* templates = workflow_templates_create();
    if (!templates) {
        LOG_USER_ERROR("Failed to create template collection\n");
        return ARC_EXIT_ERROR;
    }

    int result = workflow_templates_discover(templates);
    if (result != ARGO_SUCCESS) {
        LOG_USER_WARN("Failed to discover templates\n");
        workflow_templates_destroy(templates);
        return ARC_EXIT_ERROR;
    }

    if (templates->count == 0) {
        LOG_USER_STATUS("\nNo templates found.\n");
        LOG_USER_STATUS("  System templates: workflows/templates/\n");
        LOG_USER_STATUS("  User templates:   ~/.argo/workflows/templates/\n\n");
        workflow_templates_destroy(templates);
        return ARC_EXIT_SUCCESS;
    }

    LOG_USER_STATUS("\nTEMPLATES:\n");
    LOG_USER_STATUS("%-8s %-20s %-40s\n", "SCOPE", "NAME", "DESCRIPTION");
    LOG_USER_STATUS("------------------------------------------------------------------------\n");

    for (int i = 0; i < templates->count; i++) {
        workflow_template_t* tmpl = &templates->templates[i];
        LOG_USER_STATUS("%-8s %-20s %-40s\n",
               tmpl->is_system ? "system" : "user",
               tmpl->name,
               tmpl->description);
    }

    LOG_USER_STATUS("\n");
    workflow_templates_destroy(templates);
    return ARC_EXIT_SUCCESS;
}

/* arc workflow list command handler */
int arc_workflow_list(int argc, char** argv) {
    const char* filter = NULL;
    if (argc >= 1) {
        filter = argv[0];
    }

    /* Get effective environment filter */
    const char* environment = arc_get_effective_environment(argc, argv);

    /* Initialize argo */
    int result = argo_init();
    if (result != ARGO_SUCCESS) {
        LOG_USER_ERROR("Failed to initialize argo\n");
        return ARC_EXIT_ERROR;
    }

    /* Templates only */
    if (filter && strcmp(filter, "template") == 0) {
        result = list_templates();
        argo_exit();
        return result;
    }

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

    /* Active workflows only OR all (active + templates) */
    int exit_code = list_active_workflows(registry, environment);

    /* If no filter OR filter is not "active", also show templates */
    if (!filter || strcmp(filter, "active") != 0) {
        if (exit_code == ARC_EXIT_SUCCESS) {
            list_templates();
        }
    }

    /* Cleanup */
    workflow_registry_destroy(registry);
    argo_exit();

    return exit_code;
}

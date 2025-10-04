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

/* Format time duration */
static void format_duration(time_t seconds, char* buffer, size_t size) {
    if (seconds < 60) {
        snprintf(buffer, size, "%lds", (long)seconds);
    } else if (seconds < 3600) {
        snprintf(buffer, size, "%ldm %lds", (long)(seconds / 60), (long)(seconds % 60));
    } else {
        snprintf(buffer, size, "%ldh %ldm", (long)(seconds / 3600), (long)((seconds % 3600) / 60));
    }
}

/* List active workflows */
static int list_active_workflows(workflow_registry_t* registry) {
    const char* context = arc_context_get();
    workflow_instance_t* workflows;
    int count;

    int result = workflow_registry_list(registry, &workflows, &count);
    if (result != ARGO_SUCCESS) {
        return ARC_EXIT_ERROR;
    }

    if (count == 0) {
        printf("\nNo active workflows.\n");
        printf("Use 'arc workflow start' to create a workflow.\n\n");
        return ARC_EXIT_SUCCESS;
    }

    printf("\nACTIVE WORKFLOWS:\n");
    printf("%-8s %-30s %-16s %-12s %-8s %-12s\n",
           "CONTEXT", "NAME", "TEMPLATE", "INSTANCE", "STATUS", "TIME");
    printf("------------------------------------------------------------------------\n");

    time_t now = time(NULL);
    for (int i = 0; i < count; i++) {
        workflow_instance_t* wf = &workflows[i];
        char duration[32];
        format_duration(now - wf->created_at, duration, sizeof(duration));

        const char* mark = (context && strcmp(context, wf->id) == 0) ? "*" : " ";

        printf("%-8s %-30s %-16s %-12s %-8s %-12s\n",
               mark,
               wf->id,
               wf->template_name,
               wf->instance_name,
               workflow_status_string(wf->status),
               duration);
    }

    printf("\n");
    return ARC_EXIT_SUCCESS;
}

/* List templates (stubbed for now) */
static int list_templates(void) {
    printf("\nTEMPLATES:\n");
    printf("%-8s %-20s %-40s\n", "SCOPE", "NAME", "DESCRIPTION");
    printf("------------------------------------------------------------------------\n");
    printf("%-8s %-20s %-40s\n", "system", "create_proposal", "Create GitHub issue/PR proposal");
    printf("%-8s %-20s %-40s\n", "system", "fix_bug", "Debug and fix reported issues");
    printf("%-8s %-20s %-40s\n", "system", "refactor", "Code refactoring workflow");
    printf("\n");
    printf("Note: Template discovery not yet implemented\n\n");
    return ARC_EXIT_SUCCESS;
}

/* arc workflow list command handler */
int arc_workflow_list(int argc, char** argv) {
    const char* filter = NULL;
    if (argc >= 1) {
        filter = argv[0];
    }

    /* Initialize argo */
    int result = argo_init();
    if (result != ARGO_SUCCESS) {
        fprintf(stderr, "Error: Failed to initialize argo\n");
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

    /* Active workflows only OR all (active + templates) */
    int exit_code = list_active_workflows(registry);

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

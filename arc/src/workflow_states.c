/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "arc_commands.h"
#include "arc_context.h"
#include "arc_error.h"
#include "argo_workflow_registry.h"
#include "argo_orchestrator_api.h"
#include "argo_init.h"
#include "argo_error.h"
#include "argo_output.h"

#define WORKFLOW_REGISTRY_PATH ".argo/workflows/registry/active_workflow_registry.json"

/* arc states command handler - show status of ALL workflows */
int arc_workflow_states(int argc, char** argv) {
    /* Get effective environment filter */
    const char* environment = arc_get_effective_environment(argc, argv);

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

    /* Get workflows (filtered by environment if specified) */
    workflow_instance_t* workflows;
    int count;
    result = workflow_registry_list_filtered(registry, environment, &workflows, &count);
    if (result != ARGO_SUCCESS || count == 0) {
        LOG_USER_INFO("No active workflows\n");
        workflow_registry_destroy(registry);
        argo_exit();
        return ARC_EXIT_SUCCESS;
    }

    /* Print header */
    printf("\n");
    printf("========================================\n");
    if (environment) {
        printf("Active Workflow States (%s environment: %d workflows)\n", environment, count);
    } else {
        printf("Active Workflow States (all environments: %d workflows)\n", count);
    }
    printf("========================================\n");
    printf("\n");

    /* Get current workflow from env */
    const char* current_workflow = getenv("ARGO_ACTIVE_WORKFLOW");

    /* Print each workflow */
    for (int i = 0; i < count; i++) {
        workflow_instance_t* wf = &workflows[i];

        /* Mark current workflow */
        const char* marker = "";
        if (current_workflow && strcmp(wf->id, current_workflow) == 0) {
            marker = " *";  /* Current workflow */
        }

        /* Check if process is alive */
        bool is_running = workflow_exec_is_process_alive(wf->pid);
        const char* running_str = is_running ? "RUNNING" : "STOPPED";

        /* Format time */
        time_t now = time(NULL);
        time_t age = now - wf->last_active;
        char age_str[64];
        if (age < 60) {
            snprintf(age_str, sizeof(age_str), "%lds ago", (long)age);
        } else if (age < 3600) {
            snprintf(age_str, sizeof(age_str), "%ldm ago", (long)(age / 60));
        } else if (age < 86400) {
            snprintf(age_str, sizeof(age_str), "%ldh ago", (long)(age / 3600));
        } else {
            snprintf(age_str, sizeof(age_str), "%ldd ago", (long)(age / 86400));
        }

        /* Print workflow info */
        printf("%-30s%s\n", wf->id, marker);
        printf("  Template:     %s\n", wf->template_name);
        printf("  Instance:     %s\n", wf->instance_name);
        printf("  Branch:       %s\n", wf->active_branch);
        printf("  Environment:  %s\n", wf->environment);
        printf("  Status:       %s (%s)\n", workflow_status_string(wf->status), running_str);
        printf("  PID:          %d\n", (int)wf->pid);
        printf("  Last Active:  %s\n", age_str);

        /* Show log file */
        const char* home = getenv("HOME");
        if (!home) home = ".";
        printf("  Log:          %s/.argo/logs/%s.log\n", home, wf->id);

        printf("\n");
    }

    printf("========================================\n");
    if (current_workflow) {
        printf("Current workflow: %s\n", current_workflow);
    } else {
        printf("No current workflow set (use 'arc switch')\n");
    }
    printf("\n");

    /* Cleanup */
    workflow_registry_destroy(registry);
    argo_exit();

    return ARC_EXIT_SUCCESS;
}

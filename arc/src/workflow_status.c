/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "arc_commands.h"
#include "arc_context.h"
#include "arc_error.h"
#include "argo_workflow_registry.h"
#include "argo_workflow_executor.h"
#include "argo_init.h"
#include "argo_error.h"

#define WORKFLOW_REGISTRY_PATH ".argo/workflows/registry/active_workflow_registry.json"

/* Format time */
static void format_time(time_t t, char* buffer, size_t size) {
    struct tm* tm_info = localtime(&t);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

/* Format duration */
static void format_duration(time_t seconds, char* buffer, size_t size) {
    if (seconds < 60) {
        snprintf(buffer, size, "%lds", (long)seconds);
    } else if (seconds < 3600) {
        snprintf(buffer, size, "%ldm %lds", (long)(seconds / 60), (long)(seconds % 60));
    } else {
        snprintf(buffer, size, "%ldh %ldm", (long)(seconds / 3600), (long)((seconds % 3600) / 60));
    }
}

/* Show status for a single workflow */
static void show_workflow_status(workflow_instance_t* wf) {
    char created_time[32];
    char duration[32];
    time_t now = time(NULL);

    format_time(wf->created_at, created_time, sizeof(created_time));
    format_duration(now - wf->created_at, duration, sizeof(duration));

    printf("\nWORKFLOW: %s\n", wf->id);
    printf("  Template:       %s\n", wf->template_name);
    printf("  Instance:       %s\n", wf->instance_name);
    printf("  Branch:         %s\n", wf->active_branch);
    printf("  Status:         %s\n", workflow_status_string(wf->status));
    printf("  Created:        %s\n", created_time);
    printf("  Total Time:     %s\n", duration);
    printf("  Logs:           ~/.argo/logs/%s.log\n", wf->id);

    /* Check for checkpoint file */
    char checkpoint_path[512];
    const char* home = getenv("HOME");
    if (home) {
        snprintf(checkpoint_path, sizeof(checkpoint_path),
                "%s/.argo/workflows/checkpoints/%s.json", home, wf->id);

        FILE* cp_file = fopen(checkpoint_path, "r");
        if (cp_file) {
            char buffer[512];
            size_t bytes = fread(buffer, 1, sizeof(buffer) - 1, cp_file);
            fclose(cp_file);
            buffer[bytes] = '\0';

            /* Extract current_step from checkpoint */
            const char* step_str = strstr(buffer, JSON_CURRENT_STEP_FIELD);
            const char* total_str = strstr(buffer, JSON_TOTAL_STEPS_FIELD);
            const char* paused_str = strstr(buffer, JSON_IS_PAUSED_FIELD);

            if (step_str && total_str) {
                int current_step, total_steps;
                sscanf(step_str + JSON_CURRENT_STEP_OFFSET, "%d", &current_step);
                sscanf(total_str + JSON_TOTAL_STEPS_OFFSET, "%d", &total_steps);
                printf("  Progress:       Step %d/%d\n", current_step + 1, total_steps);
                if (paused_str) {
                    printf("  State:          PAUSED\n");
                }
            }
        }
    }
}

/* arc workflow status command handler */
int arc_workflow_status(int argc, char** argv) {
    const char* workflow_name = NULL;

    /* Get workflow name from arg or context */
    if (argc >= 1) {
        workflow_name = argv[0];
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
    if (result != ARGO_SUCCESS && result != E_SYSTEM_FILE) {
        fprintf(stderr, "Error: Failed to load workflow registry\n");
        workflow_registry_destroy(registry);
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    /* Show specific workflow or all workflows */
    if (workflow_name) {
        /* Show specific workflow */
        workflow_instance_t* wf = workflow_registry_get_workflow(registry, workflow_name);
        if (!wf) {
            fprintf(stderr, "Error: Workflow not found: %s\n", workflow_name);
            fprintf(stderr, "  Try: arc workflow list\n");
            fprintf(stderr, "  Or: arc workflow list active\n");
            workflow_registry_destroy(registry);
            argo_exit();
            return ARC_EXIT_ERROR;
        }

        show_workflow_status(wf);
    } else {
        /* Show all workflows */
        workflow_instance_t* workflows;
        int count;

        result = workflow_registry_list(registry, &workflows, &count);
        if (result != ARGO_SUCCESS) {
            fprintf(stderr, "Error: Failed to list workflows\n");
            workflow_registry_destroy(registry);
            argo_exit();
            return ARC_EXIT_ERROR;
        }

        if (count == 0) {
            printf("\nNo active workflows.\n");
            printf("Use 'arc workflow start' to create a workflow.\n\n");
        } else {
            for (int i = 0; i < count; i++) {
                show_workflow_status(&workflows[i]);
            }
            printf("\n");
        }
    }

    /* Cleanup */
    workflow_registry_destroy(registry);
    argo_exit();

    return ARC_EXIT_SUCCESS;
}

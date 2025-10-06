/* © 2025 Casey Koons All rights reserved */
/* Workflow Executor - executes workflow templates with CI integration */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

/* Project includes */
#include "argo_workflow.h"
#include "argo_registry.h"
#include "argo_lifecycle.h"
#include "argo_error.h"
#include "argo_init.h"

/* Global workflow for signal handling */
static workflow_controller_t* g_workflow = NULL;
static int g_should_stop = 0;

/* Signal handler for graceful shutdown */
static void signal_handler(int signum) {
    if (signum == SIGTERM || signum == SIGINT) {
        fprintf(stderr, "Received shutdown signal %d\n", signum);
        g_should_stop = 1;
    } else if (signum == SIGSTOP) {
        /* Pause - handle in main loop */
        fprintf(stderr, "Received pause signal\n");
    }
}

int main(int argc, char** argv) {
    int exit_code = 1;
    ci_registry_t* registry = NULL;
    lifecycle_manager_t* lifecycle = NULL;

    /* Parse arguments */
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <workflow_id> <template_path> <branch>\n", argv[0]);
        return 1;
    }

    const char* workflow_id = argv[1];
    const char* template_path = argv[2];
    const char* branch = argv[3];

    printf("=========================================\n");
    printf("Argo Workflow Executor\n");
    printf("=========================================\n");
    printf("Workflow ID: %s\n", workflow_id);
    printf("Template:    %s\n", template_path);
    printf("Branch:      %s\n", branch);
    printf("PID:         %d\n", getpid());
    printf("=========================================\n\n");

    /* Setup signal handlers */
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    /* Initialize argo */
    int result = argo_init();
    if (result != ARGO_SUCCESS) {
        fprintf(stderr, "Failed to initialize argo: %d\n", result);
        goto cleanup;
    }

    /* Create registry */
    registry = registry_create();
    if (!registry) {
        fprintf(stderr, "Failed to create registry\n");
        goto cleanup;
    }

    /* Create lifecycle manager */
    lifecycle = lifecycle_manager_create(registry);
    if (!lifecycle) {
        fprintf(stderr, "Failed to create lifecycle manager\n");
        goto cleanup;
    }

    /* Create workflow controller */
    g_workflow = workflow_create(registry, lifecycle, workflow_id);
    if (!g_workflow) {
        fprintf(stderr, "Failed to create workflow controller\n");
        goto cleanup;
    }

    /* Load workflow JSON */
    result = workflow_load_json(g_workflow, template_path);
    if (result != ARGO_SUCCESS) {
        fprintf(stderr, "Failed to load workflow from: %s (error: %d)\n", template_path, result);
        goto cleanup;
    }

    printf("Loaded workflow from: %s\n", template_path);
    printf("Branch: %s\n\n", branch);

    /* Execute workflow steps */
    printf("Starting workflow execution...\n\n");

    /* Execute until EXIT step or error */
    while (!g_should_stop && strcmp(g_workflow->current_step_id, EXECUTOR_STEP_EXIT) != 0) {
        /* Safety: prevent infinite loops */
        if (g_workflow->step_count >= EXECUTOR_MAX_STEPS) {
            fprintf(stderr, "Maximum step count exceeded (%d)\n", EXECUTOR_MAX_STEPS);
            result = E_INPUT_INVALID;
            break;
        }

        /* Execute current step */
        printf("Executing step %d: %s\n", g_workflow->step_count + 1, g_workflow->current_step_id);
        result = workflow_execute_current_step(g_workflow);

        if (result == ARGO_SUCCESS) {
            printf("✓ Step %s completed\n\n", g_workflow->previous_step_id);
        } else {
            fprintf(stderr, "✗ Step %s failed with error: %d\n\n", g_workflow->current_step_id, result);
            break;
        }
    }

    if (g_should_stop) {
        printf("Workflow stopped by signal\n");
        exit_code = 2;
    } else if (strcmp(g_workflow->current_step_id, EXECUTOR_STEP_EXIT) == 0 && result == ARGO_SUCCESS) {
        printf("=========================================\n");
        printf("Workflow completed successfully\n");
        printf("Total steps executed: %d\n", g_workflow->step_count);
        printf("=========================================\n");
        exit_code = 0;
    } else {
        printf("=========================================\n");
        printf("Workflow failed\n");
        printf("=========================================\n");
        exit_code = 1;
    }

cleanup:
    if (g_workflow) {
        workflow_destroy(g_workflow);
    }
    if (lifecycle) {
        lifecycle_manager_destroy(lifecycle);
    }
    if (registry) {
        registry_destroy(registry);
    }

    argo_exit();
    return exit_code;
}

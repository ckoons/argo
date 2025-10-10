/* © 2025 Casey Koons All rights reserved */
/* Workflow Executor - executes workflow templates with CI integration */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <curl/curl.h>
#include <sys/stat.h>

/* Project includes */
#include "argo_workflow.h"
#include "argo_registry.h"
#include "argo_lifecycle.h"
#include "argo_error.h"
#include "argo_init.h"
#include "argo_limits.h"
#include "argo_urls.h"
#include "argo_io_channel_http.h"
#include "argo_log.h"

/* Global workflow for signal handling */
static workflow_controller_t* g_workflow = NULL;
static int g_should_stop = 0;
static char g_workflow_id[ARGO_BUFFER_MEDIUM] = {0};

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

/* Get daemon base URL */
static void get_daemon_base_url(char* buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size, "http://%s:%d",
             DEFAULT_DAEMON_HOST, DEFAULT_DAEMON_PORT);
}

/* Report progress to daemon */
static void report_progress(const char* workflow_id, int current_step, int total_steps, const char* step_name) {
    CURL* curl = curl_easy_init();
    if (!curl) return;

    char base_url[ARGO_BUFFER_MEDIUM];
    char url[ARGO_PATH_MAX];
    char json_body[ARGO_PATH_MAX];

    get_daemon_base_url(base_url, sizeof(base_url));
    snprintf(url, sizeof(url), "%s/api/workflow/progress/%s",
             base_url, workflow_id);
    snprintf(json_body, sizeof(json_body),
            "{\"current_step\":%d,\"total_steps\":%d,\"step_name\":\"%s\"}",
            current_step, total_steps, step_name ? step_name : "");

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);  /* Don't need response body */

    curl_easy_perform(curl);  /* Ignore errors - progress reporting is best-effort */

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
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

    /* Save workflow_id for signal handlers and progress reporting */
    strncpy(g_workflow_id, workflow_id, sizeof(g_workflow_id) - 1);

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

    /* TODO: LOG_INFO causes executor to hang - logging system needs investigation
     * For now, using printf for step messages (output redirected to log file by orchestrator) */

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

    /* Create HTTP I/O channel for interactive workflows */
    char daemon_url[ARGO_BUFFER_MEDIUM];
    get_daemon_base_url(daemon_url, sizeof(daemon_url));
    g_workflow->context->io_channel = io_channel_create_http(daemon_url, g_workflow_id);
    if (!g_workflow->context->io_channel) {
        fprintf(stderr, "Warning: Failed to create HTTP I/O channel - interactive workflows may not work\n");
    } else {
        printf("Created HTTP I/O channel: %s (workflow: %s)\n\n", daemon_url, g_workflow_id);
    }

    /* Execute workflow steps */
    printf("Starting workflow execution...\n\n");

    /* Execute until EXIT step or error */
    while (!g_should_stop && strcmp(g_workflow->current_step_id, EXECUTOR_STEP_EXIT) != 0) {
        /* Safety: prevent infinite loops */
        if (g_workflow->step_count >= EXECUTOR_MAX_STEPS) {
            LOG_ERROR("Maximum step count exceeded (%d)", EXECUTOR_MAX_STEPS);
            result = E_INPUT_INVALID;
            break;
        }

        /* Safety: check log file size to prevent runaway loops */
        char log_path[ARGO_PATH_MAX];
        snprintf(log_path, sizeof(log_path), ".argo/logs/%s.log", g_workflow_id);
        struct stat st;
        if (stat(log_path, &st) == 0) {
            if (st.st_size > MAX_WORKFLOW_LOG_SIZE) {
                LOG_ERROR("Log file exceeded maximum size (%lld > %d bytes)",
                         (long long)st.st_size, MAX_WORKFLOW_LOG_SIZE);
                LOG_ERROR("Aborting to prevent runaway loop");
                result = E_RESOURCE_LIMIT;
                break;
            }
        }

        /* Report progress to daemon */
        report_progress(g_workflow_id, g_workflow->step_count, EXECUTOR_MAX_STEPS,
                       g_workflow->current_step_id);

        /* Execute current step */
        printf("Executing step %d: %s\n", g_workflow->step_count + 1, g_workflow->current_step_id);
        result = workflow_execute_current_step(g_workflow);

        if (result == ARGO_SUCCESS) {
            if (g_workflow->previous_step_id[0]) {
                printf("Step %s completed\n", g_workflow->previous_step_id);
            }
        } else {
            fprintf(stderr, "Step %s failed with error: [%s] %s\n",
                     g_workflow->current_step_id,
                     argo_error_string(result),
                     argo_error_message(result));
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
        if (g_workflow->context && g_workflow->context->io_channel) {
            io_channel_free(g_workflow->context->io_channel);
            g_workflow->context->io_channel = NULL;
        }
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

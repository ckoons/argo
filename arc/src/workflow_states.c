/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include "arc_commands.h"
#include "arc_context.h"
#include "arc_error.h"
#include "arc_constants.h"
#include "arc_http_client.h"
#include "argo_error.h"
#include "argo_output.h"
#include "argo_limits.h"
#include "argo_http_server.h"

/* Check if process is alive */
static bool is_process_alive(pid_t pid) {
    if (pid <= 0) return false;
    /* Send signal 0 to check if process exists */
    return (kill(pid, 0) == 0);
}

/* arc states command handler - show status of ALL workflows */
int arc_workflow_states(int argc, char** argv) {
    /* Get effective environment filter */
    const char* environment = arc_get_effective_environment(argc, argv);

    /* Send GET request to daemon */
    arc_http_response_t* response = NULL;
    int result = arc_http_get("/api/workflow/list", &response);
    if (result != ARGO_SUCCESS) {
        LOG_USER_ERROR("Failed to connect to daemon: %s\n", arc_get_daemon_url());
        LOG_USER_INFO("  Make sure daemon is running: argo-daemon\n");
        return ARC_EXIT_ERROR;
    }

    /* Check HTTP status */
    if (response->status_code != HTTP_STATUS_OK) {
        LOG_USER_ERROR("Failed to list workflows (HTTP %d)\n", response->status_code);
        if (response->body) {
            LOG_USER_INFO("  %s\n", response->body);
        }
        arc_http_response_free(response);
        return ARC_EXIT_ERROR;
    }

    /* Parse JSON response */
    if (!response->body) {
        LOG_USER_INFO("No active workflows\n");
        arc_http_response_free(response);
        return ARC_EXIT_SUCCESS;
    }

    /* Check for empty workflows array */
    const char* workflows_array = strstr(response->body, "\"workflows\":[");
    if (!workflows_array || strstr(workflows_array, "\"workflows\":[]")) {
        LOG_USER_INFO("No active workflows\n");
        arc_http_response_free(response);
        return ARC_EXIT_SUCCESS;
    }

    /* Count workflows */
    int count = 0;
    const char* ptr = workflows_array;
    while ((ptr = strstr(ptr, "{\"workflow_id\":\"")) != NULL) {
        count++;
        ptr += ARC_JSON_OFFSET_ID; /* strlen("{\"workflow_id\":\"") */
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

    /* Parse and display each workflow */
    ptr = workflows_array;
    while ((ptr = strstr(ptr, "{\"workflow_id\":\"")) != NULL) {
        char workflow_id[ARGO_BUFFER_MEDIUM] = {0};
        char status[ARGO_BUFFER_TINY] = {0};
        int pid = 0;

        /* Extract fields */
        if (sscanf(ptr, "{\"workflow_id\":\"%255[^\"]\"", workflow_id) == 1) {
            const char* status_str = strstr(ptr, "\"status\":\"");
            const char* pid_str = strstr(ptr, "\"pid\":");

            if (status_str) {
                sscanf(status_str, "\"status\":\"%31[^\"]\"", status);
            }
            if (pid_str) {
                sscanf(pid_str, "\"pid\":%d", &pid);
            }

            /* Parse template and instance from workflow_id */
            char template_name[ARGO_BUFFER_NAME] = {0};
            char instance_name[ARGO_BUFFER_NAME] = {0};
            const char* underscore = strrchr(workflow_id, '_');
            if (underscore) {
                size_t template_len = underscore - workflow_id;
                if (template_len < sizeof(template_name)) {
                    strncpy(template_name, workflow_id, template_len);
                    template_name[template_len] = '\0';
                }
                strncpy(instance_name, underscore + 1, sizeof(instance_name) - 1);
            } else {
                strncpy(template_name, workflow_id, sizeof(template_name) - 1);
            }

            /* Mark current workflow */
            const char* marker = "";
            if (current_workflow && strcmp(workflow_id, current_workflow) == 0) {
                marker = " *";  /* Current workflow */
            }

            /* Check if process is alive */
            bool is_running = is_process_alive(pid);
            const char* running_str = is_running ? "RUNNING" : "STOPPED";

            /* Print workflow info */
            printf("%-30s%s\n", workflow_id, marker);
            printf("  Template:     %s\n", template_name);
            printf("  Instance:     %s\n", instance_name);
            printf("  Branch:       main\n");  /* TODO: Include in API response */
            printf("  Environment:  dev\n");    /* TODO: Include in API response */
            printf("  Status:       %s (%s)\n", status, running_str);
            printf("  PID:          %d\n", pid);

            /* Show log file */
            const char* home = getenv("HOME");
            if (!home) home = ".";
            printf("  Log:          %s/.argo/logs/%s.log\n", home, workflow_id);

            printf("\n");
        }

        /* Move to next workflow */
        ptr = strstr(ptr + 1, "{\"workflow_id\":\"");
        if (!ptr) break;
    }

    printf("========================================\n");
    if (current_workflow) {
        printf("Current workflow: %s\n", current_workflow);
    } else {
        printf("No current workflow set (use 'arc switch')\n");
    }
    printf("\n");

    /* Cleanup */
    arc_http_response_free(response);

    return ARC_EXIT_SUCCESS;
}

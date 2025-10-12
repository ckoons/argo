/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arc_commands.h"
#include "arc_context.h"
#include "arc_error.h"
#include "arc_http_client.h"
#include "argo_error.h"
#include "argo_output.h"
#include "argo_limits.h"
#include "argo_http_server.h"

/* arc workflow status command handler */
int arc_workflow_status(int argc, char** argv) {
    const char* workflow_name = NULL;

    /* Get workflow name from arg or environment */
    if (argc >= 1) {
        workflow_name = argv[0];
    } else {
        /* Try to get from ARGO_ACTIVE_WORKFLOW */
        workflow_name = getenv("ARGO_ACTIVE_WORKFLOW");
        if (!workflow_name) {
            LOG_USER_ERROR("No workflow specified and no active workflow set\n");
            LOG_USER_INFO("Usage: arc status <workflow_id>\n");
            LOG_USER_INFO("   or: arc switch <workflow_id> (to set active workflow)\n");
            LOG_USER_INFO("   or: arc states (to see all workflows)\n");
            return ARC_EXIT_ERROR;
        }
    }

    /* Build request URL with path parameter */
    char endpoint[ARGO_PATH_MAX];
    snprintf(endpoint, sizeof(endpoint), "/api/workflow/status/%s", workflow_name);

    /* Send GET request to daemon */
    arc_http_response_t* response = NULL;
    int result = arc_http_get(endpoint, &response);
    if (result != ARGO_SUCCESS) {
        LOG_USER_ERROR("Failed to connect to daemon: %s\n", arc_get_daemon_url());
        LOG_USER_INFO("  Make sure daemon is running: argo-daemon\n");
        return ARC_EXIT_ERROR;
    }

    /* Check HTTP status */
    if (response->status_code == HTTP_STATUS_NOT_FOUND) {
        LOG_USER_ERROR("Workflow not found: %s\n", workflow_name);
        LOG_USER_INFO("  Try: arc workflow list\n");
        arc_http_response_free(response);
        return ARC_EXIT_ERROR;
    } else if (response->status_code != HTTP_STATUS_OK) {
        LOG_USER_ERROR("Failed to get workflow status (HTTP %d)\n", response->status_code);
        if (response->body) {
            LOG_USER_INFO("  %s\n", response->body);
        }
        arc_http_response_free(response);
        return ARC_EXIT_ERROR;
    }

    /* Parse JSON response and display */
    if (response->body) {
        /* Extract key fields from JSON */
        char script[ARGO_PATH_MAX] = {0};
        char state[ARGO_BUFFER_TINY] = {0};
        int pid = 0;
        int exit_code = 0;

        const char* script_str = strstr(response->body, "\"script\"");
        const char* state_str = strstr(response->body, "\"state\"");
        const char* pid_str = strstr(response->body, "\"pid\"");
        const char* exit_str = strstr(response->body, "\"exit_code\"");

        if (script_str) {
            sscanf(script_str, "\"script\":\"%511[^\"]\"", script);
        }
        if (state_str) {
            sscanf(state_str, "\"state\":\"%31[^\"]\"", state);
        }
        if (pid_str) {
            sscanf(pid_str, "\"pid\":%d", &pid);
        }
        if (exit_str) {
            sscanf(exit_str, "\"exit_code\":%d", &exit_code);
        }

        LOG_USER_STATUS("\nWORKFLOW: %s\n", workflow_name);
        LOG_USER_STATUS("  Script:         %s\n", script);
        LOG_USER_STATUS("  State:          %s\n", state);
        LOG_USER_STATUS("  PID:            %d\n", pid);
        LOG_USER_STATUS("  Exit code:      %d\n", exit_code);
        LOG_USER_STATUS("  Logs:           ~/.argo/logs/%s.log\n\n", workflow_name);
    }

    /* Cleanup */
    arc_http_response_free(response);

    return ARC_EXIT_SUCCESS;
}

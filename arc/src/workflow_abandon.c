/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arc_commands.h"
#include "arc_context.h"
#include "arc_http_client.h"
#include "argo_error.h"
#include "argo_output.h"
#include "argo_limits.h"
#include "argo_http_server.h"

/* Get confirmation from user */
static int get_confirmation(const char* workflow_name) {
    char response[10];
    LOG_USER_INFO("Abandon workflow '%s'? (y/N): ", workflow_name);
    if (fgets(response, sizeof(response), stdin) == NULL) {
        return 0;
    }
    return (response[0] == 'y' || response[0] == 'Y');
}

/* arc workflow abandon command handler */
int arc_workflow_abandon(int argc, char** argv) {
    const char* workflow_name = NULL;

    /* Get workflow ID from arg or context */
    if (argc >= 1) {
        workflow_name = argv[0];
    } else {
        workflow_name = arc_context_get();
        if (!workflow_name) {
            LOG_USER_ERROR("No active workflow context\n");
            LOG_USER_INFO("Usage: arc workflow abandon <workflow_id>\n");
            LOG_USER_INFO("   or: arc switch <workflow_id> && arc workflow abandon\n");
            return ARC_EXIT_ERROR;
        }
    }

    /* Get confirmation */
    if (!get_confirmation(workflow_name)) {
        LOG_USER_INFO("Abandon cancelled.\n");
        return ARC_EXIT_SUCCESS;
    }

    /* Build request URL with query parameter */
    char endpoint[ARGO_PATH_MAX];
    snprintf(endpoint, sizeof(endpoint), "/api/workflow/abandon?workflow_name=%s", workflow_name);

    /* Send DELETE request to daemon */
    arc_http_response_t* response = NULL;
    int result = arc_http_delete(endpoint, &response);
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
        LOG_USER_ERROR("Failed to abandon workflow (HTTP %d)\n", response->status_code);
        if (response->body) {
            LOG_USER_INFO("  %s\n", response->body);
        }
        arc_http_response_free(response);
        return ARC_EXIT_ERROR;
    }

    /* Clear context if this was the active workflow */
    const char* current_context = arc_context_get();
    if (current_context && strcmp(current_context, workflow_name) == 0) {
        arc_context_clear();
    }

    /* Print confirmation */
    LOG_USER_SUCCESS("Abandoned workflow: %s\n", workflow_name);
    LOG_USER_INFO("Logs preserved: ~/.argo/logs/%s.log\n", workflow_name);

    /* Cleanup */
    arc_http_response_free(response);

    return ARC_EXIT_SUCCESS;
}

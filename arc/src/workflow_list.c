/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "arc_commands.h"
#include "arc_context.h"
#include "arc_constants.h"
#include "arc_http_client.h"
#include "argo_error.h"
#include "argo_output.h"
#include "argo_limits.h"
#include "argo_http_server.h"

/* List active workflows via daemon HTTP API */
static int list_active_workflows(const char* environment) {
    (void)environment;  /* Reserved for future filtering */

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
        LOG_USER_STATUS("\nNo active workflows.\n");
        LOG_USER_STATUS("Use 'arc workflow start' to create a workflow.\n\n");
        arc_http_response_free(response);
        return ARC_EXIT_SUCCESS;
    }

    /* Count workflows in JSON */
    const char* workflows_array = strstr(response->body, "\"workflows\":[");
    if (!workflows_array) {
        LOG_USER_STATUS("\nNo active workflows.\n");
        LOG_USER_STATUS("Use 'arc workflow start' to create a workflow.\n\n");
        arc_http_response_free(response);
        return ARC_EXIT_SUCCESS;
    }

    /* Check if empty array */
    if (strstr(workflows_array, "\"workflows\":[]")) {
        LOG_USER_STATUS("\nNo active workflows.\n");
        LOG_USER_STATUS("Use 'arc workflow start' to create a workflow.\n\n");
        arc_http_response_free(response);
        return ARC_EXIT_SUCCESS;
    }

    /* Parse and display workflows */
    LOG_USER_STATUS("\nACTIVE WORKFLOWS:\n");
    LOG_USER_STATUS("%-20s %-50s %-12s %-8s\n",
           "ID", "SCRIPT", "STATE", "PID");
    LOG_USER_STATUS("---------------------------------------------------------------------------------\n");

    /* Simple JSON parsing - find each workflow object */
    const char* ptr = workflows_array;
    while ((ptr = strstr(ptr, "{\"workflow_id\":\"")) != NULL) {
        char workflow_id[ARGO_BUFFER_MEDIUM] = {0};
        char script[ARGO_PATH_MAX] = {0};
        char state[ARGO_BUFFER_TINY] = {0};
        int pid = 0;

        /* Extract fields */
        const char* id_str = ptr;
        const char* script_str = strstr(ptr, "\"script\":\"");
        const char* state_str = strstr(ptr, "\"state\":\"");
        const char* pid_str = strstr(ptr, "\"pid\":");

        if (id_str && sscanf(id_str, "{\"workflow_id\":\"%255[^\"]\"", workflow_id) == 1) {
            if (script_str) {
                sscanf(script_str, "\"script\":\"%511[^\"]\"", script);
            }
            if (state_str) {
                sscanf(state_str, "\"state\":\"%31[^\"]\"", state);
            }
            if (pid_str) {
                sscanf(pid_str, "\"pid\":%d", &pid);
            }

            /* Display workflow */
            LOG_USER_STATUS("%-20s %-50s %-12s %-8d\n",
                   workflow_id,
                   script,
                   state,
                   pid);
        }

        /* Move to next workflow */
        ptr = strstr(ptr + 1, "{\"workflow_id\":\"");
        if (!ptr) break;
    }

    LOG_USER_STATUS("\n");
    arc_http_response_free(response);
    return ARC_EXIT_SUCCESS;
}

/* arc workflow list command handler */
int arc_workflow_list(int argc, char** argv) {
    /* Get effective environment filter */
    const char* environment = arc_get_effective_environment(argc, argv);

    /* List active workflows */
    return list_active_workflows(environment);
}

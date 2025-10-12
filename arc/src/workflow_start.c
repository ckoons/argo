/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arc_commands.h"
#include "arc_context.h"
#include "arc_error.h"
#include "arc_constants.h"
#include "arc_http_client.h"
#include "argo_error.h"
#include "argo_output.h"

/* arc workflow start command handler - bash script execution */
int arc_workflow_start(int argc, char** argv) {
    if (argc < 1) {
        LOG_USER_ERROR("script path required\n");
        LOG_USER_INFO("Usage: arc workflow start <script.sh> [args...]\n");
        LOG_USER_INFO("Example: arc workflow start workflows/examples/hello.sh\n");
        return ARC_EXIT_ERROR;
    }

    const char* script_path = argv[0];

    /* Build JSON request */
    char json_body[1024];
    snprintf(json_body, sizeof(json_body), "{\"script\":\"%s\"}", script_path);

    /* Send POST request to daemon */
    arc_http_response_t* response = NULL;
    int result = arc_http_post("/api/workflow/start", json_body, &response);
    if (result != ARGO_SUCCESS) {
        LOG_USER_ERROR("Failed to connect to daemon: %s\n", arc_get_daemon_url());
        LOG_USER_INFO("  Make sure daemon is running: argo-daemon\n");
        return ARC_EXIT_ERROR;
    }

    /* Check HTTP status */
    if (response->status_code == 404) {
        LOG_USER_ERROR("Script not found: %s\n", script_path);
        arc_http_response_free(response);
        return ARC_EXIT_ERROR;
    } else if (response->status_code == 409) {
        LOG_USER_ERROR("Workflow already exists\n");
        LOG_USER_INFO("  Try: arc workflow list\n");
        arc_http_response_free(response);
        return ARC_EXIT_ERROR;
    } else if (response->status_code != 200) {
        LOG_USER_ERROR("Failed to start workflow (HTTP %d)\n", response->status_code);
        if (response->body) {
            LOG_USER_INFO("  %s\n", response->body);
        }
        arc_http_response_free(response);
        return ARC_EXIT_ERROR;
    }

    /* Parse workflow_id from response */
    char workflow_id[ARC_WORKFLOW_ID_MAX] = {0};
    if (response->body) {
        const char* id_str = strstr(response->body, "\"workflow_id\":\"");
        if (id_str) {
            sscanf(id_str, "\"workflow_id\":\"%127[^\"]\"", workflow_id);
        }
    }

    /* Print confirmation */
    LOG_USER_SUCCESS("Started workflow: %s\n", workflow_id);
    LOG_USER_INFO("Script: %s\n", script_path);
    LOG_USER_INFO("Logs: ~/.argo/logs/%s.log\n", workflow_id);

    /* Cleanup HTTP response */
    arc_http_response_free(response);

    /* Auto-attach to workflow */
    char* attach_argv[] = {workflow_id};
    return arc_workflow_attach(1, attach_argv);
}

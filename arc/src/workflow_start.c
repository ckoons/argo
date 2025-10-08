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

/* arc workflow start command handler */
int arc_workflow_start(int argc, char** argv) {
    if (argc < 2) {
        LOG_USER_ERROR("template and instance name required\n");
        LOG_USER_INFO("Usage: arc workflow start [template] [instance] [branch] [--env <env>]\n");
        return ARC_EXIT_ERROR;
    }

    const char* template_name = argv[0];
    const char* instance_name = argv[1];
    const char* branch = (argc >= 3) ? argv[2] : "main";

    /* Get environment for workflow creation (defaults to 'dev') */
    const char* environment = arc_get_environment_for_creation(argc, argv);

    /* Build JSON request */
    char json_body[1024];
    snprintf(json_body, sizeof(json_body),
            "{\"template\":\"%s\",\"instance\":\"%s\",\"branch\":\"%s\",\"environment\":\"%s\"}",
            template_name, instance_name, branch, environment);

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
        LOG_USER_ERROR("Template not found: %s\n", template_name);
        LOG_USER_INFO("  Try: arc workflow list template\n");
        arc_http_response_free(response);
        return ARC_EXIT_ERROR;
    } else if (response->status_code == 409) {
        LOG_USER_ERROR("Workflow already exists: %s_%s\n", template_name, instance_name);
        LOG_USER_INFO("  Try: arc workflow list\n");
        LOG_USER_INFO("  Or use: arc workflow abandon %s_%s\n", template_name, instance_name);
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
    char workflow_id[ARC_WORKFLOW_ID_MAX];
    snprintf(workflow_id, sizeof(workflow_id), "%s_%s", template_name, instance_name);

    /* Print confirmation */
    LOG_USER_SUCCESS("Started workflow: %s (environment: %s)\n", workflow_id, environment);
    LOG_USER_INFO("Logs: ~/.argo/logs/%s.log\n", workflow_id);

    /* Cleanup HTTP response */
    arc_http_response_free(response);

    /* Auto-attach to workflow */
    char* attach_argv[] = {(char*)workflow_id};
    return arc_workflow_attach(1, attach_argv);
}

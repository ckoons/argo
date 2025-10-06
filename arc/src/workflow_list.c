/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "arc_commands.h"
#include "arc_context.h"
#include "arc_constants.h"
#include "arc_http_client.h"
#include "argo_workflow_templates.h"
#include "argo_init.h"
#include "argo_error.h"
#include "argo_output.h"
#include "argo_limits.h"
#include "argo_http_server.h"

/* List active workflows via daemon HTTP API */
static int list_active_workflows(const char* environment) {
    const char* context = arc_context_get();

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
    LOG_USER_STATUS("%-8s %-30s %-16s %-12s %-8s %-8s\n",
           "CONTEXT", "NAME", "TEMPLATE", "INSTANCE", "STATUS", "PID");
    LOG_USER_STATUS("------------------------------------------------------------------------\n");

    /* Simple JSON parsing - find each workflow object */
    const char* ptr = workflows_array;
    while ((ptr = strstr(ptr, "{\"workflow_id\":\"")) != NULL) {
        char workflow_id[ARGO_BUFFER_MEDIUM] = {0};
        char status[ARGO_BUFFER_TINY] = {0};
        int pid = 0;

        /* Extract fields */
        const char* id_str = ptr;
        const char* status_str = strstr(ptr, "\"status\":\"");
        const char* pid_str = strstr(ptr, "\"pid\":");

        if (id_str && sscanf(id_str, "{\"workflow_id\":\"%255[^\"]\"", workflow_id) == 1) {
            if (status_str) {
                sscanf(status_str, "\"status\":\"%31[^\"]\"", status);
            }
            if (pid_str) {
                sscanf(pid_str, "\"pid\":%d", &pid);
            }

            /* Filter by environment if specified */
            int show = 1;
            if (environment) {
                /* Check if workflow matches environment */
                /* For now, show all - environment filtering would need to be in daemon */
                show = 1;
            }

            if (show) {
                /* Parse template and instance from workflow_id (format: template_instance) */
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

                const char* mark = (context && strcmp(context, workflow_id) == 0) ? "*" : " ";

                LOG_USER_STATUS("%-8s %-30s %-16s %-12s %-8s %-8d\n",
                       mark,
                       workflow_id,
                       template_name,
                       instance_name,
                       status,
                       pid);
            }
        }

        /* Move to next workflow */
        ptr = strstr(ptr + 1, "{\"workflow_id\":\"");
        if (!ptr) break;
    }

    LOG_USER_STATUS("\n");
    arc_http_response_free(response);
    return ARC_EXIT_SUCCESS;
}

/* List templates */
static int list_templates(void) {
    /* Discover templates */
    workflow_template_collection_t* templates = workflow_templates_create();
    if (!templates) {
        LOG_USER_ERROR("Failed to create template collection\n");
        return ARC_EXIT_ERROR;
    }

    int result = workflow_templates_discover(templates);
    if (result != ARGO_SUCCESS) {
        LOG_USER_WARN("Failed to discover templates\n");
        workflow_templates_destroy(templates);
        return ARC_EXIT_ERROR;
    }

    if (templates->count == 0) {
        LOG_USER_STATUS("\nNo templates found.\n");
        LOG_USER_STATUS("  System templates: workflows/templates/\n");
        LOG_USER_STATUS("  User templates:   ~/.argo/workflows/templates/\n\n");
        workflow_templates_destroy(templates);
        return ARC_EXIT_SUCCESS;
    }

    LOG_USER_STATUS("\nTEMPLATES:\n");
    LOG_USER_STATUS("%-8s %-20s %-40s\n", "SCOPE", "NAME", "DESCRIPTION");
    LOG_USER_STATUS("------------------------------------------------------------------------\n");

    for (int i = 0; i < templates->count; i++) {
        workflow_template_t* tmpl = &templates->templates[i];
        LOG_USER_STATUS("%-8s %-20s %-40s\n",
               tmpl->is_system ? "system" : "user",
               tmpl->name,
               tmpl->description);
    }

    LOG_USER_STATUS("\n");
    workflow_templates_destroy(templates);
    return ARC_EXIT_SUCCESS;
}

/* arc workflow list command handler */
int arc_workflow_list(int argc, char** argv) {
    const char* filter = NULL;
    if (argc >= 1) {
        filter = argv[0];
    }

    /* Get effective environment filter */
    const char* environment = arc_get_effective_environment(argc, argv);

    /* Initialize argo for template discovery */
    int result = argo_init();
    if (result != ARGO_SUCCESS) {
        LOG_USER_ERROR("Failed to initialize argo\n");
        return ARC_EXIT_ERROR;
    }

    /* Templates only */
    if (filter && strcmp(filter, "template") == 0) {
        result = list_templates();
        argo_exit();
        return result;
    }

    /* Active workflows only OR all (active + templates) */
    int exit_code = list_active_workflows(environment);

    /* If no filter OR filter is not "active", also show templates */
    if (!filter || strcmp(filter, "active") != 0) {
        if (exit_code == ARC_EXIT_SUCCESS) {
            list_templates();
        }
    }

    /* Cleanup */
    argo_exit();

    return exit_code;
}

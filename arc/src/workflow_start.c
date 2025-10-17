/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "arc_commands.h"
#include "arc_context.h"
#include "arc_error.h"
#include "arc_constants.h"
#include "arc_http_client.h"
#include "argo_error.h"
#include "argo_output.h"

/* Resolve template name to workflow.sh path (directory-based only) */
static int resolve_template_path(const char* template_name, char* script_path, size_t path_size) {
    const char* home = getenv("HOME");
    if (!home) {
        LOG_USER_ERROR("HOME environment variable not set\n");
        return ARC_EXIT_ERROR;
    }

    struct stat st;
    char user_template[ARC_PATH_BUFFER];
    char system_template[ARC_PATH_BUFFER];

    /* Try user template: ~/.argo/workflows/templates/{name}/workflow.sh */
    snprintf(user_template, sizeof(user_template),
            "%s/.argo/workflows/templates/%s/workflow.sh", home, template_name);

    if (stat(user_template, &st) == 0 && S_ISREG(st.st_mode)) {
        strncpy(script_path, user_template, path_size - 1);
        script_path[path_size - 1] = '\0';
        return ARC_EXIT_SUCCESS;
    }

    /* Try system template: workflows/templates/{name}/workflow.sh */
    snprintf(system_template, sizeof(system_template),
            "workflows/templates/%s/workflow.sh", template_name);

    if (stat(system_template, &st) == 0 && S_ISREG(st.st_mode)) {
        strncpy(script_path, system_template, path_size - 1);
        script_path[path_size - 1] = '\0';
        return ARC_EXIT_SUCCESS;
    }

    /* Not found */
    LOG_USER_ERROR("Template not found: %s\n", template_name);
    LOG_USER_INFO("  Tried:\n");
    LOG_USER_INFO("    - %s\n", user_template);
    LOG_USER_INFO("    - %s\n", system_template);
    LOG_USER_INFO("\n");
    LOG_USER_INFO("  Use 'arc templates' to see available templates\n");
    return ARC_EXIT_ERROR;
}

/* arc workflow start command handler - bash script execution */
int arc_workflow_start(int argc, char** argv) {
    if (argc < 1) {
        LOG_USER_ERROR("template or script path required\n");
        LOG_USER_INFO("Usage: arc start <template> [instance] [args...]\n");
        LOG_USER_INFO("Examples:\n");
        LOG_USER_INFO("  arc start create_workflow\n");
        LOG_USER_INFO("  arc start create_workflow my_feature\n");
        LOG_USER_INFO("  arc start build test_branch arg1 arg2\n");
        return ARC_EXIT_ERROR;
    }

    const char* template_name = argv[0];
    const char* instance_suffix = NULL;
    int arg_start_index = 1;

    /* Check if argv[1] is an instance name or a regular arg/env var */
    if (argc >= 2) {
        /* If argv[1] doesn't contain '=' and doesn't start with '-', treat as instance */
        if (strchr(argv[1], '=') == NULL && argv[1][0] != '-') {
            instance_suffix = argv[1];
            arg_start_index = 2;
        }
    }

    /* Resolve template name to script path */
    char script_path[ARC_PATH_BUFFER];
    if (resolve_template_path(template_name, script_path, sizeof(script_path)) != ARC_EXIT_SUCCESS) {
        return ARC_EXIT_ERROR;
    }

    /* Build JSON request with script, template, instance, args, and env */
    char json_body[ARC_JSON_BUFFER];  /* Larger buffer for args + env */
    size_t offset = 0;

    offset += snprintf(json_body + offset, sizeof(json_body) - offset,
                      "{\"script\":\"%s\",\"template\":\"%s\"", script_path, template_name);

    /* Add instance suffix if provided */
    if (instance_suffix) {
        offset += snprintf(json_body + offset, sizeof(json_body) - offset,
                         ",\"instance\":\"%s\"", instance_suffix);
    }

    /* Parse arguments - separate regular args from KEY=VALUE env vars */
    int env_count = 0;
    char* env_keys[ARC_MAX_ENV_VARS];
    char* env_values[ARC_MAX_ENV_VARS];

    /* First pass: identify and extract env vars */
    for (int i = arg_start_index; i < argc && env_count < ARC_MAX_ENV_VARS; i++) {
        char* eq = strchr(argv[i], '=');
        if (eq && eq != argv[i]) {
            /* This is KEY=VALUE format */
            size_t key_len = eq - argv[i];
            env_keys[env_count] = malloc(key_len + 1);
            env_values[env_count] = strdup(eq + 1);
            if (env_keys[env_count] && env_values[env_count]) {
                memcpy(env_keys[env_count], argv[i], key_len);
                env_keys[env_count][key_len] = '\0';
                env_count++;
            }
        }
    }

    /* Add args array (non-env arguments) */
    int args_added = 0;
    if (argc > arg_start_index) {
        offset += snprintf(json_body + offset, sizeof(json_body) - offset, ",\"args\":[");
        for (int i = arg_start_index; i < argc; i++) {
            if (!strchr(argv[i], '=') || strchr(argv[i], '=') == argv[i]) {
                /* Not an env var */
                offset += snprintf(json_body + offset, sizeof(json_body) - offset,
                                 "%s\"%s\"", (args_added > 0 ? "," : ""), argv[i]);
                args_added++;
                if (offset >= sizeof(json_body) - ARC_JSON_MARGIN) {
                    LOG_USER_ERROR("Too many arguments (buffer overflow)\n");
                    for (int j = 0; j < env_count; j++) {
                        free(env_keys[j]);
                        free(env_values[j]);
                    }
                    return ARC_EXIT_ERROR;
                }
            }
        }
        offset += snprintf(json_body + offset, sizeof(json_body) - offset, "]");
    }

    /* Add env object if any env vars found */
    if (env_count > 0) {
        offset += snprintf(json_body + offset, sizeof(json_body) - offset, ",\"env\":{");
        for (int i = 0; i < env_count; i++) {
            offset += snprintf(json_body + offset, sizeof(json_body) - offset,
                             "%s\"%s\":\"%s\"", (i > 0 ? "," : ""),
                             env_keys[i], env_values[i]);
        }
        offset += snprintf(json_body + offset, sizeof(json_body) - offset, "}");

        /* Free allocated memory */
        for (int i = 0; i < env_count; i++) {
            free(env_keys[i]);
            free(env_values[i]);
        }
    }

    snprintf(json_body + offset, sizeof(json_body) - offset, "}");

    /* Send POST request to daemon */
    arc_http_response_t* response = NULL;
    int result = arc_http_post("/api/workflow/start", json_body, &response);
    if (result != ARGO_SUCCESS) {
        LOG_USER_ERROR("Failed to connect to daemon: %s\n", arc_get_daemon_url());
        LOG_USER_INFO("  Make sure daemon is running: argo-daemon\n");
        return ARC_EXIT_ERROR;
    }

    /* Check HTTP status */
    if (response->status_code == ARC_HTTP_STATUS_NOT_FOUND) {
        LOG_USER_ERROR("Script not found: %s\n", script_path);
        arc_http_response_free(response);
        return ARC_EXIT_ERROR;
    } else if (response->status_code == ARC_HTTP_STATUS_CONFLICT) {
        LOG_USER_ERROR("Workflow already exists\n");
        LOG_USER_INFO("  Try: arc workflow list\n");
        arc_http_response_free(response);
        return ARC_EXIT_ERROR;
    } else if (response->status_code != ARC_HTTP_STATUS_OK) {
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
            sscanf(id_str, "\"workflow_id\":\"%ARC_SSCANF_FIELD_MEDIUM[^\"]\"", workflow_id);
        }
    }

    /* Print confirmation */
    LOG_USER_SUCCESS("Started workflow: %s\n", workflow_id);
    LOG_USER_INFO("Script: %s\n", script_path);
    LOG_USER_INFO("Logs: ~/.argo/logs/%s.log\n\n", workflow_id);

    /* Cleanup HTTP response */
    arc_http_response_free(response);

    /* Auto-attach to workflow to show output from beginning */
    if (workflow_id[0] != '\0') {
        return arc_workflow_attach_auto(workflow_id);
    }

    return ARC_EXIT_SUCCESS;
}

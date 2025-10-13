/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
        LOG_USER_INFO("Example: arc workflow start workflows/examples/hello.sh arg1 arg2\n");
        return ARC_EXIT_ERROR;
    }

     const char* script_path = argv[0];

    /* Build JSON request with args and env if provided */
    char json_body[4096];  /* Larger buffer for args + env */
    size_t offset = 0;

    offset += snprintf(json_body + offset, sizeof(json_body) - offset,
                      "{\"script\":\"%s\"", script_path);

    /* Parse arguments - separate regular args from KEY=VALUE env vars */
    int env_count = 0;
    char* env_keys[32];
    char* env_values[32];

    /* First pass: identify and extract env vars */
    for (int i = 1; i < argc && env_count < 32; i++) {
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
    if (argc > 1) {
        offset += snprintf(json_body + offset, sizeof(json_body) - offset, ",\"args\":[");
        for (int i = 1; i < argc; i++) {
            if (!strchr(argv[i], '=') || strchr(argv[i], '=') == argv[i]) {
                /* Not an env var */
                offset += snprintf(json_body + offset, sizeof(json_body) - offset,
                                 "%s\"%s\"", (args_added > 0 ? "," : ""), argv[i]);
                args_added++;
                if (offset >= sizeof(json_body) - 10) {
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

    /* Auto-attach to workflow only if stdin is a TTY (interactive mode) */
    if (isatty(STDIN_FILENO)) {
        char* attach_argv[] = {workflow_id};
        return arc_workflow_attach(1, attach_argv);
    }

    return ARC_EXIT_SUCCESS;
}

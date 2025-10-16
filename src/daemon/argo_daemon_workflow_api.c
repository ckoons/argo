/* © 2025 Casey Koons All rights reserved */
/* Daemon Workflow API - Core workflow endpoints (start, list, status, abandon) */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

/* Project includes */
#include "argo_daemon.h"
#include "argo_daemon_workflow_helpers.h"
#include "argo_http_server.h"
#include "argo_workflow_registry.h"
#include "argo_error.h"
#include "argo_log.h"
#include "argo_json.h"
#include "argo_limits.h"

/* External global daemon context (from argo_daemon_api_routes.c) */
extern argo_daemon_t* g_api_daemon;

/* POST /api/workflow/start - Start bash workflow script */
int api_workflow_start(http_request_t* req, http_response_t* resp) {
    if (!req || !resp || !g_api_daemon) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    /* Parse JSON body */
    if (!req->body) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing request body");
        return E_INPUT_NULL;
    }

    /* Extract script path from JSON */
    const char* field_path[] = {"script"};
    char* script_path = NULL;
    size_t script_len = 0;
    int result = json_extract_nested_string(req->body, field_path, 1, &script_path, &script_len);
    if (result != ARGO_SUCCESS || !script_path) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing 'script' field");
        free(script_path);
        return E_INPUT_FORMAT;
    }

    /* Extract template name from JSON (optional) */
    const char* template_field[] = {"template"};
    char* template_name = NULL;
    size_t template_len = 0;
    json_extract_nested_string(req->body, template_field, 1, &template_name, &template_len);

    /* Extract instance suffix from JSON (optional) */
    const char* instance_field[] = {"instance"};
    char* instance_suffix = NULL;
    size_t instance_len = 0;
    json_extract_nested_string(req->body, instance_field, 1, &instance_suffix, &instance_len);

    /* Extract args array from JSON (optional) */
    char** args = NULL;
    int arg_count = 0;
    result = parse_args_from_json(req->body, &args, &arg_count);
    if (result != ARGO_SUCCESS) {
        free(script_path);
        free(template_name);
        free(instance_suffix);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Memory allocation failed");
        return result;
    }

    /* Extract env object from JSON (optional) */
    char** env_keys = NULL;
    char** env_values = NULL;
    int env_count = 0;
    result = parse_env_from_json(req->body, &env_keys, &env_values, &env_count);
    if (result != ARGO_SUCCESS) {
        free(script_path);
        free(template_name);
        free(instance_suffix);
        free_workflow_params(args, arg_count, NULL, NULL, 0);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Memory allocation failed");
        return result;
    }

    /* Generate workflow ID from template name and instance suffix */
    char workflow_id[128];
    result = generate_workflow_id(g_api_daemon->workflow_registry, template_name,
                                  instance_suffix, workflow_id, sizeof(workflow_id));
    if (result != ARGO_SUCCESS) {
        free(script_path);
        free(template_name);
        free(instance_suffix);
        free_workflow_params(args, arg_count, env_keys, env_values, env_count);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to generate workflow ID");
        return result;
    }

    /* Execute bash workflow */
    result = daemon_execute_bash_workflow(g_api_daemon, script_path, args, arg_count,
                                         env_keys, env_values, env_count, workflow_id);

    /* Free allocated memory */
    free(script_path);
    free(template_name);
    free(instance_suffix);
    free_workflow_params(args, arg_count, env_keys, env_values, env_count);

    if (result != ARGO_SUCCESS) {
        if (result == E_DUPLICATE) {
            http_response_set_error(resp, HTTP_STATUS_CONFLICT, "Workflow already exists");
        } else {
            http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to start workflow");
        }
        return result;
    }

    /* Build success response */
    char response_json[512];
    snprintf(response_json, sizeof(response_json),
            "{\"status\":\"success\",\"workflow_id\":\"%s\"}",
            workflow_id);

    http_response_set_json(resp, HTTP_STATUS_OK, response_json);
    LOG_INFO("Started workflow via API: %s", workflow_id);
    return ARGO_SUCCESS;
}

/* GET /api/workflow/list - List all workflows */
int api_workflow_list(http_request_t* req, http_response_t* resp) {
    (void)req;  /* Unused */

    if (!resp || !g_api_daemon || !g_api_daemon->workflow_registry) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    /* Get all workflows from registry */
    workflow_entry_t* entries = NULL;
    int count = 0;
    int result = workflow_registry_list(g_api_daemon->workflow_registry, &entries, &count);
    if (result != ARGO_SUCCESS) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to list workflows");
        return result;
    }

    /* Build JSON response with safe string operations */
    size_t json_size = count * 256 + 100;
    char* json_response = malloc(json_size);
    if (!json_response) {
        free(entries);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Memory allocation failed");
        return E_SYSTEM_MEMORY;
    }

    /* Initialize with opening bracket using snprintf */
    size_t offset = 0;
    offset += snprintf(json_response, json_size, "{\"workflows\":[");

    /* Append each workflow entry safely */
    int first = 1;
    for (int i = 0; i < count; i++) {
        offset += snprintf(json_response + offset, json_size - offset,
                "%s{\"workflow_id\":\"%s\",\"script\":\"%s\",\"state\":\"%s\",\"pid\":%d}",
                (first ? "" : ","),
                entries[i].workflow_id,
                entries[i].workflow_name,
                workflow_state_to_string(entries[i].state),
                entries[i].executor_pid);

        first = 0;

        /* Safety check: prevent buffer overflow */
        if (offset >= json_size - 10) {
            LOG_ERROR("JSON response buffer too small");
            break;
        }
    }

    /* Append closing bracket safely */
    snprintf(json_response + offset, json_size - offset, "]}");

    http_response_set_json(resp, HTTP_STATUS_OK, json_response);
    free(json_response);
    free(entries);

    return ARGO_SUCCESS;
}

/* GET /api/workflow/status/{id} - Get workflow status */
int api_workflow_status(http_request_t* req, http_response_t* resp) {
    if (!req || !resp || !g_api_daemon || !g_api_daemon->workflow_registry) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    /* Extract workflow ID from path */
    /* Path format: /api/workflow/status/wf_123 */
    const char* id_start = strrchr(req->path, '/');
    if (!id_start || !*(id_start + 1)) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing workflow ID");
        return E_INPUT_NULL;
    }
    const char* workflow_id = id_start + 1;

    /* Find workflow in registry */
    const workflow_entry_t* entry = workflow_registry_find(g_api_daemon->workflow_registry, workflow_id);
    if (!entry) {
        http_response_set_error(resp, HTTP_STATUS_NOT_FOUND, "Workflow not found");
        return E_NOT_FOUND;
    }

    /* Build JSON response */
    char response_json[512];
    snprintf(response_json, sizeof(response_json),
            "{\"workflow_id\":\"%s\",\"script\":\"%s\",\"state\":\"%s\","
            "\"pid\":%d,\"start_time\":%ld,\"end_time\":%ld,\"exit_code\":%d}",
            entry->workflow_id,
            entry->workflow_name,
            workflow_state_to_string(entry->state),
            entry->executor_pid,
            (long)entry->start_time,
            (long)entry->end_time,
            entry->exit_code);

    http_response_set_json(resp, HTTP_STATUS_OK, response_json);
    return ARGO_SUCCESS;
}

/* DELETE /api/workflow/abandon/{id} - Abandon (kill) workflow */
int api_workflow_abandon(http_request_t* req, http_response_t* resp) {
    if (!req || !resp || !g_api_daemon || !g_api_daemon->workflow_registry) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    /* Extract workflow ID from path */
    const char* id_start = strrchr(req->path, '/');
    if (!id_start || !*(id_start + 1)) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing workflow ID");
        return E_INPUT_NULL;
    }
    const char* workflow_id = id_start + 1;

    /* Find workflow in registry */
    const workflow_entry_t* entry = workflow_registry_find(g_api_daemon->workflow_registry, workflow_id);
    if (!entry) {
        http_response_set_error(resp, HTTP_STATUS_NOT_FOUND, "Workflow not found");
        return E_NOT_FOUND;
    }

    /* Set abandon flag - completion task will handle state transition */
    workflow_entry_t* mutable_entry = (workflow_entry_t*)entry;
    mutable_entry->abandon_requested = true;

    /* Kill process if running */
    if (entry->executor_pid > 0 && entry->state == WORKFLOW_STATE_RUNNING) {
        if (kill(entry->executor_pid, SIGTERM) < 0) {
            LOG_ERROR("Failed to kill workflow PID %d: %s", entry->executor_pid, strerror(errno));
            http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to kill workflow process");
            return E_SYSTEM_PROCESS;
        }
        LOG_INFO("Sent SIGTERM to workflow %s (PID: %d)", workflow_id, entry->executor_pid);
    }

    /* Build success response */
    char response_json[256];
    snprintf(response_json, sizeof(response_json),
            "{\"status\":\"success\",\"workflow_id\":\"%s\",\"action\":\"abandoned\"}",
            workflow_id);

    http_response_set_json(resp, HTTP_STATUS_OK, response_json);
    return ARGO_SUCCESS;
}

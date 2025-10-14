/* © 2025 Casey Koons All rights reserved */
/* Daemon Workflow API - Bash workflow execution endpoints */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>

/* Project includes */
#include "argo_daemon.h"
#include "argo_http_server.h"
#include "argo_workflow_registry.h"
#include "argo_error.h"
#include "argo_log.h"
#include "argo_json.h"
#include "argo_limits.h"

/* External global daemon context (from argo_daemon_api_routes.c) */
extern argo_daemon_t* g_api_daemon;

/* Helper: Parse args array from JSON body */
static int parse_args_from_json(const char* json_body, char*** args_out, int* arg_count_out) {
    *args_out = NULL;
    *arg_count_out = 0;

    const char* args_start = strstr(json_body, "\"args\"");
    if (!args_start) {
        return ARGO_SUCCESS;  /* No args field - not an error */
    }

    /* Find array bounds */
    const char* array_start = strchr(args_start, '[');
    const char* array_end = strchr(args_start, ']');

    if (!array_start || !array_end || array_end <= array_start) {
        return ARGO_SUCCESS;  /* Empty or malformed - not fatal */
    }

    /* Count args by counting quotes */
    int arg_count = 0;
    const char* p = array_start;
    while (p < array_end) {
        if (*p == '"') arg_count++;
        p++;
    }
    arg_count /= 2;  /* Each arg has open and close quote */

    if (arg_count == 0) {
        return ARGO_SUCCESS;
    }

    char** args = malloc(sizeof(char*) * arg_count);
    if (!args) {
        return E_SYSTEM_MEMORY;
    }

    /* Extract each arg */
    int idx = 0;
    p = array_start + 1;
    while (p < array_end && idx < arg_count) {
        /* Skip whitespace and commas */
        while (*p == ' ' || *p == ',') p++;

        if (*p == '"') {
            p++;  /* Skip opening quote */
            const char* arg_start = p;
            const char* arg_end = strchr(p, '"');
            if (arg_end) {
                size_t len = arg_end - arg_start;
                args[idx] = malloc(len + 1);
                if (args[idx]) {
                    memcpy(args[idx], arg_start, len);
                    args[idx][len] = '\0';
                    idx++;
                }
                p = arg_end + 1;
            }
        } else {
            break;
        }
    }

    *args_out = args;
    *arg_count_out = idx;  /* Actual number extracted */
    return ARGO_SUCCESS;
}

/* Helper: Parse env object from JSON body */
static int parse_env_from_json(const char* json_body, char*** env_keys_out,
                               char*** env_values_out, int* env_count_out) {
    *env_keys_out = NULL;
    *env_values_out = NULL;
    *env_count_out = 0;

    const char* env_start = strstr(json_body, "\"env\"");
    if (!env_start) {
        return ARGO_SUCCESS;  /* No env field - not an error */
    }

    /* Find object bounds */
    const char* obj_start = strchr(env_start, '{');
    const char* obj_end = strchr(env_start, '}');

    if (!obj_start || !obj_end || obj_end <= obj_start) {
        return ARGO_SUCCESS;  /* Empty or malformed - not fatal */
    }

    /* Count env vars by counting colons */
    int env_count = 0;
    const char* p = obj_start;
    while (p < obj_end) {
        if (*p == ':') env_count++;
        p++;
    }

    if (env_count == 0) {
        return ARGO_SUCCESS;
    }

    char** env_keys = malloc(sizeof(char*) * env_count);
    char** env_values = malloc(sizeof(char*) * env_count);
    if (!env_keys || !env_values) {
        free(env_keys);
        free(env_values);
        return E_SYSTEM_MEMORY;
    }

    /* Extract each env var (KEY":"VALUE" format) */
    int idx = 0;
    p = obj_start + 1;
    while (p < obj_end && idx < env_count) {
        /* Skip whitespace and commas */
        while (*p == ' ' || *p == ',') p++;

        if (*p == '"') {
            /* Extract key */
            p++;  /* Skip opening quote */
            const char* key_start = p;
            const char* key_end = strchr(p, '"');
            if (key_end) {
                size_t key_len = key_end - key_start;
                env_keys[idx] = malloc(key_len + 1);
                if (env_keys[idx]) {
                    memcpy(env_keys[idx], key_start, key_len);
                    env_keys[idx][key_len] = '\0';
                }
                p = key_end + 1;

                /* Skip to value (past ":") */
                p = strchr(p, ':');
                if (p) {
                    p++;  /* Skip colon */
                    while (*p == ' ') p++;  /* Skip whitespace */
                    if (*p == '"') {
                        p++;  /* Skip opening quote */
                        const char* val_start = p;
                        const char* val_end = strchr(p, '"');
                        if (val_end) {
                            size_t val_len = val_end - val_start;
                            env_values[idx] = malloc(val_len + 1);
                            if (env_values[idx]) {
                                memcpy(env_values[idx], val_start, val_len);
                                env_values[idx][val_len] = '\0';
                                idx++;
                            }
                            p = val_end + 1;
                        }
                    }
                }
            }
        } else {
            break;
        }
    }

    *env_keys_out = env_keys;
    *env_values_out = env_values;
    *env_count_out = idx;  /* Actual number extracted */
    return ARGO_SUCCESS;
}

/* Helper: Free allocated args/env arrays */
static void free_workflow_params(char** args, int arg_count, char** env_keys,
                                char** env_values, int env_count) {
    for (int i = 0; i < arg_count; i++) {
        free(args[i]);
    }
    free(args);

    for (int i = 0; i < env_count; i++) {
        free(env_keys[i]);
        free(env_values[i]);
    }
    free(env_keys);
    free(env_values);
}

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

    /* Extract args array from JSON (optional) */
    char** args = NULL;
    int arg_count = 0;
    result = parse_args_from_json(req->body, &args, &arg_count);
    if (result != ARGO_SUCCESS) {
        free(script_path);
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
        free_workflow_params(args, arg_count, NULL, NULL, 0);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Memory allocation failed");
        return result;
    }

    /* Generate workflow ID with microseconds to avoid collisions */
    char workflow_id[64];
    struct timeval tv;
    gettimeofday(&tv, NULL);
    snprintf(workflow_id, sizeof(workflow_id), "wf_%ld_%ld",
             (long)tv.tv_sec, (long)tv.tv_usec);

    /* Execute bash workflow */
    result = daemon_execute_bash_workflow(g_api_daemon, script_path, args, arg_count,
                                         env_keys, env_values, env_count, workflow_id);

    /* Free allocated memory */
    free(script_path);
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

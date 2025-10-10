/* © 2025 Casey Koons All rights reserved */
/* Daemon API handlers for workflow I/O channel (HTTP-based interactive I/O) */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Project includes */
#include "argo_daemon_api.h"
#include "argo_daemon.h"
#include "argo_http_server.h"
#include "argo_workflow_registry.h"
#include "argo_error.h"
#include "argo_limits.h"

/* External global daemon context */
extern argo_daemon_t* g_api_daemon;

/* Helper: Get locked registry with validation */
static workflow_registry_t* get_locked_registry(http_response_t* resp) {
    if (!g_api_daemon) {
        if (resp) {
            http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Daemon not initialized");
        }
        return NULL;
    }

    pthread_mutex_lock(&g_api_daemon->workflow_registry_lock);

    if (!g_api_daemon->workflow_registry) {
        pthread_mutex_unlock(&g_api_daemon->workflow_registry_lock);
        if (resp) {
            http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Registry not initialized");
        }
        return NULL;
    }

    return g_api_daemon->workflow_registry;
}

/* Helper: Unlock registry */
static void unlock_registry(void) {
    if (g_api_daemon) {
        pthread_mutex_unlock(&g_api_daemon->workflow_registry_lock);
    }
}

/* Helper: Extract query parameter from URL (e.g., /api/workflow/input?workflow_name=foo -> "foo") */
static const char* extract_query_param(const char* path, const char* param_name) {
    if (!path || !param_name) return NULL;

    /* Find query string start */
    const char* query = strchr(path, '?');
    if (!query) return NULL;
    query++;  /* Skip '?' */

    /* Search for parameter name */
    char search_pattern[128];
    snprintf(search_pattern, sizeof(search_pattern), "%s=", param_name);

    const char* param_start = strstr(query, search_pattern);
    if (!param_start) return NULL;

    /* Skip to value */
    param_start += strlen(search_pattern);

    /* Find end of value (next '&' or end of string) */
    const char* param_end = strchr(param_start, '&');
    if (!param_end) param_end = param_start + strlen(param_start);

    /* Allocate and copy parameter value */
    size_t param_len = param_end - param_start;
    if (param_len == 0) return NULL;

    static char param_value[ARGO_BUFFER_NAME];
    if (param_len >= sizeof(param_value)) param_len = sizeof(param_value) - 1;

    memcpy(param_value, param_start, param_len);
    param_value[param_len] = '\0';

    return param_value;
}

/* POST /api/workflow/input?workflow_name=<name> - Enqueue user input for workflow */
int api_workflow_input_post(http_request_t* req, http_response_t* resp) {
    if (!req || !resp) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    const char* workflow_id = extract_query_param(req->path, "workflow_name");
    if (!workflow_id) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing workflow_name parameter");
        return E_INVALID_PARAMS;
    }

    /* Parse JSON body for input text */
    if (!req->body) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing request body");
        return E_INVALID_PARAMS;
    }

    /* Extract input field */
    char input_text[ARGO_BUFFER_STANDARD] = {0};
    const char* input_str = strstr(req->body, "\"input\"");
    if (!input_str) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing input field");
        return E_INVALID_PARAMS;
    }

    sscanf(input_str, "\"input\":\"%4095[^\"]\"", input_text);
    if (input_text[0] == '\0') {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Empty input");
        return E_INVALID_PARAMS;
    }

    /* Get locked registry */
    workflow_registry_t* registry = get_locked_registry(resp);
    if (!registry) {
        return E_INVALID_STATE;
    }

    /* Enqueue input */
    int result = workflow_registry_enqueue_input(registry, workflow_id, input_text);
    if (result == E_NOT_FOUND) {
        unlock_registry();
        http_response_set_error(resp, HTTP_STATUS_NOT_FOUND, "Workflow not found");
        return result;
    } else if (result == E_RESOURCE_LIMIT) {
        unlock_registry();
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Input queue full");
        return result;
    } else if (result != ARGO_SUCCESS) {
        unlock_registry();
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to enqueue input");
        return result;
    }

    unlock_registry();

    /* Return success */
    char response_json[ARGO_BUFFER_MEDIUM];
    snprintf(response_json, sizeof(response_json),
            "{\"status\":\"success\",\"workflow_id\":\"%s\",\"queued\":true}",
            workflow_id);

    http_response_set_json(resp, HTTP_STATUS_OK, response_json);
    return ARGO_SUCCESS;
}

/* GET /api/workflow/input?workflow_name=<name> - Dequeue input for executor (one item) */
int api_workflow_input_get(http_request_t* req, http_response_t* resp) {
    int result = ARGO_SUCCESS;
    workflow_registry_t* registry = NULL;
    char* input = NULL;

    if (!req || !resp) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_INVALID_PARAMS;
    }

    const char* workflow_id = extract_query_param(req->path, "workflow_name");
    if (!workflow_id) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing workflow_name parameter");
        return E_INVALID_PARAMS;
    }

    /* Get locked registry */
    registry = get_locked_registry(resp);
    if (!registry) {
        result = E_INVALID_STATE;
        goto error;
    }

    /* Check if workflow exists */
    workflow_instance_t* workflow = workflow_registry_get_workflow(registry, workflow_id);
    if (!workflow) {
        http_response_set_error(resp, HTTP_STATUS_NOT_FOUND, "Workflow not found");
        result = E_NOT_FOUND;
        goto cleanup;
    }

    /* Dequeue input (may return NULL if queue empty) */
    input = workflow_registry_dequeue_input(registry, workflow_id);
    if (!input) {
        http_response_set_json(resp, HTTP_STATUS_NO_CONTENT, "");
        goto cleanup;
    }

    /* Build response with input */
    char response_json[ARGO_BUFFER_STANDARD];
    snprintf(response_json, sizeof(response_json),
            "{\"workflow_id\":\"%s\",\"input\":\"%s\"}",
            workflow_id, input);

    http_response_set_json(resp, HTTP_STATUS_OK, response_json);

cleanup:
    if (registry) unlock_registry();
    free(input);
error:
    return result;
}

/* GET /api/workflow/output?workflow_name=<name>&since=<offset> - Stream workflow log output */
int api_workflow_output_get(http_request_t* req, http_response_t* resp) {
    int result = ARGO_SUCCESS;
    workflow_registry_t* registry = NULL;
    FILE* log_file = NULL;
    char* content = NULL;
    char* response_json = NULL;

    if (!req || !resp) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_INVALID_PARAMS;
    }

    const char* workflow_id = extract_query_param(req->path, "workflow_name");
    if (!workflow_id) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing workflow_name parameter");
        return E_INVALID_PARAMS;
    }

    /* Extract offset from query string (format: ?since=12345) */
    long offset = 0;
    const char* query = strchr(req->path, '?');
    if (query) {
        const char* since_param = strstr(query, "since=");
        if (since_param) {
            sscanf(since_param, "since=%ld", &offset);
        }
    }

    /* Get locked registry - verify workflow exists */
    registry = get_locked_registry(resp);
    if (!registry) {
        result = E_INVALID_STATE;
        goto error;
    }

    workflow_instance_t* workflow = workflow_registry_get_workflow(registry, workflow_id);
    if (!workflow) {
        http_response_set_error(resp, HTTP_STATUS_NOT_FOUND, "Workflow not found");
        result = E_NOT_FOUND;
        goto cleanup;
    }

    /* Unlock registry before file I/O (don't hold lock during I/O) */
    unlock_registry();
    registry = NULL;

    /* Build log file path */
    char log_path[ARGO_PATH_MAX];
    snprintf(log_path, sizeof(log_path), ".argo/logs/%s.log", workflow_id);

    /* Check if log file exists and get size */
    log_file = fopen(log_path, "r");
    if (!log_file) {
        http_response_set_json(resp, HTTP_STATUS_NO_CONTENT, "");
        goto cleanup;
    }

    /* Get file size */
    fseek(log_file, 0, SEEK_END);
    long file_size = ftell(log_file);

    /* Check if offset is beyond file size */
    if (offset >= file_size) {
        http_response_set_json(resp, HTTP_STATUS_NO_CONTENT, "");
        goto cleanup;
    }

    /* Seek to offset and read remaining content */
    fseek(log_file, offset, SEEK_SET);
    long bytes_to_read = file_size - offset;

    /* Limit response size to prevent memory issues */
    if (bytes_to_read > ARGO_BUFFER_LARGE) {
        bytes_to_read = ARGO_BUFFER_LARGE;
    }

    content = malloc(bytes_to_read + 1);
    if (!content) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Memory allocation failed");
        result = E_SYSTEM_MEMORY;
        goto cleanup;
    }

    size_t bytes_read = fread(content, 1, bytes_to_read, log_file);
    content[bytes_read] = '\0';

    /* Build JSON response with content and new offset */
    long new_offset = offset + bytes_read;
    response_json = malloc(bytes_to_read + ARGO_BUFFER_MEDIUM);
    if (!response_json) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Memory allocation failed");
        result = E_SYSTEM_MEMORY;
        goto cleanup;
    }

    snprintf(response_json, bytes_to_read + ARGO_BUFFER_MEDIUM,
            "{\"workflow_id\":\"%s\",\"offset\":%ld,\"content\":\"%s\"}",
            workflow_id, new_offset, content);

    http_response_set_json(resp, HTTP_STATUS_OK, response_json);

cleanup:
    if (registry) unlock_registry();
    if (log_file) fclose(log_file);
    free(content);
    free(response_json);
error:
    return result;
}


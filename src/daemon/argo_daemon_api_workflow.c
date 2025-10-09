/* © 2025 Casey Koons All rights reserved */
/* Daemon API handlers for workflow and registry operations */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

/* Project includes */
#include "argo_daemon_api.h"
#include "argo_daemon.h"
#include "argo_http_server.h"
#include "argo_workflow_registry.h"
#include "argo_workflow_templates.h"
#include "argo_orchestrator_api.h"
#include "argo_error.h"
#include "argo_json.h"
#include "argo_limits.h"
#include "argo_log.h"

/* Global daemon context for API handlers */
argo_daemon_t* g_api_daemon = NULL;

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

/* Helper: Extract path parameter (e.g., /api/workflow/status/{id} -> id) */
static const char* extract_path_param(const char* path, const char* prefix) {
    if (!path || !prefix) return NULL;

    size_t prefix_len = strlen(prefix);
    if (strncmp(path, prefix, prefix_len) != 0) return NULL;

    const char* param = path + prefix_len;
    if (*param == '/') param++;

    return (*param != '\0') ? param : NULL;
}

/* POST /api/workflow/start - Start new workflow */
int api_workflow_start(http_request_t* req, http_response_t* resp) {
    int result = ARGO_SUCCESS;
    workflow_registry_t* registry = NULL;
    workflow_template_collection_t* templates = NULL;
    char workflow_id[ARGO_BUFFER_MEDIUM] = {0};

    if (!req || !resp || !g_api_daemon) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_INVALID_PARAMS;
    }

    /* Parse JSON body */
    if (!req->body) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing request body");
        return E_INVALID_PARAMS;
    }

    /* Simple JSON parsing - look for required fields */
    /* Note: Using sscanf for simple field extraction. Full JSON parser integration tracked in issue #1 */
    char template_name[ARGO_BUFFER_NAME] = {0};
    char instance_name[ARGO_BUFFER_NAME] = {0};
    char branch[ARGO_BUFFER_SMALL] = "main";
    char environment[ARGO_BUFFER_TINY] = "dev";

    /* This is a simplified parser - in production would use argo_json.h */
    const char* template_str = strstr(req->body, "\"template\"");
    const char* instance_str = strstr(req->body, "\"instance\"");

    if (!template_str || !instance_str) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing template or instance");
        return E_INVALID_PARAMS;
    }

    /* Extract values (simplified) - using buffer size - 1 for scanf */
    sscanf(template_str, "\"template\":\"%127[^\"]\"", template_name);
    sscanf(instance_str, "\"instance\":\"%127[^\"]\"", instance_name);

    /* Optional branch and environment */
    const char* branch_str = strstr(req->body, "\"branch\"");
    if (branch_str) {
        sscanf(branch_str, "\"branch\":\"%63[^\"]\"", branch);
    }

    const char* env_str = strstr(req->body, "\"environment\"");
    if (env_str) {
        sscanf(env_str, "\"environment\":\"%31[^\"]\"", environment);
    }

    /* Validate template exists */
    templates = workflow_templates_create();
    if (!templates) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to load templates");
        result = E_SYSTEM_MEMORY;
        goto error;
    }

    result = workflow_templates_discover(templates);
    if (result != ARGO_SUCCESS) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to discover templates");
        goto cleanup;
    }

    workflow_template_t* template = workflow_templates_find(templates, template_name);
    if (!template) {
        http_response_set_error(resp, HTTP_STATUS_NOT_FOUND, "Template not found");
        result = E_NOT_FOUND;
        goto cleanup;
    }

    /* Get locked registry */
    registry = get_locked_registry(resp);
    if (!registry) {
        result = E_INVALID_STATE;
        goto cleanup;
    }

    /* Create workflow ID */
    snprintf(workflow_id, sizeof(workflow_id), "%s_%s", template_name, instance_name);

    /* Add workflow to registry */
    result = workflow_registry_add_workflow(registry, template_name, instance_name, branch, environment);
    if (result == E_DUPLICATE) {
        http_response_set_error(resp, HTTP_STATUS_CONFLICT, "Workflow already exists");
        goto cleanup;
    } else if (result != ARGO_SUCCESS) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to add workflow");
        goto cleanup;
    }

    /* Mark registry dirty for periodic save */
    registry->dirty = true;

    /* Start workflow execution (NOTE: passes shared registry, not owned) */
    result = workflow_exec_start(workflow_id, template->path, branch, registry);
    if (result != ARGO_SUCCESS) {
        workflow_registry_remove_workflow(registry, workflow_id);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to start workflow");
        goto cleanup;
    }

    /* Build success response */
    char response_json[ARGO_PATH_MAX];
    snprintf(response_json, sizeof(response_json),
        "{\"status\":\"success\",\"workflow_id\":\"%s\",\"environment\":\"%s\"}",
        workflow_id, environment);

    http_response_set_json(resp, HTTP_STATUS_OK, response_json);

cleanup:
    if (registry) unlock_registry();
    workflow_templates_destroy(templates);
error:
    return result;
}

/* GET /api/workflow/list - List all workflows */
int api_workflow_list(http_request_t* req, http_response_t* resp) {
    (void)req;  /* Unused */
    int result = ARGO_SUCCESS;
    workflow_registry_t* registry = NULL;
    char* json_response = NULL;

    if (!resp) {
        return E_INVALID_PARAMS;
    }

    /* Get locked registry */
    registry = get_locked_registry(resp);
    if (!registry) {
        result = E_INVALID_STATE;
        goto error;
    }

    /* Get workflow list */
    workflow_instance_t* workflows = NULL;
    int count = 0;
    result = workflow_registry_list(registry, &workflows, &count);
    if (result != ARGO_SUCCESS) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to list workflows");
        goto cleanup;
    }

    if (count == 0) {
        http_response_set_json(resp, HTTP_STATUS_OK, "{\"workflows\":[]}");
        goto cleanup;
    }

    /* Build JSON response */
    json_response = malloc(ARGO_BUFFER_STANDARD);
    if (!json_response) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Memory allocation failed");
        result = E_SYSTEM_MEMORY;
        goto cleanup;
    }

    size_t offset = 0;
    offset += snprintf(json_response + offset, ARGO_BUFFER_STANDARD - offset, "{\"workflows\":[");

    for (int i = 0; i < count; i++) {
        if (i > 0) {
            offset += snprintf(json_response + offset, ARGO_BUFFER_STANDARD - offset, ",");
        }
        offset += snprintf(json_response + offset, ARGO_BUFFER_STANDARD - offset,
            "{\"workflow_id\":\"%s\",\"status\":\"%s\",\"pid\":%d}",
            workflows[i].id,
            workflow_status_string(workflows[i].status),
            workflows[i].pid);
    }

    offset += snprintf(json_response + offset, ARGO_BUFFER_STANDARD - offset, "]}");

    http_response_set_json(resp, HTTP_STATUS_OK, json_response);

cleanup:
    if (registry) unlock_registry();
    free(json_response);
error:
    return result;
}

/* GET /api/workflow/status/{id} - Get workflow status */
int api_workflow_status(http_request_t* req, http_response_t* resp) {
    int result = ARGO_SUCCESS;
    workflow_registry_t* registry = NULL;

    if (!req || !resp) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_INVALID_PARAMS;
    }

    /* Extract workflow ID from path */
    const char* workflow_id = extract_path_param(req->path, "/api/workflow/status");
    if (!workflow_id) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing workflow ID");
        return E_INVALID_PARAMS;
    }

    /* Get locked registry */
    registry = get_locked_registry(resp);
    if (!registry) {
        result = E_INVALID_STATE;
        goto error;
    }

    /* Get workflow info */
    workflow_instance_t* info = workflow_registry_get_workflow(registry, workflow_id);
    if (!info) {
        http_response_set_error(resp, HTTP_STATUS_NOT_FOUND, "Workflow not found");
        result = E_NOT_FOUND;
        goto cleanup;
    }

    /* Build response */
    char response_json[ARGO_PATH_MAX];
    snprintf(response_json, sizeof(response_json),
        "{\"workflow_id\":\"%s\",\"status\":\"%s\",\"pid\":%d,\"template\":\"%s\"}",
        info->id,
        workflow_status_string(info->status),
        info->pid,
        info->template_name);

    http_response_set_json(resp, HTTP_STATUS_OK, response_json);

cleanup:
    if (registry) unlock_registry();
error:
    return result;
}

/* POST /api/workflow/pause/{id} - Pause workflow */
int api_workflow_pause(http_request_t* req, http_response_t* resp) {
    if (!req || !resp || !g_api_daemon) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    const char* workflow_id = extract_path_param(req->path, "/api/workflow/pause");
    if (!workflow_id) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing workflow ID");
        return E_INVALID_PARAMS;
    }

    /* Use shared registry with mutex protection */
    pthread_mutex_lock(&g_api_daemon->workflow_registry_lock);

    workflow_registry_t* registry = g_api_daemon->workflow_registry;
    if (!registry) {
        pthread_mutex_unlock(&g_api_daemon->workflow_registry_lock);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Registry not initialized");
        return E_INVALID_STATE;
    }

    /* Get workflow info */
    workflow_instance_t* info = workflow_registry_get_workflow(registry, workflow_id);
    if (!info) {
        pthread_mutex_unlock(&g_api_daemon->workflow_registry_lock);
        http_response_set_error(resp, HTTP_STATUS_NOT_FOUND, "Workflow not found");
        return E_NOT_FOUND;
    }

    /* Check if already paused */
    if (info->status == WORKFLOW_STATUS_SUSPENDED) {
        pthread_mutex_unlock(&g_api_daemon->workflow_registry_lock);
        char response_json[ARGO_BUFFER_MEDIUM];
        snprintf(response_json, sizeof(response_json),
            "{\"status\":\"already_paused\",\"workflow_id\":\"%s\"}",
            workflow_id);
        http_response_set_json(resp, HTTP_STATUS_OK, response_json);
        return ARGO_SUCCESS;
    }

    /* Check if workflow has active process */
    if (info->pid <= 0) {
        pthread_mutex_unlock(&g_api_daemon->workflow_registry_lock);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Workflow has no active process");
        return E_SYSTEM_PROCESS;
    }

    /* Send SIGSTOP to process */
    if (kill(info->pid, SIGSTOP) != 0) {
        pthread_mutex_unlock(&g_api_daemon->workflow_registry_lock);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to pause workflow process");
        return E_SYSTEM_PROCESS;
    }

    /* Update status to suspended */
    int result = workflow_registry_set_status(registry, workflow_id, WORKFLOW_STATUS_SUSPENDED);
    if (result != ARGO_SUCCESS) {
        pthread_mutex_unlock(&g_api_daemon->workflow_registry_lock);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to update workflow status");
        return result;
    }

    pthread_mutex_unlock(&g_api_daemon->workflow_registry_lock);

    /* Build success response */
    char response_json[ARGO_BUFFER_MEDIUM];
    snprintf(response_json, sizeof(response_json),
        "{\"status\":\"success\",\"workflow_id\":\"%s\",\"action\":\"paused\"}",
        workflow_id);

    http_response_set_json(resp, HTTP_STATUS_OK, response_json);
    return ARGO_SUCCESS;
}

/* POST /api/workflow/resume/{id} - Resume workflow */
int api_workflow_resume(http_request_t* req, http_response_t* resp) {
    if (!req || !resp || !g_api_daemon) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    const char* workflow_id = extract_path_param(req->path, "/api/workflow/resume");
    if (!workflow_id) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing workflow ID");
        return E_INVALID_PARAMS;
    }

    /* Use shared registry with mutex protection */
    pthread_mutex_lock(&g_api_daemon->workflow_registry_lock);

    workflow_registry_t* registry = g_api_daemon->workflow_registry;
    if (!registry) {
        pthread_mutex_unlock(&g_api_daemon->workflow_registry_lock);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Registry not initialized");
        return E_INVALID_STATE;
    }

    /* Get workflow info */
    workflow_instance_t* info = workflow_registry_get_workflow(registry, workflow_id);
    if (!info) {
        pthread_mutex_unlock(&g_api_daemon->workflow_registry_lock);
        http_response_set_error(resp, HTTP_STATUS_NOT_FOUND, "Workflow not found");
        return E_NOT_FOUND;
    }

    /* Check if already running */
    if (info->status == WORKFLOW_STATUS_ACTIVE) {
        pthread_mutex_unlock(&g_api_daemon->workflow_registry_lock);
        char response_json[ARGO_BUFFER_MEDIUM];
        snprintf(response_json, sizeof(response_json),
            "{\"status\":\"already_running\",\"workflow_id\":\"%s\"}",
            workflow_id);
        http_response_set_json(resp, HTTP_STATUS_OK, response_json);
        return ARGO_SUCCESS;
    }

    /* Check if workflow has active process */
    if (info->pid <= 0) {
        pthread_mutex_unlock(&g_api_daemon->workflow_registry_lock);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Workflow has no active process");
        return E_SYSTEM_PROCESS;
    }

    /* Send SIGCONT to process */
    if (kill(info->pid, SIGCONT) != 0) {
        pthread_mutex_unlock(&g_api_daemon->workflow_registry_lock);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to resume workflow process");
        return E_SYSTEM_PROCESS;
    }

    /* Update status to active */
    int result = workflow_registry_set_status(registry, workflow_id, WORKFLOW_STATUS_ACTIVE);
    if (result != ARGO_SUCCESS) {
        pthread_mutex_unlock(&g_api_daemon->workflow_registry_lock);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to update workflow status");
        return result;
    }

    pthread_mutex_unlock(&g_api_daemon->workflow_registry_lock);

    /* Build success response */
    char response_json[ARGO_BUFFER_MEDIUM];
    snprintf(response_json, sizeof(response_json),
        "{\"status\":\"success\",\"workflow_id\":\"%s\",\"action\":\"resumed\"}",
        workflow_id);

    http_response_set_json(resp, HTTP_STATUS_OK, response_json);
    return ARGO_SUCCESS;
}

/* DELETE /api/workflow/abandon/{id} - Abandon workflow */
int api_workflow_abandon(http_request_t* req, http_response_t* resp) {
    int result = ARGO_SUCCESS;
    workflow_registry_t* registry = NULL;

    if (!req || !resp) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_INVALID_PARAMS;
    }

    const char* workflow_id = extract_path_param(req->path, "/api/workflow/abandon");
    if (!workflow_id) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing workflow ID");
        return E_INVALID_PARAMS;
    }

    /* Get locked registry */
    registry = get_locked_registry(resp);
    if (!registry) {
        result = E_INVALID_STATE;
        goto error;
    }

    /* Get workflow to verify it exists */
    workflow_instance_t* info = workflow_registry_get_workflow(registry, workflow_id);
    if (!info) {
        http_response_set_error(resp, HTTP_STATUS_NOT_FOUND, "Workflow not found");
        result = E_NOT_FOUND;
        goto cleanup;
    }

    /* Abandon workflow (kills process) */
    result = workflow_exec_abandon(workflow_id, registry);
    if (result != ARGO_SUCCESS) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to abandon workflow");
        goto cleanup;
    }

    /* Remove from registry */
    result = workflow_registry_remove_workflow(registry, workflow_id);
    if (result != ARGO_SUCCESS) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to remove workflow from registry");
        goto cleanup;
    }

    /* Mark registry dirty for periodic save */
    registry->dirty = true;

    /* Build success response */
    char response_json[ARGO_BUFFER_MEDIUM];
    snprintf(response_json, sizeof(response_json),
        "{\"status\":\"success\",\"workflow_id\":\"%s\",\"action\":\"abandoned\"}",
        workflow_id);

    http_response_set_json(resp, HTTP_STATUS_OK, response_json);

cleanup:
    if (registry) unlock_registry();
error:
    return result;
}

/* POST /api/workflow/progress/{id} - Report executor progress */
int api_workflow_progress(http_request_t* req, http_response_t* resp) {
    if (!req || !resp) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    const char* workflow_id = extract_path_param(req->path, "/api/workflow/progress");
    if (!workflow_id) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing workflow ID");
        return E_INVALID_PARAMS;
    }

    /* Parse JSON body for progress info */
    if (!req->body) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing request body");
        return E_INVALID_PARAMS;
    }

    /* Simple JSON parsing for current_step and total_steps */
    int current_step = 0;
    int total_steps = 0;
    char step_name[ARGO_BUFFER_NAME] = {0};

    const char* current_str = strstr(req->body, "\"current_step\"");
    const char* total_str = strstr(req->body, "\"total_steps\"");
    const char* name_str = strstr(req->body, "\"step_name\"");

    if (current_str) {
        sscanf(current_str, "\"current_step\":%d", &current_step);
    }
    if (total_str) {
        sscanf(total_str, "\"total_steps\":%d", &total_steps);
    }
    if (name_str) {
        sscanf(name_str, "\"step_name\":\"%127[^\"]\"", step_name);  /* ARGO_BUFFER_NAME = 128 */
    }

    /* Log progress (in production, would update registry or database) */
    fprintf(stderr, "[PROGRESS] %s: step %d/%d (%s)\n",
            workflow_id, current_step, total_steps, step_name);

    /* Return success */
    char response_json[ARGO_BUFFER_MEDIUM];
    snprintf(response_json, sizeof(response_json),
            "{\"status\":\"success\",\"workflow_id\":\"%s\"}",
            workflow_id);

    http_response_set_json(resp, HTTP_STATUS_OK, response_json);
    return ARGO_SUCCESS;
}

/* POST /api/workflow/input/{id} - Enqueue user input for workflow */
int api_workflow_input_post(http_request_t* req, http_response_t* resp) {
    if (!req || !resp) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    const char* workflow_id = extract_path_param(req->path, "/api/workflow/input");
    if (!workflow_id) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing workflow ID");
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

/* GET /api/workflow/input/{id} - Dequeue input for executor (one item) */
int api_workflow_input_get(http_request_t* req, http_response_t* resp) {
    int result = ARGO_SUCCESS;
    workflow_registry_t* registry = NULL;
    char* input = NULL;

    if (!req || !resp) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_INVALID_PARAMS;
    }

    const char* workflow_id = extract_path_param(req->path, "/api/workflow/input");
    if (!workflow_id) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing workflow ID");
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

/* GET /api/workflow/output/{id}?since={offset} - Stream workflow log output */
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

    const char* workflow_id = extract_path_param(req->path, "/api/workflow/output");
    if (!workflow_id) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing workflow ID");
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


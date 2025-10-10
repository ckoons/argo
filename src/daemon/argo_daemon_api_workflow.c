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

/* Helper: Extract query parameter from URL (e.g., /api/workflow/abandon?workflow_name=foo -> "foo") */
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

/* GET /api/workflow/status?workflow_name=<name> - Get workflow status */
int api_workflow_status(http_request_t* req, http_response_t* resp) {
    int result = ARGO_SUCCESS;
    workflow_registry_t* registry = NULL;

    if (!req || !resp) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_INVALID_PARAMS;
    }

    /* Extract workflow ID from query parameter */
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

/* POST /api/workflow/pause?workflow_name=<name> - Pause workflow */
int api_workflow_pause(http_request_t* req, http_response_t* resp) {
    if (!req || !resp || !g_api_daemon) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    const char* workflow_id = extract_query_param(req->path, "workflow_name");
    if (!workflow_id) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing workflow_name parameter");
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

/* POST /api/workflow/resume?workflow_name=<name> - Resume workflow */
int api_workflow_resume(http_request_t* req, http_response_t* resp) {
    if (!req || !resp || !g_api_daemon) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    const char* workflow_id = extract_query_param(req->path, "workflow_name");
    if (!workflow_id) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing workflow_name parameter");
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

/* DELETE /api/workflow/abandon?workflow_name=<name> - Abandon workflow */
int api_workflow_abandon(http_request_t* req, http_response_t* resp) {
    int result = ARGO_SUCCESS;
    workflow_registry_t* registry = NULL;

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

/* POST /api/workflow/progress?workflow_name=<name> - Report executor progress */
int api_workflow_progress(http_request_t* req, http_response_t* resp) {
    if (!req || !resp) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    const char* workflow_id = extract_query_param(req->path, "workflow_name");
    if (!workflow_id) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing workflow_name parameter");
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

    /* Log progress */
    LOG_INFO("[PROGRESS] %s: step %d/%d (%s)",
            workflow_id, current_step, total_steps, step_name);

    /* Return success */
    char response_json[ARGO_BUFFER_MEDIUM];
    snprintf(response_json, sizeof(response_json),
            "{\"status\":\"success\",\"workflow_id\":\"%s\"}",
            workflow_id);

    http_response_set_json(resp, HTTP_STATUS_OK, response_json);
    return ARGO_SUCCESS;
}


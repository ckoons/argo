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

/* Global daemon context for API handlers */
argo_daemon_t* g_api_daemon = NULL;

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
    if (!req || !resp || !g_api_daemon) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    /* Parse JSON body */
    if (!req->body) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing request body");
        return E_INVALID_PARAMS;
    }

    /* Simple JSON parsing - look for required fields */
    /* TODO: Use proper JSON parser when available */
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

    /* Extract values (simplified) */
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
    workflow_template_collection_t* templates = workflow_templates_create();
    if (!templates) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to load templates");
        return E_SYSTEM_MEMORY;
    }

    int result = workflow_templates_discover(templates);
    if (result != ARGO_SUCCESS) {
        workflow_templates_destroy(templates);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to discover templates");
        return result;
    }

    workflow_template_t* template = workflow_templates_find(templates, template_name);
    if (!template) {
        workflow_templates_destroy(templates);
        http_response_set_error(resp, HTTP_STATUS_NOT_FOUND, "Template not found");
        return E_NOT_FOUND;
    }

    /* Load workflow registry */
    workflow_registry_t* registry = workflow_registry_create(".argo/workflows/registry/active_workflow_registry.json");
    if (!registry) {
        workflow_templates_destroy(templates);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to create registry");
        return E_SYSTEM_MEMORY;
    }

    result = workflow_registry_load(registry);
    if (result != ARGO_SUCCESS && result != E_SYSTEM_FILE) {
        workflow_registry_destroy(registry);
        workflow_templates_destroy(templates);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to load registry");
        return result;
    }

    /* Add workflow to registry */
    result = workflow_registry_add_workflow(registry, template_name, instance_name, branch, environment);
    if (result == E_DUPLICATE) {
        workflow_registry_destroy(registry);
        workflow_templates_destroy(templates);
        http_response_set_error(resp, HTTP_STATUS_CONFLICT, "Workflow already exists");
        return E_DUPLICATE;
    } else if (result != ARGO_SUCCESS) {
        workflow_registry_destroy(registry);
        workflow_templates_destroy(templates);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to add workflow");
        return result;
    }

    /* Create workflow ID */
    char workflow_id[ARGO_BUFFER_MEDIUM];
    snprintf(workflow_id, sizeof(workflow_id), "%s_%s", template_name, instance_name);

    /* Start workflow execution */
    result = workflow_exec_start(workflow_id, template->path, branch, registry);
    if (result != ARGO_SUCCESS) {
        workflow_registry_remove_workflow(registry, workflow_id);
        workflow_registry_destroy(registry);
        workflow_templates_destroy(templates);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to start workflow");
        return result;
    }

    /* Build success response */
    char response_json[ARGO_PATH_MAX];
    snprintf(response_json, sizeof(response_json),
        "{\"status\":\"success\",\"workflow_id\":\"%s\",\"environment\":\"%s\"}",
        workflow_id, environment);

    http_response_set_json(resp, HTTP_STATUS_OK, response_json);

    /* Cleanup */
    workflow_registry_destroy(registry);
    workflow_templates_destroy(templates);

    return ARGO_SUCCESS;
}

/* GET /api/workflow/list - List all workflows */
int api_workflow_list(http_request_t* req, http_response_t* resp) {
    (void)req;  /* Unused */

    if (!resp || !g_api_daemon) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    /* Load registry */
    workflow_registry_t* registry = workflow_registry_create(".argo/workflows/registry/active_workflow_registry.json");
    if (!registry) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to create registry");
        return E_SYSTEM_MEMORY;
    }

    int result = workflow_registry_load(registry);
    if (result != ARGO_SUCCESS && result != E_SYSTEM_FILE) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to load registry");
        return result;
    }

    /* Get workflow list */
    workflow_instance_t* workflows = NULL;
    int count = 0;
    result = workflow_registry_list(registry, &workflows, &count);
    if (result != ARGO_SUCCESS) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to list workflows");
        return result;
    }

    if (count == 0) {
        workflow_registry_destroy(registry);
        http_response_set_json(resp, HTTP_STATUS_OK, "{\"workflows\":[]}");
        return ARGO_SUCCESS;
    }

    /* Build JSON response (simplified) */
    char* json_response = malloc(4096);
    if (!json_response) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Memory allocation failed");
        return E_SYSTEM_MEMORY;
    }

    size_t offset = 0;
    offset += snprintf(json_response + offset, 4096 - offset, "{\"workflows\":[");

    for (int i = 0; i < count; i++) {
        if (i > 0) {
            offset += snprintf(json_response + offset, 4096 - offset, ",");
        }
        offset += snprintf(json_response + offset, 4096 - offset,
            "{\"workflow_id\":\"%s\",\"status\":\"%s\",\"pid\":%d}",
            workflows[i].id,
            workflow_status_string(workflows[i].status),
            workflows[i].pid);
    }

    offset += snprintf(json_response + offset, 4096 - offset, "]}");

    http_response_set_json(resp, HTTP_STATUS_OK, json_response);

    free(json_response);
    workflow_registry_destroy(registry);

    return ARGO_SUCCESS;
}

/* GET /api/workflow/status/{id} - Get workflow status */
int api_workflow_status(http_request_t* req, http_response_t* resp) {
    if (!req || !resp || !g_api_daemon) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    /* Extract workflow ID from path */
    const char* workflow_id = extract_path_param(req->path, "/api/workflow/status");
    if (!workflow_id) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing workflow ID");
        return E_INVALID_PARAMS;
    }

    /* Load registry */
    workflow_registry_t* registry = workflow_registry_create(".argo/workflows/registry/active_workflow_registry.json");
    if (!registry) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to create registry");
        return E_SYSTEM_MEMORY;
    }

    int result = workflow_registry_load(registry);
    if (result != ARGO_SUCCESS && result != E_SYSTEM_FILE) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to load registry");
        return result;
    }

    /* Get workflow info */
    workflow_instance_t* info = workflow_registry_get_workflow(registry, workflow_id);
    if (!info) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, HTTP_STATUS_NOT_FOUND, "Workflow not found");
        return E_NOT_FOUND;
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

    workflow_registry_destroy(registry);
    return ARGO_SUCCESS;
}

/* POST /api/workflow/pause/{id} - Pause workflow */
int api_workflow_pause(http_request_t* req, http_response_t* resp) {
    if (!req || !resp) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    const char* workflow_id = extract_path_param(req->path, "/api/workflow/pause");
    if (!workflow_id) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing workflow ID");
        return E_INVALID_PARAMS;
    }

    /* Load registry */
    workflow_registry_t* registry = workflow_registry_create(".argo/workflows/registry/active_workflow_registry.json");
    if (!registry) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to create registry");
        return E_SYSTEM_MEMORY;
    }

    int result = workflow_registry_load(registry);
    if (result != ARGO_SUCCESS && result != E_SYSTEM_FILE) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to load registry");
        return result;
    }

    /* Get workflow info */
    workflow_instance_t* info = workflow_registry_get_workflow(registry, workflow_id);
    if (!info) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, HTTP_STATUS_NOT_FOUND, "Workflow not found");
        return E_NOT_FOUND;
    }

    /* Check if already paused */
    if (info->status == WORKFLOW_STATUS_SUSPENDED) {
        workflow_registry_destroy(registry);
        char response_json[ARGO_BUFFER_MEDIUM];
        snprintf(response_json, sizeof(response_json),
            "{\"status\":\"already_paused\",\"workflow_id\":\"%s\"}",
            workflow_id);
        http_response_set_json(resp, HTTP_STATUS_OK, response_json);
        return ARGO_SUCCESS;
    }

    /* Check if workflow has active process */
    if (info->pid <= 0) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Workflow has no active process");
        return E_SYSTEM_PROCESS;
    }

    /* Send SIGSTOP to process */
    if (kill(info->pid, SIGSTOP) != 0) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to pause workflow process");
        return E_SYSTEM_PROCESS;
    }

    /* Update status to suspended */
    result = workflow_registry_set_status(registry, workflow_id, WORKFLOW_STATUS_SUSPENDED);
    if (result != ARGO_SUCCESS) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to update workflow status");
        return result;
    }

    /* Build success response */
    char response_json[ARGO_BUFFER_MEDIUM];
    snprintf(response_json, sizeof(response_json),
        "{\"status\":\"success\",\"workflow_id\":\"%s\",\"action\":\"paused\"}",
        workflow_id);

    http_response_set_json(resp, HTTP_STATUS_OK, response_json);
    workflow_registry_destroy(registry);
    return ARGO_SUCCESS;
}

/* POST /api/workflow/resume/{id} - Resume workflow */
int api_workflow_resume(http_request_t* req, http_response_t* resp) {
    if (!req || !resp) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    const char* workflow_id = extract_path_param(req->path, "/api/workflow/resume");
    if (!workflow_id) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing workflow ID");
        return E_INVALID_PARAMS;
    }

    /* Load registry */
    workflow_registry_t* registry = workflow_registry_create(".argo/workflows/registry/active_workflow_registry.json");
    if (!registry) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to create registry");
        return E_SYSTEM_MEMORY;
    }

    int result = workflow_registry_load(registry);
    if (result != ARGO_SUCCESS && result != E_SYSTEM_FILE) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to load registry");
        return result;
    }

    /* Get workflow info */
    workflow_instance_t* info = workflow_registry_get_workflow(registry, workflow_id);
    if (!info) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, HTTP_STATUS_NOT_FOUND, "Workflow not found");
        return E_NOT_FOUND;
    }

    /* Check if already running */
    if (info->status == WORKFLOW_STATUS_ACTIVE) {
        workflow_registry_destroy(registry);
        char response_json[ARGO_BUFFER_MEDIUM];
        snprintf(response_json, sizeof(response_json),
            "{\"status\":\"already_running\",\"workflow_id\":\"%s\"}",
            workflow_id);
        http_response_set_json(resp, HTTP_STATUS_OK, response_json);
        return ARGO_SUCCESS;
    }

    /* Check if workflow has active process */
    if (info->pid <= 0) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Workflow has no active process");
        return E_SYSTEM_PROCESS;
    }

    /* Send SIGCONT to process */
    if (kill(info->pid, SIGCONT) != 0) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to resume workflow process");
        return E_SYSTEM_PROCESS;
    }

    /* Update status to active */
    result = workflow_registry_set_status(registry, workflow_id, WORKFLOW_STATUS_ACTIVE);
    if (result != ARGO_SUCCESS) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to update workflow status");
        return result;
    }

    /* Build success response */
    char response_json[ARGO_BUFFER_MEDIUM];
    snprintf(response_json, sizeof(response_json),
        "{\"status\":\"success\",\"workflow_id\":\"%s\",\"action\":\"resumed\"}",
        workflow_id);

    http_response_set_json(resp, HTTP_STATUS_OK, response_json);
    workflow_registry_destroy(registry);
    return ARGO_SUCCESS;
}

/* DELETE /api/workflow/abandon/{id} - Abandon workflow */
int api_workflow_abandon(http_request_t* req, http_response_t* resp) {
    if (!req || !resp) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    const char* workflow_id = extract_path_param(req->path, "/api/workflow/abandon");
    if (!workflow_id) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing workflow ID");
        return E_INVALID_PARAMS;
    }

    /* Load registry */
    workflow_registry_t* registry = workflow_registry_create(".argo/workflows/registry/active_workflow_registry.json");
    if (!registry) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to create registry");
        return E_SYSTEM_MEMORY;
    }

    int result = workflow_registry_load(registry);
    if (result != ARGO_SUCCESS && result != E_SYSTEM_FILE) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to load registry");
        return result;
    }

    /* Get workflow to get PID */
    workflow_instance_t* info = workflow_registry_get_workflow(registry, workflow_id);
    if (!info) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, HTTP_STATUS_NOT_FOUND, "Workflow not found");
        return E_NOT_FOUND;
    }

    /* Abandon workflow */
    result = workflow_exec_abandon(workflow_id, registry);
    if (result != ARGO_SUCCESS) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to abandon workflow");
        return result;
    }

    /* Remove from registry */
    workflow_registry_remove_workflow(registry, workflow_id);

    char response_json[ARGO_BUFFER_MEDIUM];
    snprintf(response_json, sizeof(response_json),
        "{\"status\":\"success\",\"workflow_id\":\"%s\",\"action\":\"abandoned\"}",
        workflow_id);

    http_response_set_json(resp, HTTP_STATUS_OK, response_json);

    workflow_registry_destroy(registry);
    return ARGO_SUCCESS;
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
        sscanf(name_str, "\"step_name\":\"%127[^\"]\"", step_name);
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

/* GET /api/registry/ci - List CIs */
int api_registry_list_ci(http_request_t* req, http_response_t* resp) {
    (void)req;  /* Unused */

    if (!resp || !g_api_daemon || !g_api_daemon->registry) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    /* For now, return empty list */
    http_response_set_json(resp, HTTP_STATUS_OK, "{\"cis\":[]}");
    return ARGO_SUCCESS;
}

/* Register all API routes with daemon */
int argo_daemon_register_api_routes(argo_daemon_t* daemon) {
    if (!daemon) return E_INVALID_PARAMS;

    g_api_daemon = daemon;

    /* Workflow routes */
    http_server_add_route(daemon->http_server, HTTP_METHOD_POST,
                         "/api/workflow/start", api_workflow_start);
    http_server_add_route(daemon->http_server, HTTP_METHOD_GET,
                         "/api/workflow/list", api_workflow_list);
    http_server_add_route(daemon->http_server, HTTP_METHOD_GET,
                         "/api/workflow/status", api_workflow_status);
    http_server_add_route(daemon->http_server, HTTP_METHOD_POST,
                         "/api/workflow/pause", api_workflow_pause);
    http_server_add_route(daemon->http_server, HTTP_METHOD_POST,
                         "/api/workflow/resume", api_workflow_resume);
    http_server_add_route(daemon->http_server, HTTP_METHOD_DELETE,
                         "/api/workflow/abandon", api_workflow_abandon);
    http_server_add_route(daemon->http_server, HTTP_METHOD_POST,
                         "/api/workflow/progress", api_workflow_progress);

    /* Registry routes */
    http_server_add_route(daemon->http_server, HTTP_METHOD_GET,
                         "/api/registry/ci", api_registry_list_ci);

    return ARGO_SUCCESS;
}

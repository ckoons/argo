/* © 2025 Casey Koons All rights reserved */
/* Daemon API handlers for workflow and registry operations */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Project includes */
#include "argo_daemon_api.h"
#include "argo_daemon.h"
#include "argo_http_server.h"
#include "argo_workflow_registry.h"
#include "argo_workflow_templates.h"
#include "argo_orchestrator_api.h"
#include "argo_error.h"
#include "argo_json.h"

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
        http_response_set_error(resp, 500, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    /* Parse JSON body */
    if (!req->body) {
        http_response_set_error(resp, 400, "Missing request body");
        return E_INVALID_PARAMS;
    }

    /* Simple JSON parsing - look for required fields */
    /* TODO: Use proper JSON parser when available */
    char template_name[128] = {0};
    char instance_name[128] = {0};
    char branch[64] = "main";
    char environment[32] = "dev";

    /* This is a simplified parser - in production would use argo_json.h */
    const char* template_str = strstr(req->body, "\"template\"");
    const char* instance_str = strstr(req->body, "\"instance\"");

    if (!template_str || !instance_str) {
        http_response_set_error(resp, 400, "Missing template or instance");
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
        http_response_set_error(resp, 500, "Failed to load templates");
        return E_SYSTEM_MEMORY;
    }

    int result = workflow_templates_discover(templates);
    if (result != ARGO_SUCCESS) {
        workflow_templates_destroy(templates);
        http_response_set_error(resp, 500, "Failed to discover templates");
        return result;
    }

    workflow_template_t* template = workflow_templates_find(templates, template_name);
    if (!template) {
        workflow_templates_destroy(templates);
        http_response_set_error(resp, 404, "Template not found");
        return E_NOT_FOUND;
    }

    /* Load workflow registry */
    workflow_registry_t* registry = workflow_registry_create(".argo/workflows/registry/active_workflow_registry.json");
    if (!registry) {
        workflow_templates_destroy(templates);
        http_response_set_error(resp, 500, "Failed to create registry");
        return E_SYSTEM_MEMORY;
    }

    result = workflow_registry_load(registry);
    if (result != ARGO_SUCCESS && result != E_SYSTEM_FILE) {
        workflow_registry_destroy(registry);
        workflow_templates_destroy(templates);
        http_response_set_error(resp, 500, "Failed to load registry");
        return result;
    }

    /* Add workflow to registry */
    result = workflow_registry_add_workflow(registry, template_name, instance_name, branch, environment);
    if (result == E_DUPLICATE) {
        workflow_registry_destroy(registry);
        workflow_templates_destroy(templates);
        http_response_set_error(resp, 409, "Workflow already exists");
        return E_DUPLICATE;
    } else if (result != ARGO_SUCCESS) {
        workflow_registry_destroy(registry);
        workflow_templates_destroy(templates);
        http_response_set_error(resp, 500, "Failed to add workflow");
        return result;
    }

    /* Create workflow ID */
    char workflow_id[256];
    snprintf(workflow_id, sizeof(workflow_id), "%s_%s", template_name, instance_name);

    /* Start workflow execution */
    result = workflow_exec_start(workflow_id, template->path, branch, registry);
    if (result != ARGO_SUCCESS) {
        workflow_registry_remove_workflow(registry, workflow_id);
        workflow_registry_destroy(registry);
        workflow_templates_destroy(templates);
        http_response_set_error(resp, 500, "Failed to start workflow");
        return result;
    }

    /* Build success response */
    char response_json[512];
    snprintf(response_json, sizeof(response_json),
        "{\"status\":\"success\",\"workflow_id\":\"%s\",\"environment\":\"%s\"}",
        workflow_id, environment);

    http_response_set_json(resp, 200, response_json);

    /* Cleanup */
    workflow_registry_destroy(registry);
    workflow_templates_destroy(templates);

    return ARGO_SUCCESS;
}

/* GET /api/workflow/list - List all workflows */
int api_workflow_list(http_request_t* req, http_response_t* resp) {
    (void)req;  /* Unused */

    if (!resp || !g_api_daemon) {
        http_response_set_error(resp, 500, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    /* Load registry */
    workflow_registry_t* registry = workflow_registry_create(".argo/workflows/registry/active_workflow_registry.json");
    if (!registry) {
        http_response_set_error(resp, 500, "Failed to create registry");
        return E_SYSTEM_MEMORY;
    }

    int result = workflow_registry_load(registry);
    if (result != ARGO_SUCCESS && result != E_SYSTEM_FILE) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, 500, "Failed to load registry");
        return result;
    }

    /* Get workflow list */
    workflow_instance_t* workflows[64];
    int count = 0;
    result = workflow_registry_list(registry, workflows, &count);
    if (result != ARGO_SUCCESS) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, 500, "Failed to list workflows");
        return result;
    }

    if (count == 0) {
        workflow_registry_destroy(registry);
        http_response_set_json(resp, 200, "{\"workflows\":[]}");
        return ARGO_SUCCESS;
    }

    /* Build JSON response (simplified) */
    char* json_response = malloc(4096);
    if (!json_response) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, 500, "Memory allocation failed");
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
            workflows[i]->id,
            workflow_status_string(workflows[i]->status),
            workflows[i]->pid);
    }

    offset += snprintf(json_response + offset, 4096 - offset, "]}");

    http_response_set_json(resp, 200, json_response);

    free(json_response);
    workflow_registry_destroy(registry);

    return ARGO_SUCCESS;
}

/* GET /api/workflow/status/{id} - Get workflow status */
int api_workflow_status(http_request_t* req, http_response_t* resp) {
    if (!req || !resp || !g_api_daemon) {
        http_response_set_error(resp, 500, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    /* Extract workflow ID from path */
    const char* workflow_id = extract_path_param(req->path, "/api/workflow/status");
    if (!workflow_id) {
        http_response_set_error(resp, 400, "Missing workflow ID");
        return E_INVALID_PARAMS;
    }

    /* Load registry */
    workflow_registry_t* registry = workflow_registry_create(".argo/workflows/registry/active_workflow_registry.json");
    if (!registry) {
        http_response_set_error(resp, 500, "Failed to create registry");
        return E_SYSTEM_MEMORY;
    }

    int result = workflow_registry_load(registry);
    if (result != ARGO_SUCCESS && result != E_SYSTEM_FILE) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, 500, "Failed to load registry");
        return result;
    }

    /* Get workflow info */
    workflow_instance_t* info = workflow_registry_get_workflow(registry, workflow_id);
    if (!info) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, 404, "Workflow not found");
        return E_NOT_FOUND;
    }

    /* Build response */
    char response_json[512];
    snprintf(response_json, sizeof(response_json),
        "{\"workflow_id\":\"%s\",\"status\":\"%s\",\"pid\":%d,\"template\":\"%s\"}",
        info->id,
        workflow_status_string(info->status),
        info->pid,
        info->template_name);

    http_response_set_json(resp, 200, response_json);

    workflow_registry_destroy(registry);
    return ARGO_SUCCESS;
}

/* POST /api/workflow/pause/{id} - Pause workflow */
int api_workflow_pause(http_request_t* req, http_response_t* resp) {
    if (!req || !resp) {
        http_response_set_error(resp, 500, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    const char* workflow_id = extract_path_param(req->path, "/api/workflow/pause");
    if (!workflow_id) {
        http_response_set_error(resp, 400, "Missing workflow ID");
        return E_INVALID_PARAMS;
    }

    /* For now, just return success - actual pause logic will be added */
    char response_json[256];
    snprintf(response_json, sizeof(response_json),
        "{\"status\":\"success\",\"workflow_id\":\"%s\",\"action\":\"paused\"}",
        workflow_id);

    http_response_set_json(resp, 200, response_json);
    return ARGO_SUCCESS;
}

/* POST /api/workflow/resume/{id} - Resume workflow */
int api_workflow_resume(http_request_t* req, http_response_t* resp) {
    if (!req || !resp) {
        http_response_set_error(resp, 500, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    const char* workflow_id = extract_path_param(req->path, "/api/workflow/resume");
    if (!workflow_id) {
        http_response_set_error(resp, 400, "Missing workflow ID");
        return E_INVALID_PARAMS;
    }

    /* For now, just return success */
    char response_json[256];
    snprintf(response_json, sizeof(response_json),
        "{\"status\":\"success\",\"workflow_id\":\"%s\",\"action\":\"resumed\"}",
        workflow_id);

    http_response_set_json(resp, 200, response_json);
    return ARGO_SUCCESS;
}

/* DELETE /api/workflow/abandon/{id} - Abandon workflow */
int api_workflow_abandon(http_request_t* req, http_response_t* resp) {
    if (!req || !resp) {
        http_response_set_error(resp, 500, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    const char* workflow_id = extract_path_param(req->path, "/api/workflow/abandon");
    if (!workflow_id) {
        http_response_set_error(resp, 400, "Missing workflow ID");
        return E_INVALID_PARAMS;
    }

    /* Load registry */
    workflow_registry_t* registry = workflow_registry_create(".argo/workflows/registry/active_workflow_registry.json");
    if (!registry) {
        http_response_set_error(resp, 500, "Failed to create registry");
        return E_SYSTEM_MEMORY;
    }

    int result = workflow_registry_load(registry);
    if (result != ARGO_SUCCESS && result != E_SYSTEM_FILE) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, 500, "Failed to load registry");
        return result;
    }

    /* Get workflow to get PID */
    workflow_instance_t* info = workflow_registry_get_workflow(registry, workflow_id);
    if (!info) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, 404, "Workflow not found");
        return E_NOT_FOUND;
    }

    /* Abandon workflow */
    result = workflow_exec_abandon(workflow_id, registry);
    if (result != ARGO_SUCCESS) {
        workflow_registry_destroy(registry);
        http_response_set_error(resp, 500, "Failed to abandon workflow");
        return result;
    }

    /* Remove from registry */
    workflow_registry_remove_workflow(registry, workflow_id);

    char response_json[256];
    snprintf(response_json, sizeof(response_json),
        "{\"status\":\"success\",\"workflow_id\":\"%s\",\"action\":\"abandoned\"}",
        workflow_id);

    http_response_set_json(resp, 200, response_json);

    workflow_registry_destroy(registry);
    return ARGO_SUCCESS;
}

/* GET /api/registry/ci - List CIs */
int api_registry_list_ci(http_request_t* req, http_response_t* resp) {
    (void)req;  /* Unused */

    if (!resp || !g_api_daemon || !g_api_daemon->registry) {
        http_response_set_error(resp, 500, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    /* For now, return empty list */
    http_response_set_json(resp, 200, "{\"cis\":[]}");
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

    /* Registry routes */
    http_server_add_route(daemon->http_server, HTTP_METHOD_GET,
                         "/api/registry/ci", api_registry_list_ci);

    return ARGO_SUCCESS;
}

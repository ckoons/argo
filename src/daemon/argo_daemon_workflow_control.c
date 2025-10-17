/* © 2025 Casey Koons All rights reserved */
/* Daemon Workflow Control - Pause, resume, and input endpoints */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

/* Project includes */
#include "argo_daemon.h"
#include "argo_http_server.h"
#include "argo_workflow_registry.h"
#include "argo_error.h"
#include "argo_log.h"
#include "argo_json.h"

/* External global daemon context (from argo_daemon_api_routes.c) */
extern argo_daemon_t* g_api_daemon;

/* Unescape JSON string sequences in place */
static void unescape_json_string(char* str) {
    if (!str) return;

    char* src = str;
    char* dst = str;

    while (*src) {
        if (*src == '\\' && *(src + 1)) {
            src++;  /* Skip backslash */
            switch (*src) {
                case 'n':  *dst++ = '\n'; break;
                case 'r':  *dst++ = '\r'; break;
                case 't':  *dst++ = '\t'; break;
                case 'b':  *dst++ = '\b'; break;
                case 'f':  *dst++ = '\f'; break;
                case '"':  *dst++ = '"'; break;
                case '\\': *dst++ = '\\'; break;
                case '/':  *dst++ = '/'; break;
                default:   *dst++ = *src; break;  /* Unknown escape, keep as-is */
            }
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

/* POST /api/workflow/pause/{id} - Pause workflow execution */
int api_workflow_pause(http_request_t* req, http_response_t* resp) {
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

    /* Check if workflow is running */
    if (entry->state != WORKFLOW_STATE_RUNNING) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg),
                "Workflow is not running (state: %s)",
                workflow_state_to_string(entry->state));
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, error_msg);
        return E_INVALID_STATE;
    }

    /* Pause process using SIGSTOP */
    if (entry->executor_pid > 0) {
        if (kill(entry->executor_pid, SIGSTOP) < 0) {
            LOG_ERROR("Failed to pause workflow PID %d: %s", entry->executor_pid, strerror(errno));
            http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to pause workflow process");
            return E_SYSTEM_PROCESS;
        }
        LOG_INFO("Paused workflow %s (PID: %d)", workflow_id, entry->executor_pid);
    }

    /* Update state to paused */
    workflow_entry_t* mutable_entry = (workflow_entry_t*)entry;
    mutable_entry->state = WORKFLOW_STATE_PAUSED;

    /* Build success response */
    char response_json[256];
    snprintf(response_json, sizeof(response_json),
            "{\"status\":\"success\",\"workflow_id\":\"%s\",\"action\":\"paused\"}",
            workflow_id);

    http_response_set_json(resp, HTTP_STATUS_OK, response_json);
    return ARGO_SUCCESS;
}

/* POST /api/workflow/resume/{id} - Resume paused workflow */
int api_workflow_resume(http_request_t* req, http_response_t* resp) {
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

    /* Check if workflow is paused */
    if (entry->state != WORKFLOW_STATE_PAUSED) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg),
                "Workflow is not paused (state: %s)",
                workflow_state_to_string(entry->state));
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, error_msg);
        return E_INVALID_STATE;
    }

    /* Resume process using SIGCONT */
    if (entry->executor_pid > 0) {
        if (kill(entry->executor_pid, SIGCONT) < 0) {
            LOG_ERROR("Failed to resume workflow PID %d: %s", entry->executor_pid, strerror(errno));
            http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to resume workflow process");
            return E_SYSTEM_PROCESS;
        }
        LOG_INFO("Resumed workflow %s (PID: %d)", workflow_id, entry->executor_pid);
    }

    /* Update state back to running */
    workflow_entry_t* mutable_entry = (workflow_entry_t*)entry;
    mutable_entry->state = WORKFLOW_STATE_RUNNING;

    /* Build success response */
    char response_json[256];
    snprintf(response_json, sizeof(response_json),
            "{\"status\":\"success\",\"workflow_id\":\"%s\",\"action\":\"resumed\"}",
            workflow_id);

    http_response_set_json(resp, HTTP_STATUS_OK, response_json);
    return ARGO_SUCCESS;
}

/* POST /api/workflow/input/{id} - Send user input to workflow */
int api_workflow_input(http_request_t* req, http_response_t* resp) {
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

    /* Check if workflow is running */
    if (entry->state != WORKFLOW_STATE_RUNNING && entry->state != WORKFLOW_STATE_PAUSED) {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg),
                "Workflow is not running (state: %s)",
                workflow_state_to_string(entry->state));
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, error_msg);
        return E_INVALID_STATE;
    }

    /* Check if stdin pipe is available */
    if (entry->stdin_pipe <= 0) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Workflow has no stdin pipe");
        return E_INVALID_STATE;
    }

    /* Parse JSON body to extract input text */
    if (!req->body) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing request body");
        return E_INPUT_NULL;
    }

    const char* field_path[] = {"input"};
    char* input_text = NULL;
    size_t input_len = 0;
    int result = json_extract_nested_string(req->body, field_path, 1, &input_text, &input_len);
    if (result != ARGO_SUCCESS || !input_text) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing 'input' field");
        free(input_text);
        return E_INPUT_FORMAT;
    }

    /* Unescape JSON sequences (\n -> newline, etc.) */
    unescape_json_string(input_text);
    input_len = strlen(input_text);  /* Recalculate length after unescaping */

    /* Write input to workflow's stdin pipe */
    ssize_t written = write(entry->stdin_pipe, input_text, input_len);
    free(input_text);

    if (written < 0) {
        LOG_ERROR("Failed to write to workflow stdin: %s", strerror(errno));
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to send input to workflow");
        return E_SYSTEM_PROCESS;
    }

    /* Build success response */
    char response_json[256];
    snprintf(response_json, sizeof(response_json),
            "{\"status\":\"success\",\"workflow_id\":\"%s\",\"bytes_written\":%zd}",
            workflow_id, written);

    http_response_set_json(resp, HTTP_STATUS_OK, response_json);
    LOG_INFO("Sent input to workflow %s: %zd bytes", workflow_id, written);
    return ARGO_SUCCESS;
}

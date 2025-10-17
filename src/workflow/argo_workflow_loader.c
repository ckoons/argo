/* © 2025 Casey Koons All rights reserved */
/* Workflow loader - parse JSON workflow configuration */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Project includes */
#include "argo_workflow.h"
#include "argo_error.h"
#include "argo_limits.h"
#include "argo_log.h"
#include "argo_json.h"
#include "argo_file_utils.h"

/* JSON field names */
static const char* JSON_FIELD_WORKFLOW_NAME = "workflow_name";
static const char* JSON_FIELD_DESCRIPTION = "description";
static const char* JSON_FIELD_SCRIPT = "script";
static const char* JSON_FIELD_WORKING_DIR = "working_dir";
static const char* JSON_FIELD_TIMEOUT_SECONDS = "timeout_seconds";

/* Helper: Extract string field from JSON */
static int extract_string_field(const char* json, const char* field_name,
                                char* output, size_t output_size) {
    const char* field_path[] = {field_name};
    char* value = NULL;
    size_t value_len = 0;

    int result = json_extract_nested_string(json, field_path, 1, &value, &value_len);
    if (result != ARGO_SUCCESS || !value) {
        return result;
    }

    if (value_len >= output_size) {
        free(value);
        return E_PROTOCOL_SIZE;
    }

    strncpy(output, value, output_size - 1);
    output[output_size - 1] = '\0';
    free(value);

    return ARGO_SUCCESS;
}

/* Helper: Extract integer field from JSON */
static int extract_int_field(const char* json, const char* field_name,
                             int* output, int default_value) {
    char search_pattern[ARGO_BUFFER_MEDIUM];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\"", field_name);

    const char* field_start = strstr(json, search_pattern);
    if (!field_start) {
        *output = default_value;
        return ARGO_SUCCESS;
    }

    /* Find colon after field name */
    const char* colon = strchr(field_start, ':');
    if (!colon) {
        *output = default_value;
        return ARGO_SUCCESS;
    }

    /* Skip whitespace */
    colon++;
    while (*colon == ' ' || *colon == '\t' || *colon == '\n') colon++;

    /* Parse integer */
    char* endptr = NULL;
    long value = strtol(colon, &endptr, DECIMAL_BASE);
    if (endptr == colon) {
        *output = default_value;
        return ARGO_SUCCESS;
    }

    *output = (int)value;
    return ARGO_SUCCESS;
}

/* Create new workflow instance */
workflow_t* workflow_create(const char* workflow_id, const char* workflow_name) {
    if (!workflow_id || !workflow_name) {
        argo_report_error(E_INPUT_NULL, "workflow_create",
                         "NULL workflow_id or workflow_name");
        return NULL;
    }

    workflow_t* workflow = calloc(1, sizeof(workflow_t));
    if (!workflow) {
        argo_report_error(E_SYSTEM_MEMORY, "workflow_create",
                         "Failed to allocate workflow");
        return NULL;
    }

    /* Set identity */
    strncpy(workflow->workflow_id, workflow_id, sizeof(workflow->workflow_id) - 1);
    strncpy(workflow->workflow_name, workflow_name, sizeof(workflow->workflow_name) - 1);

    /* Set defaults */
    workflow->timeout_seconds = DEFAULT_WORKFLOW_TIMEOUT_SECONDS;
    workflow->state = WORKFLOW_STATE_PENDING;
    workflow->checkpoint_enabled = true;

    LOG_DEBUG("Created workflow '%s' (ID: %s)", workflow_name, workflow_id);
    return workflow;
}

/* Load workflow from JSON string */
workflow_t* workflow_load_from_string(const char* json_content, const char* workflow_id) {
    if (!json_content || !workflow_id) {
        argo_report_error(E_INPUT_NULL, "workflow_load_from_string",
                         "NULL json_content or workflow_id");
        return NULL;
    }

    workflow_t* workflow = calloc(1, sizeof(workflow_t));
    if (!workflow) {
        argo_report_error(E_SYSTEM_MEMORY, "workflow_load_from_string",
                         "Failed to allocate workflow");
        return NULL;
    }

    int result = ARGO_SUCCESS;

    /* Set workflow ID */
    strncpy(workflow->workflow_id, workflow_id, sizeof(workflow->workflow_id) - 1);

    /* Extract workflow_name (required) */
    result = extract_string_field(json_content, JSON_FIELD_WORKFLOW_NAME,
                                  workflow->workflow_name, sizeof(workflow->workflow_name));
    if (result != ARGO_SUCCESS || workflow->workflow_name[0] == '\0') {
        argo_report_error(E_INPUT_FORMAT, "workflow_load_from_string",
                         "Missing or invalid 'workflow_name'");
        goto cleanup;
    }

    /* Extract script (required) */
    result = extract_string_field(json_content, JSON_FIELD_SCRIPT,
                                  workflow->script_path, sizeof(workflow->script_path));
    if (result != ARGO_SUCCESS || workflow->script_path[0] == '\0') {
        argo_report_error(E_INPUT_FORMAT, "workflow_load_from_string",
                         "Missing or invalid 'script' field");
        goto cleanup;
    }

    /* Extract description (optional) */
    extract_string_field(json_content, JSON_FIELD_DESCRIPTION,
                        workflow->description, sizeof(workflow->description));

    /* Extract working_dir (optional) */
    extract_string_field(json_content, JSON_FIELD_WORKING_DIR,
                        workflow->working_dir, sizeof(workflow->working_dir));

    /* Extract timeout_seconds (optional) */
    extract_int_field(json_content, JSON_FIELD_TIMEOUT_SECONDS,
                     &workflow->timeout_seconds, DEFAULT_WORKFLOW_TIMEOUT_SECONDS);

    /* Initialize state */
    workflow->state = WORKFLOW_STATE_PENDING;
    workflow->checkpoint_enabled = true;

    LOG_INFO("Loaded workflow '%s' (script: %s, timeout: %ds)",
            workflow->workflow_name, workflow->script_path, workflow->timeout_seconds);

    return workflow;

cleanup:
    workflow_destroy(workflow);
    return NULL;
}

/* Load workflow from JSON file */
workflow_t* workflow_load_from_file(const char* json_path, const char* workflow_id) {
    if (!json_path || !workflow_id) {
        argo_report_error(E_INPUT_NULL, "workflow_load_from_file",
                         "NULL json_path or workflow_id");
        return NULL;
    }

    /* Check file exists and size */
    struct stat st;
    if (stat(json_path, &st) != 0) {
        argo_report_error(E_SYSTEM_FILE, "workflow_load_from_file",
                         "File not found: %s", json_path);
        return NULL;
    }

    if (st.st_size > WORKFLOW_MAX_JSON_SIZE) {
        argo_report_error(E_PROTOCOL_SIZE, "workflow_load_from_file",
                         "File too large: %ld bytes (max: %d)",
                         st.st_size, WORKFLOW_MAX_JSON_SIZE);
        return NULL;
    }

    /* Read file content */
    char* json_content = NULL;
    size_t content_len = 0;
    int result = file_read_all(json_path, &json_content, &content_len);
    if (result != ARGO_SUCCESS || !json_content) {
        argo_report_error(result, "workflow_load_from_file",
                         "Failed to read file: %s", json_path);
        return NULL;
    }

    /* Parse JSON */
    workflow_t* workflow = workflow_load_from_string(json_content, workflow_id);
    free(json_content);

    if (workflow) {
        /* If no working_dir specified, use directory of JSON file */
        if (workflow->working_dir[0] == '\0') {
            const char* last_slash = strrchr(json_path, '/');
            if (last_slash) {
                size_t dir_len = last_slash - json_path;
                if (dir_len < sizeof(workflow->working_dir)) {
                    memcpy(workflow->working_dir, json_path, dir_len);
                    workflow->working_dir[dir_len] = '\0';
                }
            }
        }
    }

    return workflow;
}

/* Destroy workflow and free resources */
void workflow_destroy(workflow_t* workflow) {
    if (!workflow) {
        return;
    }

    LOG_DEBUG("Destroyed workflow '%s' (ID: %s)",
             workflow->workflow_name, workflow->workflow_id);
    free(workflow);
}

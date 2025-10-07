/* © 2025 Casey Koons All rights reserved */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include "argo_workflow_registry.h"
#include "argo_json.h"
#include "argo_workflow_json.h"
#include "argo_error.h"
#include "argo_log.h"
#include "argo_file_utils.h"
#include "argo_limits.h"

/* Forward declaration from jsmn.h */
typedef struct jsmntok {
    int type;
    int start;
    int end;
    int size;
} jsmntok_t;

/* JSMN token types */
#define JSMN_OBJECT (1 << 0)
#define JSMN_ARRAY (1 << 1)

/* Forward declaration of internal functions from core module */
extern workflow_instance_t* workflow_registry_get_workflow(workflow_registry_t* registry,
                                                           const char* workflow_id);

/* Create directory and all parent directories (like mkdir -p) */
static int mkdir_recursive(const char* path) {
    char tmp[WORKFLOW_REGISTRY_PATH_MAX];
    char* p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, ARGO_DIR_PERMISSIONS) != 0 && errno != EEXIST) {
                return E_SYSTEM_FILE;
            }
            *p = '/';
        }
    }

    if (mkdir(tmp, ARGO_DIR_PERMISSIONS) != 0 && errno != EEXIST) {
        return E_SYSTEM_FILE;
    }

    return ARGO_SUCCESS;
}

/* Parse status string to enum */
static workflow_status_t parse_workflow_status(const char* status_str) {
    if (strcmp(status_str, "active") == 0) {
        return WORKFLOW_STATUS_ACTIVE;
    } else if (strcmp(status_str, "suspended") == 0) {
        return WORKFLOW_STATUS_SUSPENDED;
    } else if (strcmp(status_str, "completed") == 0) {
        return WORKFLOW_STATUS_COMPLETED;
    }
    return WORKFLOW_STATUS_ACTIVE;  /* Default to active */
}

/* Parse single workflow entry from JSON tokens */
static int parse_workflow_entry(const char* json, jsmntok_t* tokens,
                                int token_idx, workflow_instance_t* wf) {
    if (!json || !tokens || !wf) {
        return E_INVALID_PARAMS;
    }

    /* Extract id */
    int field_idx = workflow_json_find_field(json, tokens, token_idx, "id");
    if (field_idx >= 0) {
        workflow_json_extract_string(json, &tokens[field_idx],
                                    wf->id, sizeof(wf->id));
    }

    /* Extract template */
    field_idx = workflow_json_find_field(json, tokens, token_idx, "template");
    if (field_idx >= 0) {
        workflow_json_extract_string(json, &tokens[field_idx],
                                    wf->template_name, sizeof(wf->template_name));
    }

    /* Extract instance */
    field_idx = workflow_json_find_field(json, tokens, token_idx, "instance");
    if (field_idx >= 0) {
        workflow_json_extract_string(json, &tokens[field_idx],
                                    wf->instance_name, sizeof(wf->instance_name));
    }

    /* Extract branch */
    field_idx = workflow_json_find_field(json, tokens, token_idx, "branch");
    if (field_idx >= 0) {
        workflow_json_extract_string(json, &tokens[field_idx],
                                    wf->active_branch, sizeof(wf->active_branch));
    }

    /* Extract environment */
    field_idx = workflow_json_find_field(json, tokens, token_idx, "environment");
    if (field_idx >= 0) {
        workflow_json_extract_string(json, &tokens[field_idx],
                                    wf->environment, sizeof(wf->environment));
    } else {
        /* Default to 'dev' if not specified (backward compatibility) */
        strncpy(wf->environment, "dev", sizeof(wf->environment) - 1);
        wf->environment[sizeof(wf->environment) - 1] = '\0';
    }

    /* Extract status */
    field_idx = workflow_json_find_field(json, tokens, token_idx, "status");
    if (field_idx >= 0) {
        char status_str[32];
        workflow_json_extract_string(json, &tokens[field_idx],
                                    status_str, sizeof(status_str));
        wf->status = parse_workflow_status(status_str);
    }

    /* Extract created_at */
    field_idx = workflow_json_find_field(json, tokens, token_idx, "created_at");
    if (field_idx >= 0) {
        int created_at_int;
        if (workflow_json_extract_int(json, &tokens[field_idx], &created_at_int) == ARGO_SUCCESS) {
            wf->created_at = (time_t)created_at_int;
        }
    }

    /* Extract last_active */
    field_idx = workflow_json_find_field(json, tokens, token_idx, "last_active");
    if (field_idx >= 0) {
        int last_active_int;
        if (workflow_json_extract_int(json, &tokens[field_idx], &last_active_int) == ARGO_SUCCESS) {
            wf->last_active = (time_t)last_active_int;
        }
    }

    /* Extract pid */
    field_idx = workflow_json_find_field(json, tokens, token_idx, "pid");
    if (field_idx >= 0) {
        int pid_int;
        if (workflow_json_extract_int(json, &tokens[field_idx], &pid_int) == ARGO_SUCCESS) {
            wf->pid = (pid_t)pid_int;
        }
    } else {
        wf->pid = 0;  /* Default to no process */
    }

    return ARGO_SUCCESS;
}

/* Validate parsed workflow data */
static int validate_workflow_data(const workflow_instance_t* wf) {
    if (!wf) {
        return E_INVALID_PARAMS;
    }

    /* Ensure required fields are present */
    if (wf->id[0] == '\0') {
        LOG_WARN("Workflow missing id field");
        return E_PROTOCOL_FORMAT;
    }

    if (wf->template_name[0] == '\0') {
        LOG_WARN("Workflow %s missing template field", wf->id);
        return E_PROTOCOL_FORMAT;
    }

    return ARGO_SUCCESS;
}

/* Load registry from JSON file */
int workflow_registry_load(workflow_registry_t* registry) {
    if (!registry) {
        return E_INVALID_PARAMS;
    }

    /* Read file contents */
    char* json_content = NULL;
    size_t file_size = 0;
    int result = file_read_all(registry->registry_path, &json_content, &file_size);
    if (result != ARGO_SUCCESS) {
        if (errno == ENOENT) {
            LOG_DEBUG("Registry file not found, starting with empty registry");
            return ARGO_SUCCESS;
        }
        LOG_ERROR("Failed to open registry file: %s", strerror(errno));
        return E_SYSTEM_FILE;
    }

    /* Parse JSON */
    registry->workflow_count = 0;
    jsmntok_t* tokens = malloc(sizeof(jsmntok_t) * WORKFLOW_JSON_MAX_TOKENS);
    if (!tokens) {
        free(json_content);
        return E_SYSTEM_MEMORY;
    }

    int token_count = workflow_json_parse(json_content, tokens, WORKFLOW_JSON_MAX_TOKENS);
    if (token_count < 0) {
        free(tokens);
        free(json_content);
        LOG_ERROR("Failed to parse registry JSON");
        return E_PROTOCOL_FORMAT;
    }

    /* Find workflows array */
    int workflows_idx = workflow_json_find_field(json_content, tokens, 0, "workflows");
    if (workflows_idx < 0) {
        free(tokens);
        free(json_content);
        registry->dirty = false;
        registry->last_saved = time(NULL);
        LOG_DEBUG("No workflows in registry");
        return ARGO_SUCCESS;
    }

    /* Validate workflows array */
    jsmntok_t* workflows_array = &tokens[workflows_idx];
    if (workflows_array->type != JSMN_ARRAY) {
        free(tokens);
        free(json_content);
        LOG_ERROR("workflows field is not an array");
        return E_PROTOCOL_FORMAT;
    }

    /* Parse each workflow */
    int workflow_count = workflows_array->size;
    if (workflow_count > WORKFLOW_REGISTRY_MAX_WORKFLOWS) {
        workflow_count = WORKFLOW_REGISTRY_MAX_WORKFLOWS;
        LOG_WARN("Registry has more workflows than max, truncating to %d", workflow_count);
    }

    int current_token = workflows_idx + 1;
    for (int i = 0; i < workflow_count && current_token < token_count; i++) {
        if (tokens[current_token].type != JSMN_OBJECT) {
            current_token += workflow_json_count_tokens(tokens, current_token);
            continue;
        }

        workflow_instance_t* wf = &registry->workflows[registry->workflow_count];
        result = parse_workflow_entry(json_content, tokens, current_token, wf);
        if (result == ARGO_SUCCESS) {
            result = validate_workflow_data(wf);
            if (result == ARGO_SUCCESS) {
                registry->workflow_count++;
            }
        }

        current_token += workflow_json_count_tokens(tokens, current_token);
    }

    free(tokens);
    free(json_content);

    registry->dirty = false;
    registry->last_saved = time(NULL);

    LOG_DEBUG("Loaded %d workflows from registry", registry->workflow_count);
    return ARGO_SUCCESS;
}

/* Save registry to JSON file */
int workflow_registry_save(workflow_registry_t* registry) {
    if (!registry) {
        return E_INVALID_PARAMS;
    }

    if (!registry->dirty) {
        /* No changes to save */
        return ARGO_SUCCESS;
    }

    /* Create directory if needed */
    char dir_path[WORKFLOW_REGISTRY_PATH_MAX];
    strncpy(dir_path, registry->registry_path, sizeof(dir_path) - 1);

    char* last_slash = strrchr(dir_path, '/');
    if (last_slash) {
        *last_slash = '\0';
        int result = mkdir_recursive(dir_path);
        if (result != ARGO_SUCCESS) {
            LOG_ERROR("Failed to create registry directory: %s", dir_path);
            return result;
        }
    }

    /* Open file for writing */
    FILE* file = fopen(registry->registry_path, "w");
    if (!file) {
        LOG_ERROR("Failed to open registry file for writing: %s", strerror(errno));
        return E_SYSTEM_FILE;
    }

    /* Write JSON header */
    fprintf(file, "{\n");
    fprintf(file, "  \"workflows\": [\n");

    /* Write workflows */
    for (int i = 0; i < registry->workflow_count; i++) {
        workflow_instance_t* wf = &registry->workflows[i];

        fprintf(file, "    {\n");
        fprintf(file, "      \"id\": \"%s\",\n", wf->id);
        fprintf(file, "      \"template\": \"%s\",\n", wf->template_name);
        fprintf(file, "      \"instance\": \"%s\",\n", wf->instance_name);
        fprintf(file, "      \"branch\": \"%s\",\n", wf->active_branch);
        fprintf(file, "      \"environment\": \"%s\",\n", wf->environment);
        fprintf(file, "      \"status\": \"%s\",\n",
                workflow_status_string(wf->status));
        fprintf(file, "      \"created_at\": %ld,\n", (long)wf->created_at);
        fprintf(file, "      \"last_active\": %ld,\n", (long)wf->last_active);
        fprintf(file, "      \"pid\": %d\n", (int)wf->pid);
        fprintf(file, "    }%s\n", (i < registry->workflow_count - 1) ? "," : "");
    }

    fprintf(file, "  ],\n");
    fprintf(file, "  \"last_updated\": %ld\n", (long)time(NULL));
    fprintf(file, "}\n");

    fclose(file);

    registry->dirty = false;
    registry->last_saved = time(NULL);

    LOG_DEBUG("Saved %d workflows to registry", registry->workflow_count);
    return ARGO_SUCCESS;
}

/* Schedule batched save (called after modifications) */
int workflow_registry_schedule_save(workflow_registry_t* registry) {
    if (!registry) {
        return E_INVALID_PARAMS;
    }

    registry->dirty = true;
    registry->last_modified = time(NULL);

    /* Future Enhancement: Could integrate with shared services for batched writes
     * to reduce disk I/O. Current immediate save approach ensures data integrity
     * and is appropriate for workflow state management. */
    return workflow_registry_save(registry);
}

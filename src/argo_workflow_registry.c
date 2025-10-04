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

/* Create workflow registry */
workflow_registry_t* workflow_registry_create(const char* registry_path) {
    if (!registry_path) {
        return NULL;
    }

    workflow_registry_t* registry = calloc(1, sizeof(workflow_registry_t));
    if (!registry) {
        LOG_ERROR("Failed to allocate workflow registry");
        return NULL;
    }

    strncpy(registry->registry_path, registry_path,
            sizeof(registry->registry_path) - 1);

    registry->workflow_count = 0;
    registry->last_saved = 0;
    registry->last_modified = 0;
    registry->dirty = false;
    registry->shared_services = NULL;

    LOG_DEBUG("Created workflow registry at %s", registry_path);
    return registry;
}

/* Destroy workflow registry */
void workflow_registry_destroy(workflow_registry_t* registry) {
    if (!registry) {
        return;
    }

    /* Save any pending changes */
    if (registry->dirty) {
        workflow_registry_save(registry);
    }

    free(registry);
}

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
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return E_SYSTEM_FILE;
            }
            *p = '/';
        }
    }

    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
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

/* Load registry from JSON file */
int workflow_registry_load(workflow_registry_t* registry) {
    if (!registry) {
        return E_INVALID_PARAMS;
    }

    FILE* file = fopen(registry->registry_path, "r");
    if (!file) {
        if (errno == ENOENT) {
            /* File doesn't exist yet - not an error */
            LOG_DEBUG("Registry file not found, starting with empty registry");
            return ARGO_SUCCESS;
        }
        LOG_ERROR("Failed to open registry file: %s", strerror(errno));
        return E_SYSTEM_FILE;
    }

    /* Read file contents */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* json_content = malloc(file_size + 1);
    if (!json_content) {
        fclose(file);
        return E_SYSTEM_MEMORY;
    }

    size_t read_size = fread(json_content, 1, file_size, file);
    json_content[read_size] = '\0';
    fclose(file);

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
        /* No workflows in registry yet */
        free(tokens);
        free(json_content);
        registry->dirty = false;
        registry->last_saved = time(NULL);
        LOG_DEBUG("No workflows in registry");
        return ARGO_SUCCESS;
    }

    /* Workflows array token */
    jsmntok_t* workflows_array = &tokens[workflows_idx];
    if (workflows_array->type != JSMN_ARRAY) {
        free(tokens);
        free(json_content);
        LOG_ERROR("workflows field is not an array");
        return E_PROTOCOL_FORMAT;
    }

    int workflow_count = workflows_array->size;
    if (workflow_count > WORKFLOW_REGISTRY_MAX_WORKFLOWS) {
        workflow_count = WORKFLOW_REGISTRY_MAX_WORKFLOWS;
        LOG_WARN("Registry has more workflows than max, truncating to %d", workflow_count);
    }

    /* Parse each workflow object */
    int current_token = workflows_idx + 1;  /* First element after array token */
    for (int i = 0; i < workflow_count; i++) {
        if (current_token >= token_count) {
            break;
        }

        jsmntok_t* wf_obj = &tokens[current_token];
        if (wf_obj->type != JSMN_OBJECT) {
            /* Skip this token and continue */
            current_token += workflow_json_count_tokens(tokens, current_token);
            continue;
        }

        workflow_instance_t* wf = &registry->workflows[registry->workflow_count];

        /* Extract fields */
        int field_idx;

        /* id */
        field_idx = workflow_json_find_field(json_content, tokens, current_token, "id");
        if (field_idx >= 0) {
            workflow_json_extract_string(json_content, &tokens[field_idx],
                                        wf->id, sizeof(wf->id));
        }

        /* template */
        field_idx = workflow_json_find_field(json_content, tokens, current_token, "template");
        if (field_idx >= 0) {
            workflow_json_extract_string(json_content, &tokens[field_idx],
                                        wf->template_name, sizeof(wf->template_name));
        }

        /* instance */
        field_idx = workflow_json_find_field(json_content, tokens, current_token, "instance");
        if (field_idx >= 0) {
            workflow_json_extract_string(json_content, &tokens[field_idx],
                                        wf->instance_name, sizeof(wf->instance_name));
        }

        /* branch */
        field_idx = workflow_json_find_field(json_content, tokens, current_token, "branch");
        if (field_idx >= 0) {
            workflow_json_extract_string(json_content, &tokens[field_idx],
                                        wf->active_branch, sizeof(wf->active_branch));
        }

        /* status */
        field_idx = workflow_json_find_field(json_content, tokens, current_token, "status");
        if (field_idx >= 0) {
            char status_str[32];
            workflow_json_extract_string(json_content, &tokens[field_idx],
                                        status_str, sizeof(status_str));
            wf->status = parse_workflow_status(status_str);
        }

        /* created_at */
        field_idx = workflow_json_find_field(json_content, tokens, current_token, "created_at");
        if (field_idx >= 0) {
            int created_at_int;
            if (workflow_json_extract_int(json_content, &tokens[field_idx], &created_at_int) == ARGO_SUCCESS) {
                wf->created_at = (time_t)created_at_int;
            }
        }

        /* last_active */
        field_idx = workflow_json_find_field(json_content, tokens, current_token, "last_active");
        if (field_idx >= 0) {
            int last_active_int;
            if (workflow_json_extract_int(json_content, &tokens[field_idx], &last_active_int) == ARGO_SUCCESS) {
                wf->last_active = (time_t)last_active_int;
            }
        }

        /* pid */
        field_idx = workflow_json_find_field(json_content, tokens, current_token, "pid");
        if (field_idx >= 0) {
            int pid_int;
            if (workflow_json_extract_int(json_content, &tokens[field_idx], &pid_int) == ARGO_SUCCESS) {
                wf->pid = (pid_t)pid_int;
            }
        } else {
            wf->pid = 0;  /* Default to no process */
        }

        registry->workflow_count++;

        /* Move to next workflow object */
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

    /* TODO: Integrate with shared services for batched writes */
    /* For now, save immediately */
    return workflow_registry_save(registry);
}

/* Add new workflow instance */
int workflow_registry_add_workflow(workflow_registry_t* registry,
                                   const char* template_name,
                                   const char* instance_name,
                                   const char* initial_branch) {
    if (!registry || !template_name || !instance_name) {
        return E_INVALID_PARAMS;
    }

    if (registry->workflow_count >= WORKFLOW_REGISTRY_MAX_WORKFLOWS) {
        return E_RESOURCE_LIMIT;
    }

    /* Create workflow ID: template_instance */
    char workflow_id[WORKFLOW_REGISTRY_ID_MAX];
    snprintf(workflow_id, sizeof(workflow_id), "%s_%s",
             template_name, instance_name);

    /* Check for duplicates */
    for (int i = 0; i < registry->workflow_count; i++) {
        if (strcmp(registry->workflows[i].id, workflow_id) == 0) {
            return E_DUPLICATE;
        }
    }

    /* Add new workflow */
    workflow_instance_t* wf = &registry->workflows[registry->workflow_count];

    strncpy(wf->id, workflow_id, sizeof(wf->id) - 1);
    strncpy(wf->template_name, template_name, sizeof(wf->template_name) - 1);
    strncpy(wf->instance_name, instance_name, sizeof(wf->instance_name) - 1);
    strncpy(wf->active_branch, initial_branch ? initial_branch : "main",
            sizeof(wf->active_branch) - 1);

    wf->status = WORKFLOW_STATUS_ACTIVE;
    wf->created_at = time(NULL);
    wf->last_active = wf->created_at;
    wf->pid = 0;  /* No process yet */

    registry->workflow_count++;

    LOG_INFO("Added workflow: %s (template=%s, instance=%s, branch=%s)",
             wf->id, wf->template_name, wf->instance_name, wf->active_branch);

    return workflow_registry_schedule_save(registry);
}

/* Remove workflow */
int workflow_registry_remove_workflow(workflow_registry_t* registry,
                                      const char* workflow_id) {
    if (!registry || !workflow_id) {
        return E_INVALID_PARAMS;
    }

    /* Find workflow */
    int found = -1;
    for (int i = 0; i < registry->workflow_count; i++) {
        if (strcmp(registry->workflows[i].id, workflow_id) == 0) {
            found = i;
            break;
        }
    }

    if (found == -1) {
        return E_NOT_FOUND;
    }

    /* Shift remaining workflows down */
    for (int i = found; i < registry->workflow_count - 1; i++) {
        registry->workflows[i] = registry->workflows[i + 1];
    }

    registry->workflow_count--;

    LOG_INFO("Removed workflow: %s", workflow_id);
    return workflow_registry_schedule_save(registry);
}

/* Get workflow by ID */
workflow_instance_t* workflow_registry_get_workflow(workflow_registry_t* registry,
                                                    const char* workflow_id) {
    if (!registry || !workflow_id) {
        return NULL;
    }

    for (int i = 0; i < registry->workflow_count; i++) {
        if (strcmp(registry->workflows[i].id, workflow_id) == 0) {
            return &registry->workflows[i];
        }
    }

    return NULL;
}

/* Update active branch */
int workflow_registry_update_branch(workflow_registry_t* registry,
                                    const char* workflow_id,
                                    const char* branch_name) {
    if (!registry || !workflow_id || !branch_name) {
        return E_INVALID_PARAMS;
    }

    workflow_instance_t* wf = workflow_registry_get_workflow(registry, workflow_id);
    if (!wf) {
        return E_NOT_FOUND;
    }

    strncpy(wf->active_branch, branch_name, sizeof(wf->active_branch) - 1);
    wf->last_active = time(NULL);

    LOG_DEBUG("Updated workflow %s branch to %s", workflow_id, branch_name);
    return workflow_registry_schedule_save(registry);
}

/* Set workflow status */
int workflow_registry_set_status(workflow_registry_t* registry,
                                 const char* workflow_id,
                                 workflow_status_t status) {
    if (!registry || !workflow_id) {
        return E_INVALID_PARAMS;
    }

    workflow_instance_t* wf = workflow_registry_get_workflow(registry, workflow_id);
    if (!wf) {
        return E_NOT_FOUND;
    }

    wf->status = status;
    wf->last_active = time(NULL);

    LOG_DEBUG("Updated workflow %s status to %s",
              workflow_id, workflow_status_string(status));
    return workflow_registry_schedule_save(registry);
}

/* List all workflows */
int workflow_registry_list(workflow_registry_t* registry,
                          workflow_instance_t** workflows,
                          int* count) {
    if (!registry || !workflows || !count) {
        return E_INVALID_PARAMS;
    }

    *workflows = registry->workflows;
    *count = registry->workflow_count;

    return ARGO_SUCCESS;
}

/* Get active workflow from environment */
workflow_instance_t* workflow_registry_get_active(workflow_registry_t* registry) {
    if (!registry) {
        return NULL;
    }

    const char* active_id = getenv("ARGO_ACTIVE_WORKFLOW");
    if (!active_id) {
        return NULL;
    }

    return workflow_registry_get_workflow(registry, active_id);
}

/* Get workflow count */
int workflow_registry_count(workflow_registry_t* registry) {
    if (!registry) {
        return 0;
    }
    return registry->workflow_count;
}

/* Convert status to string */
const char* workflow_status_string(workflow_status_t status) {
    switch (status) {
        case WORKFLOW_STATUS_ACTIVE:    return "active";
        case WORKFLOW_STATUS_SUSPENDED: return "suspended";
        case WORKFLOW_STATUS_COMPLETED: return "completed";
        default:                        return "unknown";
    }
}

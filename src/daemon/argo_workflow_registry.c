/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Project includes */
#include "argo_workflow_registry.h"
#include "argo_error.h"
#include "argo_log.h"
#include "argo_json.h"
#include "argo_limits.h"

/* Registry entry node */
typedef struct registry_node {
    workflow_entry_t entry;
    struct registry_node* next;
} registry_node_t;

/* Registry structure */
struct workflow_registry {
    registry_node_t* head;
    int count;
};

/* Create workflow registry */
workflow_registry_t* workflow_registry_create(void) {
    workflow_registry_t* reg = calloc(1, sizeof(workflow_registry_t));
    if (!reg) {
        argo_report_error(E_SYSTEM_MEMORY, "workflow_registry_create",
                         "allocation failed");
        return NULL;
    }

    reg->head = NULL;
    reg->count = 0;

    LOG_DEBUG("Created workflow registry");
    return reg;
}

/* Find node by ID */
static registry_node_t* find_node(const workflow_registry_t* reg, const char* id) {
    if (!reg || !id) return NULL;

    registry_node_t* node = reg->head;
    while (node) {
        if (strcmp(node->entry.workflow_id, id) == 0) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

/* Add workflow to registry */
int workflow_registry_add(workflow_registry_t* reg, const workflow_entry_t* entry) {
    if (!reg || !entry) {
        return E_INPUT_NULL;
    }

    /* Check if already exists */
    if (find_node(reg, entry->workflow_id)) {
        argo_report_error(E_DUPLICATE, "workflow_registry_add",
                         entry->workflow_id);
        return E_DUPLICATE;
    }

    /* Create new node */
    registry_node_t* node = calloc(1, sizeof(registry_node_t));
    if (!node) {
        argo_report_error(E_SYSTEM_MEMORY, "workflow_registry_add",
                         "allocation failed");
        return E_SYSTEM_MEMORY;
    }

    /* Copy entry */
    memcpy(&node->entry, entry, sizeof(workflow_entry_t));

    /* Add to head of list */
    node->next = reg->head;
    reg->head = node;
    reg->count++;

    LOG_DEBUG("Added workflow: %s (state=%d)", entry->workflow_id, entry->state);
    return ARGO_SUCCESS;
}

/* Update workflow state */
int workflow_registry_update_state(workflow_registry_t* reg, const char* id,
                                    workflow_state_t state) {
    if (!reg || !id) {
        return E_INPUT_NULL;
    }

    registry_node_t* node = find_node(reg, id);
    if (!node) {
        argo_report_error(E_NOT_FOUND, "workflow_registry_update_state", id);
        return E_NOT_FOUND;
    }

    node->entry.state = state;

    /* Set end_time for terminal states */
    if (state == WORKFLOW_STATE_COMPLETED ||
        state == WORKFLOW_STATE_FAILED ||
        state == WORKFLOW_STATE_ABANDONED) {
        if (node->entry.end_time == 0) {
            node->entry.end_time = time(NULL);
        }
    }

    LOG_DEBUG("Updated workflow %s state: %d", id, state);
    return ARGO_SUCCESS;
}

/* Update workflow progress */
int workflow_registry_update_progress(workflow_registry_t* reg, const char* id,
                                       int current_step) {
    if (!reg || !id) {
        return E_INPUT_NULL;
    }

    registry_node_t* node = find_node(reg, id);
    if (!node) {
        argo_report_error(E_NOT_FOUND, "workflow_registry_update_progress", id);
        return E_NOT_FOUND;
    }

    node->entry.current_step = current_step;

    LOG_DEBUG("Updated workflow %s progress: %d/%d", id,
              current_step, node->entry.total_steps);
    return ARGO_SUCCESS;
}

/* Remove workflow from registry */
int workflow_registry_remove(workflow_registry_t* reg, const char* id) {
    if (!reg || !id) {
        return E_INPUT_NULL;
    }

    registry_node_t* prev = NULL;
    registry_node_t* node = reg->head;

    while (node) {
        if (strcmp(node->entry.workflow_id, id) == 0) {
            /* Found - remove from list */
            if (prev) {
                prev->next = node->next;
            } else {
                reg->head = node->next;
            }

            LOG_DEBUG("Removed workflow: %s", id);
            free(node);
            reg->count--;
            return ARGO_SUCCESS;
        }

        prev = node;
        node = node->next;
    }

    argo_report_error(E_NOT_FOUND, "workflow_registry_remove", id);
    return E_NOT_FOUND;
}

/* Find workflow by ID */
const workflow_entry_t* workflow_registry_find(const workflow_registry_t* reg,
                                                 const char* id) {
    if (!reg || !id) return NULL;

    registry_node_t* node = find_node(reg, id);
    return node ? &node->entry : NULL;
}

/* List all workflows */
int workflow_registry_list(const workflow_registry_t* reg,
                            workflow_entry_t** entries, int* count) {
    if (!reg || !entries || !count) {
        return E_INPUT_NULL;
    }

    *entries = NULL;
    *count = 0;

    if (reg->count == 0) {
        return ARGO_SUCCESS;
    }

    /* Allocate array */
    workflow_entry_t* arr = calloc(reg->count, sizeof(workflow_entry_t));
    if (!arr) {
        argo_report_error(E_SYSTEM_MEMORY, "workflow_registry_list",
                         "allocation failed");
        return E_SYSTEM_MEMORY;
    }

    /* Copy entries */
    int index = 0;
    registry_node_t* node = reg->head;
    while (node) {
        memcpy(&arr[index++], &node->entry, sizeof(workflow_entry_t));
        node = node->next;
    }

    *entries = arr;
    *count = reg->count;

    return ARGO_SUCCESS;
}

/* Count workflows by state */
int workflow_registry_count(const workflow_registry_t* reg, workflow_state_t state) {
    if (!reg) return 0;

    if (state == (workflow_state_t)-1) {
        return reg->count;  /* All workflows */
    }

    int count = 0;
    registry_node_t* node = reg->head;
    while (node) {
        if (node->entry.state == state) {
            count++;
        }
        node = node->next;
    }

    return count;
}

/* Convert state to string */
const char* workflow_state_to_string(workflow_state_t state) {
    switch (state) {
        case WORKFLOW_STATE_PENDING:    return "pending";
        case WORKFLOW_STATE_RUNNING:    return "running";
        case WORKFLOW_STATE_COMPLETED:  return "completed";
        case WORKFLOW_STATE_FAILED:     return "failed";
        case WORKFLOW_STATE_ABANDONED:  return "abandoned";
        default:                        return "unknown";
    }
}

/* Convert string to state */
workflow_state_t workflow_state_from_string(const char* str) {
    if (!str) return WORKFLOW_STATE_PENDING;

    if (strcmp(str, "pending") == 0)    return WORKFLOW_STATE_PENDING;
    if (strcmp(str, "running") == 0)    return WORKFLOW_STATE_RUNNING;
    if (strcmp(str, "completed") == 0)  return WORKFLOW_STATE_COMPLETED;
    if (strcmp(str, "failed") == 0)     return WORKFLOW_STATE_FAILED;
    if (strcmp(str, "abandoned") == 0)  return WORKFLOW_STATE_ABANDONED;

    return WORKFLOW_STATE_PENDING;
}

/* Save registry to JSON file */
int workflow_registry_save(const workflow_registry_t* reg, const char* path) {
    if (!reg || !path) {
        return E_INPUT_NULL;
    }

    FILE* fp = fopen(path, "w");
    if (!fp) {
        argo_report_error(E_SYSTEM_FILE, "workflow_registry_save", path);
        return E_SYSTEM_FILE;
    }

    /* Write JSON header */
    fprintf(fp, "{\n");
    fprintf(fp, "  \"version\": 1,\n");
    fprintf(fp, "  \"workflows\": [\n");

    /* Write workflows */
    int first = 1;
    registry_node_t* node = reg->head;
    while (node) {
        if (!first) fprintf(fp, ",\n");
        first = 0;

        fprintf(fp, "    {\n");
        fprintf(fp, "      \"workflow_id\": \"%s\",\n", node->entry.workflow_id);
        fprintf(fp, "      \"workflow_name\": \"%s\",\n", node->entry.workflow_name);
        fprintf(fp, "      \"state\": \"%s\",\n",
                workflow_state_to_string(node->entry.state));
        fprintf(fp, "      \"executor_pid\": %d,\n", node->entry.executor_pid);
        fprintf(fp, "      \"start_time\": %ld,\n", (long)node->entry.start_time);
        fprintf(fp, "      \"end_time\": %ld,\n", (long)node->entry.end_time);
        fprintf(fp, "      \"exit_code\": %d,\n", node->entry.exit_code);
        fprintf(fp, "      \"current_step\": %d,\n", node->entry.current_step);
        fprintf(fp, "      \"total_steps\": %d,\n", node->entry.total_steps);
        fprintf(fp, "      \"timeout_seconds\": %d,\n", node->entry.timeout_seconds);
        fprintf(fp, "      \"retry_count\": %d,\n", node->entry.retry_count);
        fprintf(fp, "      \"max_retries\": %d,\n", node->entry.max_retries);
        fprintf(fp, "      \"last_retry_time\": %ld\n", (long)node->entry.last_retry_time);
        fprintf(fp, "    }");

        node = node->next;
    }

    fprintf(fp, "\n  ]\n");
    fprintf(fp, "}\n");

    fclose(fp);

    LOG_DEBUG("Saved %d workflows to %s", reg->count, path);
    return ARGO_SUCCESS;
}

/* Load registry from JSON file */
int workflow_registry_load(workflow_registry_t* reg, const char* path) {
    if (!reg || !path) {
        return E_INPUT_NULL;
    }

    FILE* fp = fopen(path, "r");
    if (!fp) {
        /* Not an error - file may not exist yet */
        LOG_DEBUG("No registry file to load: %s", path);
        return ARGO_SUCCESS;
    }

    /* Read entire file */
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* json = malloc(size + 1);
    if (!json) {
        fclose(fp);
        argo_report_error(E_SYSTEM_MEMORY, "workflow_registry_load",
                         "allocation failed");
        return E_SYSTEM_MEMORY;
    }

    size_t read = fread(json, 1, size, fp);
    if (read != (size_t)size && ferror(fp)) {
        argo_report_error(E_SYSTEM_IO, "workflow_registry_load",
                         "Failed to read %ld bytes from %s (read %zu)", size, path, read);
        free(json);
        fclose(fp);
        return E_SYSTEM_IO;
    }
    json[read] = '\0';
    fclose(fp);

    /* Parse JSON (simplified - just extract workflow entries) */
    /* Note: Full JSON parsing would use jsmn or similar */
    /* For now, this is a placeholder that loads minimal data */

    int loaded = 0;
    LOG_INFO("Loaded %d workflows from %s", loaded, path);

    free(json);
    return ARGO_SUCCESS;
}

/* Prune old workflows */
int workflow_registry_prune(workflow_registry_t* reg, time_t older_than) {
    if (!reg) return -1;

    int pruned = 0;
    registry_node_t* prev = NULL;
    registry_node_t* node = reg->head;

    while (node) {
        registry_node_t* next = node->next;

        /* Only prune terminal states */
        bool is_terminal = (node->entry.state == WORKFLOW_STATE_COMPLETED ||
                           node->entry.state == WORKFLOW_STATE_FAILED ||
                           node->entry.state == WORKFLOW_STATE_ABANDONED);

        if (is_terminal && node->entry.end_time > 0 &&
            node->entry.end_time < older_than) {
            /* Remove this node */
            if (prev) {
                prev->next = next;
            } else {
                reg->head = next;
            }

            LOG_DEBUG("Pruned workflow: %s", node->entry.workflow_id);
            free(node);
            reg->count--;
            pruned++;
        } else {
            prev = node;
        }

        node = next;
    }

    if (pruned > 0) {
        LOG_INFO("Pruned %d old workflows", pruned);
    }

    return pruned;
}

/* Destroy registry */
void workflow_registry_destroy(workflow_registry_t* reg) {
    if (!reg) return;

    registry_node_t* node = reg->head;
    while (node) {
        registry_node_t* next = node->next;
        free(node);
        node = next;
    }

    free(reg);
    LOG_DEBUG("Destroyed workflow registry");
}

/* © 2025 Casey Koons All rights reserved */

/* Workflow checkpoint - save/restore workflow state */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Project includes */
#include "argo_workflow_checkpoint.h"
#include "argo_error.h"
#include "argo_error_messages.h"
#include "argo_log.h"
#include "argo_json.h"
#include "argo_file_utils.h"

/* Helper: Extract integer field from JSON */
static int extract_int_field(const char* json, const char* field_name, int* out_value) {
    char search_pattern[CHECKPOINT_PATTERN_SIZE];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\": %%d", field_name);

    const char* field = strstr(json, field_name);
    if (!field) return E_PROTOCOL_FORMAT;

    /* Move past the field name and look for the value */
    const char* colon = strchr(field, ':');
    if (!colon) return E_PROTOCOL_FORMAT;

    if (sscanf(colon + 1, "%d", out_value) != 1) {
        return E_PROTOCOL_FORMAT;
    }

    return ARGO_SUCCESS;
}

/* Helper: Extract long field from JSON */
static int extract_long_field(const char* json, const char* field_name, long* out_value) {
    const char* field = strstr(json, field_name);
    if (!field) return E_PROTOCOL_FORMAT;

    const char* colon = strchr(field, ':');
    if (!colon) return E_PROTOCOL_FORMAT;

    if (sscanf(colon + 1, "%ld", out_value) != 1) {
        return E_PROTOCOL_FORMAT;
    }

    return ARGO_SUCCESS;
}

/* Save workflow state to JSON checkpoint */
char* workflow_save_checkpoint(workflow_controller_t* workflow) {
    if (!workflow) return NULL;

    /* Allocate buffer for JSON */
    size_t capacity = CHECKPOINT_INITIAL_CAPACITY;
    char* json = malloc(capacity);
    if (!json) return NULL;

    int pos = 0;
    pos += snprintf(json + pos, capacity - pos, "{\n");
    pos += snprintf(json + pos, capacity - pos, "  \"workflow_id\": \"%s\",\n",
                   workflow->workflow_id);
    pos += snprintf(json + pos, capacity - pos, "  \"current_phase\": %d,\n",
                   workflow->current_phase);
    pos += snprintf(json + pos, capacity - pos, "  \"state\": %d,\n",
                   workflow->state);
    pos += snprintf(json + pos, capacity - pos, "  \"base_branch\": \"%s\",\n",
                   workflow->base_branch);
    pos += snprintf(json + pos, capacity - pos, "  \"feature_branch\": \"%s\",\n",
                   workflow->feature_branch);
    pos += snprintf(json + pos, capacity - pos, "  \"total_tasks\": %d,\n",
                   workflow->total_tasks);
    pos += snprintf(json + pos, capacity - pos, "  \"completed_tasks\": %d,\n",
                   workflow->completed_tasks);
    pos += snprintf(json + pos, capacity - pos, "  \"phase_start_time\": %ld,\n",
                   (long)workflow->phase_start_time);
    pos += snprintf(json + pos, capacity - pos, "  \"workflow_start_time\": %ld,\n",
                   (long)workflow->workflow_start_time);

    /* Save tasks */
    pos += snprintf(json + pos, capacity - pos, "  \"tasks\": [\n");
    ci_task_t* task = workflow->tasks;
    int first = 1;
    while (task) {
        if (!first) {
            pos += snprintf(json + pos, capacity - pos, ",\n");
        }
        pos += snprintf(json + pos, capacity - pos, "    {\n");
        pos += snprintf(json + pos, capacity - pos, "      \"id\": \"%s\",\n", task->id);
        pos += snprintf(json + pos, capacity - pos, "      \"description\": \"%s\",\n",
                       task->description);
        pos += snprintf(json + pos, capacity - pos, "      \"assigned_to\": \"%s\",\n",
                       task->assigned_to);
        pos += snprintf(json + pos, capacity - pos, "      \"phase\": %d,\n", task->phase);
        pos += snprintf(json + pos, capacity - pos, "      \"completed\": %d\n", task->completed);
        pos += snprintf(json + pos, capacity - pos, "    }");
        first = 0;
        task = task->next;
    }
    pos += snprintf(json + pos, capacity - pos, "\n  ]\n");
    snprintf(json + pos, capacity - pos, "}\n");

    LOG_INFO("Saved checkpoint for workflow %s", workflow->workflow_id);
    return json;
}

/* Restore workflow state from JSON checkpoint */
int workflow_restore_checkpoint(workflow_controller_t* workflow,
                                const char* checkpoint_json) {
    if (!workflow || !checkpoint_json) {
        return E_INPUT_NULL;
    }

    int result;
    int int_val;
    long long_val;
    char* str_val = NULL;
    size_t str_len = 0;

    /* Extract workflow_id */
    result = json_extract_string_field(checkpoint_json, "workflow_id", &str_val, &str_len);
    if (result == ARGO_SUCCESS) {
        strncpy(workflow->workflow_id, str_val, sizeof(workflow->workflow_id) - 1);
        free(str_val);
        str_val = NULL;
    }

    /* Extract current_phase */
    if (extract_int_field(checkpoint_json, "current_phase", &int_val) == ARGO_SUCCESS) {
        workflow->current_phase = (workflow_phase_t)int_val;
    }

    /* Extract state */
    if (extract_int_field(checkpoint_json, "state", &int_val) == ARGO_SUCCESS) {
        workflow->state = (workflow_state_t)int_val;
    }

    /* Extract base_branch */
    result = json_extract_string_field(checkpoint_json, "base_branch", &str_val, &str_len);
    if (result == ARGO_SUCCESS) {
        strncpy(workflow->base_branch, str_val, sizeof(workflow->base_branch) - 1);
        free(str_val);
        str_val = NULL;
    }

    /* Extract feature_branch */
    result = json_extract_string_field(checkpoint_json, "feature_branch", &str_val, &str_len);
    if (result == ARGO_SUCCESS) {
        strncpy(workflow->feature_branch, str_val, sizeof(workflow->feature_branch) - 1);
        free(str_val);
        str_val = NULL;
    }

    /* Extract task counts */
    if (extract_int_field(checkpoint_json, "total_tasks", &int_val) == ARGO_SUCCESS) {
        workflow->total_tasks = int_val;
    }
    if (extract_int_field(checkpoint_json, "completed_tasks", &int_val) == ARGO_SUCCESS) {
        workflow->completed_tasks = int_val;
    }

    /* Extract timestamps */
    if (extract_long_field(checkpoint_json, "phase_start_time", &long_val) == ARGO_SUCCESS) {
        workflow->phase_start_time = (time_t)long_val;
    }
    if (extract_long_field(checkpoint_json, "workflow_start_time", &long_val) == ARGO_SUCCESS) {
        workflow->workflow_start_time = (time_t)long_val;
    }

    /* Note: Task array restoration not implemented yet - would require
     * parsing the tasks array and recreating task linked list.
     * For now, tasks are preserved from the existing workflow structure. */

    LOG_INFO("Restored checkpoint for workflow %s", workflow->workflow_id);
    return ARGO_SUCCESS;
}

/* Save checkpoint to file */
int workflow_checkpoint_to_file(workflow_controller_t* workflow,
                                const char* filepath) {
    if (!workflow || !filepath) {
        return E_INPUT_NULL;
    }

    char* json = workflow_save_checkpoint(workflow);
    if (!json) {
        return E_SYSTEM_MEMORY;
    }

    FILE* fp = fopen(filepath, "w");
    if (!fp) {
        free(json);
        argo_report_error(E_SYSTEM_FILE, "workflow_checkpoint_to_file",
                         ERR_MSG_CHECKPOINT_FAILED);
        return E_SYSTEM_FILE;
    }

    fprintf(fp, "%s", json);
    fclose(fp);
    free(json);

    LOG_INFO("Saved workflow checkpoint to %s", filepath);
    return ARGO_SUCCESS;
}

/* Restore workflow from checkpoint file */
workflow_controller_t* workflow_restore_from_file(ci_registry_t* registry,
                                                  lifecycle_manager_t* lifecycle,
                                                  const char* filepath) {
    if (!registry || !lifecycle || !filepath) {
        return NULL;
    }

    /* Read file */
    char* json = NULL;
    size_t file_size = 0;
    int result = file_read_all(filepath, &json, &file_size);
    if (result != ARGO_SUCCESS) {
        argo_report_error(E_SYSTEM_FILE, "workflow_restore_from_file",
                         ERR_MSG_RESTORE_FAILED);
        return NULL;
    }

    /* Create new workflow */
    workflow_controller_t* workflow = workflow_create(registry, lifecycle, "restored");
    if (workflow) {
        workflow_restore_checkpoint(workflow, json);
    }

    free(json);

    LOG_INFO("Restored workflow from checkpoint file %s", filepath);
    return workflow;
}

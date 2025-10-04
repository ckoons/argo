/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Project includes */
#include "argo_workflow_loader.h"
#include "argo_orchestrator.h"
#include "argo_error.h"
#include "argo_error_messages.h"
#include "argo_log.h"
#include "argo_file_utils.h"

/* Load workflow definition from file */
workflow_definition_t* workflow_load_from_file(const char* path) {
    if (!path) {
        argo_report_error(E_INPUT_NULL, "workflow_load_from_file", WORKFLOW_ERR_PATH_NULL);
        return NULL;
    }

    /* Read entire file */
    char* json = NULL;
    size_t file_size = 0;
    int result = file_read_all(path, &json, &file_size);
    if (result != ARGO_SUCCESS) {
        argo_report_error(E_SYSTEM_FILE, "workflow_load_from_file",
                         ERR_FMT_FAILED_TO_OPEN, path);
        return NULL;
    }

    if (file_size == 0) {
        free(json);
        argo_report_error(E_SYSTEM_FILE, "workflow_load_from_file", WORKFLOW_ERR_FILE_EMPTY);
        return NULL;
    }

    /* Parse JSON */
    workflow_definition_t* def = workflow_definition_from_json(json);
    free(json);

    if (def) {
        LOG_INFO("Loaded workflow definition from %s", path);
    } else {
        argo_report_error(E_PROTOCOL_FORMAT, "workflow_load_from_file",
                         WORKFLOW_ERR_JSON_PARSE_FAILED);
    }

    return def;
}

/* Load workflow by name from standard location */
workflow_definition_t* workflow_load_by_name(const char* category,
                                             const char* event,
                                             const char* name) {
    if (!category || !event || !name) {
        argo_report_error(E_INPUT_NULL, "workflow_load_by_name",
                         WORKFLOW_ERR_PARAMS_NULL);
        return NULL;
    }

    char path[WORKFLOW_MAX_PATH];
    int result = workflow_build_path(path, sizeof(path), category, event, name);
    if (result != ARGO_SUCCESS) {
        return NULL;
    }

    return workflow_load_from_file(path);
}

/* Free workflow definition */
void workflow_definition_free(workflow_definition_t* def) {
    if (!def) return;
    free(def);
}

/* Build workflow path */
int workflow_build_path(char* buffer, size_t size,
                       const char* category,
                       const char* event,
                       const char* name) {
    ARGO_CHECK_NULL(buffer);
    ARGO_CHECK_NULL(category);
    ARGO_CHECK_NULL(event);
    ARGO_CHECK_NULL(name);

    int written = snprintf(buffer, size, "%s/%s/%s/%s.json",
                          WORKFLOW_BASE_DIR, category, event, name);

    if (written < 0 || (size_t)written >= size) {
        argo_report_error(E_PROTOCOL_SIZE, "workflow_build_path", WORKFLOW_ERR_PATH_TOO_LONG);
        return E_PROTOCOL_SIZE;
    }

    return ARGO_SUCCESS;
}

/* Validate workflow definition */
int workflow_validate_definition(workflow_definition_t* def) {
    ARGO_CHECK_NULL(def);

    if (!def->name[0]) {
        argo_report_error(E_INPUT_INVALID, "workflow_validate_definition",
                         WORKFLOW_ERR_NAME_EMPTY);
        return E_INPUT_INVALID;
    }

    if (def->phase_count <= 0) {
        argo_report_error(E_INPUT_INVALID, "workflow_validate_definition",
                         WORKFLOW_ERR_NO_PHASES);
        return E_INPUT_INVALID;
    }

    if (def->personnel_count <= 0) {
        argo_report_error(E_INPUT_INVALID, "workflow_validate_definition",
                         WORKFLOW_ERR_NO_PERSONNEL);
        return E_INPUT_INVALID;
    }

    /* Validate each phase has tasks */
    for (int i = 0; i < def->phase_count; i++) {
        if (def->phases[i].task_count <= 0) {
            argo_report_error(E_INPUT_INVALID, "workflow_validate_definition",
                             WORKFLOW_ERR_FMT_PHASE_NO_TASKS, i);
            return E_INPUT_INVALID;
        }
    }

    return ARGO_SUCCESS;
}

/* Parse workflow definition from JSON */
workflow_definition_t* workflow_definition_from_json(const char* json) {
    if (!json) return NULL;

    workflow_definition_t* def = calloc(1, sizeof(workflow_definition_t));
    if (!def) return NULL;

    /* Parse name */
    const char* name_start = strstr(json, JSON_FIELD_NAME);
    if (name_start) {
        name_start += JSON_FIELD_NAME_LEN;
        const char* name_end = strchr(name_start, '"');
        if (name_end) {
            size_t len = name_end - name_start;
            if (len < sizeof(def->name)) {
                strncpy(def->name, name_start, len);
            }
        }
    }

    /* Parse description */
    const char* desc_start = strstr(json, JSON_FIELD_DESCRIPTION);
    if (desc_start) {
        desc_start += JSON_FIELD_DESCRIPTION_LEN;
        const char* desc_end = strchr(desc_start, '"');
        if (desc_end) {
            size_t len = desc_end - desc_start;
            if (len < sizeof(def->description)) {
                strncpy(def->description, desc_start, len);
            }
        }
    }

    /* Parse category */
    const char* cat_start = strstr(json, JSON_FIELD_CATEGORY);
    if (cat_start) {
        cat_start += JSON_FIELD_CATEGORY_LEN;
        const char* cat_end = strchr(cat_start, '"');
        if (cat_end) {
            size_t len = cat_end - cat_start;
            if (len < sizeof(def->category)) {
                strncpy(def->category, cat_start, len);
            }
        }
    }

    /* Basic parsing - a real implementation would use a proper JSON library */
    /* For now, just return a minimal valid definition */
    if (!def->name[0]) {
        strncpy(def->name, WORKFLOW_DEFAULT_NAME, sizeof(def->name) - 1);
    }

    /* Set minimal defaults */
    def->phase_count = WORKFLOW_DEFAULT_PHASE_COUNT;
    def->phases[0].phase = WORKFLOW_PLAN;
    strncpy(def->phases[0].name, WORKFLOW_DEFAULT_PHASE_NAME, sizeof(def->phases[0].name) - 1);
    def->phases[0].task_count = WORKFLOW_DEFAULT_TASK_COUNT;
    strncpy(def->phases[0].tasks[0].description, WORKFLOW_DEFAULT_TASK_DESC,
            sizeof(def->phases[0].tasks[0].description) - 1);

    def->personnel_count = WORKFLOW_DEFAULT_PERSONNEL_COUNT;
    strncpy(def->personnel[0].role, WORKFLOW_DEFAULT_ROLE, sizeof(def->personnel[0].role) - 1);
    def->personnel[0].min_count = WORKFLOW_DEFAULT_MIN_COUNT;
    def->personnel[0].max_count = WORKFLOW_DEFAULT_MAX_COUNT;

    return def;
}

/* Convert workflow definition to JSON */
char* workflow_definition_to_json(workflow_definition_t* def) {
    if (!def) return NULL;

    /* Allocate buffer */
    size_t buffer_size = WORKFLOW_JSON_BUFFER_SIZE;
    char* json = malloc(buffer_size);
    if (!json) return NULL;

    int offset = 0;

    /* Start object */
    offset += snprintf(json + offset, buffer_size - offset,
                      "{\n  \"name\": \"%s\",\n", def->name);
    offset += snprintf(json + offset, buffer_size - offset,
                      "  \"description\": \"%s\",\n", def->description);
    offset += snprintf(json + offset, buffer_size - offset,
                      "  \"category\": \"%s\",\n", def->category);
    offset += snprintf(json + offset, buffer_size - offset,
                      "  \"event\": \"%s\",\n", def->event);

    /* Personnel requirements */
    offset += snprintf(json + offset, buffer_size - offset,
                      "  \"personnel\": [\n");
    for (int i = 0; i < def->personnel_count; i++) {
        offset += snprintf(json + offset, buffer_size - offset,
                          "    {\n      \"role\": \"%s\",\n",
                          def->personnel[i].role);
        offset += snprintf(json + offset, buffer_size - offset,
                          "      \"min_count\": %d,\n",
                          def->personnel[i].min_count);
        offset += snprintf(json + offset, buffer_size - offset,
                          "      \"max_count\": %d\n    }%s\n",
                          def->personnel[i].max_count,
                          i < def->personnel_count - 1 ? "," : "");
    }
    offset += snprintf(json + offset, buffer_size - offset, "  ],\n");

    /* Phases */
    offset += snprintf(json + offset, buffer_size - offset,
                      "  \"phases\": [\n");
    for (int i = 0; i < def->phase_count; i++) {
        offset += snprintf(json + offset, buffer_size - offset,
                          "    {\n      \"phase\": %d,\n",
                          def->phases[i].phase);
        offset += snprintf(json + offset, buffer_size - offset,
                          "      \"name\": \"%s\",\n",
                          def->phases[i].name);
        offset += snprintf(json + offset, buffer_size - offset,
                          "      \"tasks\": [\n");

        for (int j = 0; j < def->phases[i].task_count; j++) {
            offset += snprintf(json + offset, buffer_size - offset,
                              "        {\n          \"description\": \"%s\"\n        }%s\n",
                              def->phases[i].tasks[j].description,
                              j < def->phases[i].task_count - 1 ? "," : "");
        }

        offset += snprintf(json + offset, buffer_size - offset,
                          "      ]\n    }%s\n",
                          i < def->phase_count - 1 ? "," : "");
    }
    offset += snprintf(json + offset, buffer_size - offset, "  ]\n");

    /* End object */
    offset += snprintf(json + offset, buffer_size - offset, "}\n");

    return json;
}

/* Execute workflow from definition */
int workflow_execute_definition(argo_orchestrator_t* orch,
                               workflow_definition_t* def,
                               const char* session_id) {
    ARGO_CHECK_NULL(orch);
    ARGO_CHECK_NULL(def);

    /* Validate definition */
    int result = workflow_validate_definition(def);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Start workflow */
    result = orchestrator_start_workflow(orch);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Create tasks from definition */
    for (int i = 0; i < def->phase_count; i++) {
        for (int j = 0; j < def->phases[i].task_count; j++) {
            result = orchestrator_create_task(orch,
                                            def->phases[i].tasks[j].description,
                                            def->phases[i].phase);
            if (result != ARGO_SUCCESS) {
                LOG_WARN("Failed to create task: %s", def->phases[i].tasks[j].description);
            }
        }
    }

    /* Auto-assign tasks */
    result = orchestrator_assign_all_tasks(orch);
    if (result != ARGO_SUCCESS) {
        LOG_WARN("Failed to auto-assign tasks");
    }

    LOG_INFO("Executing workflow: %s (session: %s)", def->name,
             session_id ? session_id : WORKFLOW_DEFAULT_SESSION);

    return ARGO_SUCCESS;
}

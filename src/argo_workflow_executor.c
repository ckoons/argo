/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* External library - header only for struct definitions */
#define JSMN_HEADER
#include "jsmn.h"

/* Project includes */
#include "argo_workflow_executor.h"
#include "argo_workflow_steps.h"
#include "argo_workflow_json.h"
#include "argo_error.h"
#include "argo_log.h"

/* Find step in workflow */
int workflow_find_step(const char* json, jsmntok_t* tokens, const char* step_id) {
    if (!json || !tokens || !step_id) {
        return -1;
    }

    /* Find phases array */
    int phases_idx = workflow_json_find_field(json, tokens, 0, WORKFLOW_JSON_FIELD_PHASES);
    if (phases_idx < 0 || tokens[phases_idx].type != JSMN_ARRAY) {
        return -1;
    }

    /* Search each phase */
    int phase_count = tokens[phases_idx].size;
    int current_token = phases_idx + 1;

    for (int p = 0; p < phase_count; p++) {
        if (tokens[current_token].type != JSMN_OBJECT) {
            return -1;
        }

        /* Find steps array in this phase */
        int steps_idx = workflow_json_find_field(json, tokens, current_token, WORKFLOW_JSON_FIELD_STEPS);
        if (steps_idx >= 0 && tokens[steps_idx].type == JSMN_ARRAY) {
            int step_count = tokens[steps_idx].size;
            int step_token = steps_idx + 1;

            /* Search steps */
            for (int s = 0; s < step_count; s++) {
                if (tokens[step_token].type != JSMN_OBJECT) {
                    continue;
                }

                /* Check step number */
                int step_num_idx = workflow_json_find_field(json, tokens, step_token, WORKFLOW_JSON_FIELD_STEP);
                if (step_num_idx >= 0) {
                    char num_str[WORKFLOW_JSON_INT_BUFFER_SIZE];
                    int step_num;
                    if (workflow_json_extract_int(json, &tokens[step_num_idx], &step_num) == ARGO_SUCCESS) {
                        snprintf(num_str, sizeof(num_str), "%d", step_num);
                        if (strcmp(num_str, step_id) == 0) {
                            return step_token;
                        }
                    }
                }

                /* Skip to next step - count children */
                step_token++;
                int children = tokens[step_token - 1].size;
                for (int c = 0; c < children * 2; c++) {
                    step_token++;
                }
            }
        }

        /* Skip to next phase */
        current_token++;
        int phase_children = tokens[current_token - 1].size;
        for (int c = 0; c < phase_children * 2; c++) {
            current_token++;
        }
    }

    return -1;
}

/* Execute single workflow step */
int workflow_execute_step(const char* json, jsmntok_t* tokens, int step_index,
                         workflow_context_t* ctx,
                         char* next_step, size_t next_step_size) {
    if (!json || !tokens || !ctx || !next_step) {
        argo_report_error(E_INPUT_NULL, "workflow_execute_step", "parameter is NULL");
        return E_INPUT_NULL;
    }

    /* Get step type */
    int type_idx = workflow_json_find_field(json, tokens, step_index, WORKFLOW_JSON_FIELD_TYPE);
    if (type_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "workflow_execute_step", "missing step type");
        return E_PROTOCOL_FORMAT;
    }

    char type[EXECUTOR_TYPE_BUFFER_SIZE];
    int result = workflow_json_extract_string(json, &tokens[type_idx], type, sizeof(type));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Execute step based on type */
    if (strcmp(type, STEP_TYPE_USER_ASK) == 0) {
        result = step_user_ask(json, tokens, step_index, ctx);
    } else if (strcmp(type, STEP_TYPE_DISPLAY) == 0) {
        result = step_display(json, tokens, step_index, ctx);
    } else if (strcmp(type, STEP_TYPE_SAVE_FILE) == 0) {
        result = step_save_file(json, tokens, step_index, ctx);
    } else {
        argo_report_error(E_INPUT_INVALID, "workflow_execute_step", type);
        return E_INPUT_INVALID;
    }

    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Get next step */
    int next_idx = workflow_json_find_field(json, tokens, step_index, WORKFLOW_JSON_FIELD_NEXT_STEP);
    if (next_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "workflow_execute_step", "missing next_step");
        return E_PROTOCOL_FORMAT;
    }

    /* Handle next_step as string or number */
    if (tokens[next_idx].type == JSMN_STRING) {
        result = workflow_json_extract_string(json, &tokens[next_idx], next_step, next_step_size);
    } else if (tokens[next_idx].type == JSMN_PRIMITIVE) {
        int next_num;
        result = workflow_json_extract_int(json, &tokens[next_idx], &next_num);
        if (result == ARGO_SUCCESS) {
            snprintf(next_step, next_step_size, "%d", next_num);
        }
    } else {
        argo_report_error(E_INPUT_INVALID, "workflow_execute_step", "invalid next_step type");
        return E_INPUT_INVALID;
    }

    return result;
}

/* Execute workflow from JSON */
int workflow_execute(const char* json, jsmntok_t* tokens, int token_count) {
    if (!json || !tokens) {
        argo_report_error(E_INPUT_NULL, "workflow_execute", "json or tokens is NULL");
        return E_INPUT_NULL;
    }

    if (token_count <= 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "workflow_execute", "invalid token count");
        return E_PROTOCOL_FORMAT;
    }

    /* Create workflow context */
    workflow_context_t* ctx = workflow_context_create();
    if (!ctx) {
        return E_SYSTEM_MEMORY;
    }

    /* Get workflow name for logging */
    char workflow_name[EXECUTOR_NAME_BUFFER_SIZE] = EXECUTOR_DEFAULT_WORKFLOW_NAME;
    int name_idx = workflow_json_find_field(json, tokens, 0, WORKFLOW_JSON_FIELD_WORKFLOW_NAME);
    if (name_idx >= 0) {
        workflow_json_extract_string(json, &tokens[name_idx], workflow_name, sizeof(workflow_name));
    }

    LOG_INFO("Starting workflow: %s", workflow_name);

    /* Start with step 1 */
    char current_step[EXECUTOR_STEP_NAME_MAX] = "1";
    int step_count = 0;
    int result = ARGO_SUCCESS;

    /* Execute steps until EXIT */
    while (strcmp(current_step, EXECUTOR_STEP_EXIT) != 0) {
        /* Safety check for infinite loops */
        if (step_count >= EXECUTOR_MAX_STEPS) {
            argo_report_error(E_INPUT_INVALID, "workflow_execute", "too many steps");
            result = E_INPUT_INVALID;
            break;
        }

        /* Find current step */
        int step_idx = workflow_find_step(json, tokens, current_step);
        if (step_idx < 0) {
            argo_report_error(E_INPUT_INVALID, "workflow_execute", current_step);
            result = E_INPUT_INVALID;
            break;
        }

        /* Execute step */
        char next_step[EXECUTOR_STEP_NAME_MAX];
        result = workflow_execute_step(json, tokens, step_idx, ctx, next_step, sizeof(next_step));
        if (result != ARGO_SUCCESS) {
            break;
        }

        /* Move to next step */
        strncpy(current_step, next_step, sizeof(current_step) - 1);
        current_step[sizeof(current_step) - 1] = '\0';
        step_count++;
    }

    /* Cleanup */
    workflow_context_destroy(ctx);

    if (result == ARGO_SUCCESS) {
        LOG_INFO("Workflow completed: %s (%d steps)", workflow_name, step_count);
    } else {
        LOG_ERROR("Workflow failed: %s (step %d)", workflow_name, step_count);
    }

    return result;
}

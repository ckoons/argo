/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* External library - header only for struct definitions */
#define JSMN_HEADER
#include "jsmn.h"

/* Project includes */
#include "argo_workflow.h"
#include "argo_workflow_steps.h"
#include "argo_workflow_persona.h"
#include "argo_error.h"
#include "argo_log.h"
#include "argo_json.h"

/* Helper: Check loop and update workflow step tracking */
static int check_and_update_loop(workflow_controller_t* workflow,
                                 const char* json, jsmntok_t* tokens,
                                 int step_idx, const char* next_step) {
    /* Detect loop: check if next_step points to a previous step */
    int is_looping_back = 0;

    /* Convert step IDs to numbers for comparison (if possible) */
    char* endptr;
    long current_num = strtol(workflow->current_step_id, &endptr, 10);
    int current_is_num = (*endptr == '\0');

    long next_num = strtol(next_step, &endptr, 10);
    int next_is_num = (*endptr == '\0');

    /* Loop detected if next step number is less than or equal to current */
    if (current_is_num && next_is_num && next_num <= current_num) {
        is_looping_back = 1;
    }

    if (is_looping_back) {
        /* Check if this is the same loop or a new loop */
        if (workflow->loop_start_step_id[0] == '\0' ||
            strcmp(next_step, workflow->loop_start_step_id) == 0) {
            /* Same loop - increment iteration count */
            workflow->loop_iteration_count++;

            /* Set loop start if not set */
            if (workflow->loop_start_step_id[0] == '\0') {
                strncpy(workflow->loop_start_step_id, next_step, sizeof(workflow->loop_start_step_id) - 1);
            }

            /* Check max_iterations from step definition */
            int max_iterations = EXECUTOR_MAX_LOOP_ITERATIONS;
            int max_iter_idx = workflow_json_find_field(json, tokens, step_idx, STEP_FIELD_MAX_ITERATIONS);
            if (max_iter_idx >= 0) {
                int step_max_iter;
                if (workflow_json_extract_int(json, &tokens[max_iter_idx], &step_max_iter) == ARGO_SUCCESS) {
                    max_iterations = step_max_iter;
                }
            }

            /* Check if we've exceeded max iterations */
            if (workflow->loop_iteration_count > max_iterations) {
                argo_report_error(E_INPUT_INVALID, "check_and_update_loop", "max_iterations exceeded");
                return E_INPUT_INVALID;
            }

            LOG_DEBUG("Loop iteration %d/%d (back to step %s)",
                     workflow->loop_iteration_count, max_iterations, next_step);
        } else {
            /* New loop - reset tracking */
            strncpy(workflow->loop_start_step_id, next_step, sizeof(workflow->loop_start_step_id) - 1);
            workflow->loop_iteration_count = 1;
            LOG_DEBUG("Starting new loop at step %s", next_step);
        }
    } else {
        /* Not looping - reset loop tracking */
        workflow->loop_start_step_id[0] = '\0';
        workflow->loop_iteration_count = 0;
    }

    /* Save previous step before updating */
    strncpy(workflow->previous_step_id, workflow->current_step_id, sizeof(workflow->previous_step_id) - 1);

    /* Update current step */
    strncpy(workflow->current_step_id, next_step, sizeof(workflow->current_step_id) - 1);
    workflow->step_count++;

    return ARGO_SUCCESS;
}

/* Load JSON workflow definition */
int workflow_load_json(workflow_controller_t* workflow, const char* json_path) {
    if (!workflow || !json_path) {
        argo_report_error(E_INPUT_NULL, "workflow_load_json", "workflow or json_path is NULL");
        return E_INPUT_NULL;
    }

    /* Load JSON file */
    size_t json_size;
    char* json = workflow_json_load_file(json_path, &json_size);
    if (!json) {
        argo_report_error(E_NOT_FOUND, "workflow_load_json", json_path);
        return E_NOT_FOUND;
    }

    /* Allocate tokens */
    jsmntok_t* tokens = malloc(sizeof(jsmntok_t) * WORKFLOW_JSON_MAX_TOKENS);
    if (!tokens) {
        free(json);
        argo_report_error(E_SYSTEM_MEMORY, "workflow_load_json", "token allocation failed");
        return E_SYSTEM_MEMORY;
    }

    /* Parse JSON */
    int token_count = workflow_json_parse(json, tokens, WORKFLOW_JSON_MAX_TOKENS);
    if (token_count < 0) {
        free(json);
        free(tokens);
        argo_report_error(E_PROTOCOL_FORMAT, "workflow_load_json", "JSON parse failed");
        return E_PROTOCOL_FORMAT;
    }

    /* Create context */
    workflow_context_t* context = workflow_context_create();
    if (!context) {
        free(json);
        free(tokens);
        return E_SYSTEM_MEMORY;
    }

    /* Create and parse persona registry */
    persona_registry_t* personas = persona_registry_create();
    if (!personas) {
        workflow_context_destroy(context);
        free(json);
        free(tokens);
        return E_SYSTEM_MEMORY;
    }

    int persona_result = persona_registry_parse_json(personas, json, tokens, token_count);
    if (persona_result != ARGO_SUCCESS) {
        LOG_WARN("Failed to parse personas (continuing anyway): %d", persona_result);
    }

    /* Free old workflow data if exists */
    free(workflow->json_workflow);
    free(workflow->tokens);
    if (workflow->context) {
        workflow_context_destroy(workflow->context);
    }
    if (workflow->personas) {
        persona_registry_destroy(workflow->personas);
    }

    /* Store in workflow */
    workflow->json_workflow = json;
    workflow->json_size = json_size;
    workflow->tokens = tokens;
    workflow->token_count = token_count;
    workflow->context = context;
    workflow->personas = personas;
    strncpy(workflow->current_step_id, "1", sizeof(workflow->current_step_id) - 1);
    workflow->step_count = 0;

    LOG_INFO("Loaded workflow JSON: %s (%d tokens, %d personas)",
             json_path, token_count, personas ? personas->count : 0);
    return ARGO_SUCCESS;
}

/* Find step token index by step ID */
int workflow_find_step_token(workflow_controller_t* workflow, const char* step_id) {
    if (!workflow || !step_id || !workflow->json_workflow || !workflow->tokens) {
        return -1;
    }

    const char* json = workflow->json_workflow;
    jsmntok_t* tokens = workflow->tokens;

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

                /* Skip to next step */
                int step_tokens = workflow_json_count_tokens(tokens, step_token);
                step_token += step_tokens;
            }
        }

        /* Skip to next phase */
        int phase_tokens = workflow_json_count_tokens(tokens, current_token);
        current_token += phase_tokens;
    }

    return -1;
}

/* Execute current step */
int workflow_execute_current_step(workflow_controller_t* workflow) {
    if (!workflow || !workflow->json_workflow || !workflow->tokens || !workflow->context) {
        argo_report_error(E_INPUT_NULL, "workflow_execute_current_step", "workflow not loaded");
        return E_INPUT_NULL;
    }

    /* Find current step */
    int step_idx = workflow_find_step_token(workflow, workflow->current_step_id);
    if (step_idx < 0) {
        argo_report_error(E_NOT_FOUND, "workflow_execute_current_step", workflow->current_step_id);
        return E_NOT_FOUND;
    }

    const char* json = workflow->json_workflow;
    jsmntok_t* tokens = workflow->tokens;

    /* Get step type */
    int type_idx = workflow_json_find_field(json, tokens, step_idx, WORKFLOW_JSON_FIELD_TYPE);
    if (type_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "workflow_execute_current_step", "missing step type");
        return E_PROTOCOL_FORMAT;
    }

    char type[EXECUTOR_TYPE_BUFFER_SIZE];
    int result = workflow_json_extract_string(json, &tokens[type_idx], type, sizeof(type));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    char next_step[WORKFLOW_STEP_ID_MAX];

    /* Execute step based on type */
    if (strcmp(type, STEP_TYPE_USER_ASK) == 0) {
        result = step_user_ask(json, tokens, step_idx, workflow->context);
    } else if (strcmp(type, STEP_TYPE_DISPLAY) == 0) {
        result = step_display(json, tokens, step_idx, workflow->context);
    } else if (strcmp(type, STEP_TYPE_SAVE_FILE) == 0) {
        result = step_save_file(json, tokens, step_idx, workflow->context);
    } else if (strcmp(type, STEP_TYPE_DECIDE) == 0) {
        /* decide step determines next_step based on condition */
        result = step_decide(json, tokens, step_idx, workflow->context, next_step, sizeof(next_step));
        if (result == ARGO_SUCCESS) {
            /* Check loop and update step tracking */
            return check_and_update_loop(workflow, json, tokens, step_idx, next_step);
        }
        return result;
    } else if (strcmp(type, STEP_TYPE_USER_CHOOSE) == 0) {
        /* user_choose step determines next_step based on selection */
        result = step_user_choose(json, tokens, step_idx, workflow->context, next_step, sizeof(next_step));
        if (result == ARGO_SUCCESS) {
            /* Check loop and update step tracking */
            return check_and_update_loop(workflow, json, tokens, step_idx, next_step);
        }
        return result;
    } else if (strcmp(type, STEP_TYPE_CI_ASK) == 0) {
        result = step_ci_ask(workflow, json, tokens, step_idx);
    } else if (strcmp(type, STEP_TYPE_CI_ANALYZE) == 0) {
        result = step_ci_analyze(workflow, json, tokens, step_idx);
    } else if (strcmp(type, STEP_TYPE_CI_ASK_SERIES) == 0) {
        result = step_ci_ask_series(workflow, json, tokens, step_idx);
    } else if (strcmp(type, STEP_TYPE_CI_PRESENT) == 0) {
        result = step_ci_present(workflow, json, tokens, step_idx);
    } else if (strcmp(type, STEP_TYPE_WORKFLOW_CALL) == 0) {
        result = step_workflow_call(workflow, json, tokens, step_idx);
    } else if (strcmp(type, STEP_TYPE_PARALLEL) == 0) {
        result = step_parallel(workflow, json, tokens, step_idx);
    } else {
        argo_report_error(E_INPUT_INVALID, "workflow_execute_current_step", type);
        return E_INPUT_INVALID;
    }

    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Get next step from step definition (for non-branching steps) */
    int next_idx = workflow_json_find_field(json, tokens, step_idx, WORKFLOW_JSON_FIELD_NEXT_STEP);
    if (next_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "workflow_execute_current_step", "missing next_step");
        return E_PROTOCOL_FORMAT;
    }

    /* Handle next_step as string or number */
    if (tokens[next_idx].type == JSMN_STRING) {
        result = workflow_json_extract_string(json, &tokens[next_idx], next_step, sizeof(next_step));
    } else if (tokens[next_idx].type == JSMN_PRIMITIVE) {
        int next_num;
        result = workflow_json_extract_int(json, &tokens[next_idx], &next_num);
        if (result == ARGO_SUCCESS) {
            snprintf(next_step, sizeof(next_step), "%d", next_num);
        }
    } else {
        argo_report_error(E_INPUT_INVALID, "workflow_execute_current_step", "invalid next_step type");
        return E_INPUT_INVALID;
    }

    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Check loop and update step tracking */
    return check_and_update_loop(workflow, json, tokens, step_idx, next_step);
}

/* Execute all workflow steps */
int workflow_execute_all_steps(workflow_controller_t* workflow) {
    if (!workflow || !workflow->json_workflow) {
        argo_report_error(E_INPUT_NULL, "workflow_execute_all_steps", "workflow not loaded");
        return E_INPUT_NULL;
    }

    LOG_INFO("Starting workflow execution: %s", workflow->workflow_id);

    /* Execute steps until EXIT */
    while (strcmp(workflow->current_step_id, EXECUTOR_STEP_EXIT) != 0) {
        /* Safety check for infinite loops */
        if (workflow->step_count >= EXECUTOR_MAX_STEPS) {
            argo_report_error(E_INPUT_INVALID, "workflow_execute_all_steps", "too many steps");
            return E_INPUT_INVALID;
        }

        int result = workflow_execute_current_step(workflow);
        if (result != ARGO_SUCCESS) {
            LOG_ERROR("Workflow step failed: %s (step %d)", workflow->workflow_id, workflow->step_count);
            return result;
        }
    }

    LOG_INFO("Workflow completed: %s (%d steps)", workflow->workflow_id, workflow->step_count);
    return ARGO_SUCCESS;
}

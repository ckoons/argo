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
#include "argo_workflow_steps.h"
#include "argo_workflow.h"
#include "argo_workflow_json.h"
#include "argo_socket.h"
#include "argo_error.h"
#include "argo_log.h"
#include "argo_limits.h"

/* Helper: Extract retry configuration from step JSON */
int step_extract_retry_config(const char* json, jsmntok_t* tokens,
                               int step_index, retry_config_t* config) {
    if (!json || !tokens || !config) {
        argo_report_error(E_INPUT_NULL, "step_extract_retry_config", "parameter is NULL");
        return E_INPUT_NULL;
    }

    /* Set defaults */
    config->max_retries = STEP_DEFAULT_MAX_RETRIES;
    config->retry_delay_ms = STEP_DEFAULT_RETRY_DELAY_MS;
    strncpy(config->backoff, RETRY_BACKOFF_EXPONENTIAL, sizeof(config->backoff) - 1);

    /* Check for retry configuration */
    int retry_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_RETRY);
    if (retry_idx < 0) {
        /* No retry config */
        return ARGO_SUCCESS;
    }

    /* Extract max_retries if present */
    int max_retries_idx = workflow_json_find_field(json, tokens, retry_idx, STEP_FIELD_MAX_RETRIES);
    if (max_retries_idx >= 0) {
        char max_retries_str[ARGO_BUFFER_TINY];
        workflow_json_extract_string(json, &tokens[max_retries_idx], max_retries_str, sizeof(max_retries_str));
        config->max_retries = atoi(max_retries_str);
        if (config->max_retries < 0) {
            config->max_retries = 0;
        }
    }

    /* Extract retry_delay if present */
    int delay_idx = workflow_json_find_field(json, tokens, retry_idx, STEP_FIELD_RETRY_DELAY);
    if (delay_idx >= 0) {
        char delay_str[ARGO_BUFFER_TINY];
        workflow_json_extract_string(json, &tokens[delay_idx], delay_str, sizeof(delay_str));
        config->retry_delay_ms = atoi(delay_str);
        if (config->retry_delay_ms < 0) {
            config->retry_delay_ms = STEP_DEFAULT_RETRY_DELAY_MS;
        }
        if (config->retry_delay_ms > STEP_MAX_RETRY_DELAY_MS) {
            config->retry_delay_ms = STEP_MAX_RETRY_DELAY_MS;
        }
    }

    /* Extract backoff strategy if present */
    int backoff_idx = workflow_json_find_field(json, tokens, retry_idx, STEP_FIELD_RETRY_BACKOFF);
    if (backoff_idx >= 0) {
        workflow_json_extract_string(json, &tokens[backoff_idx], config->backoff, sizeof(config->backoff));
    }

    return ARGO_SUCCESS;
}

/* Helper: Calculate retry delay based on backoff strategy */
int step_calculate_retry_delay(const retry_config_t* config, int attempt) {
    if (!config) {
        return STEP_DEFAULT_RETRY_DELAY_MS;
    }

    int delay = config->retry_delay_ms;

    if (strcmp(config->backoff, RETRY_BACKOFF_LINEAR) == 0) {
        /* Linear: delay * (attempt + 1) */
        delay = config->retry_delay_ms * (attempt + 1);
    } else if (strcmp(config->backoff, RETRY_BACKOFF_EXPONENTIAL) == 0) {
        /* Exponential: delay * 2^attempt */
        delay = config->retry_delay_ms;
        for (int i = 0; i < attempt; i++) {
            delay *= 2;
            if (delay > STEP_MAX_RETRY_DELAY_MS) {
                delay = STEP_MAX_RETRY_DELAY_MS;
                break;
            }
        }
    }
    /* RETRY_BACKOFF_FIXED uses base delay without modification */

    if (delay > STEP_MAX_RETRY_DELAY_MS) {
        delay = STEP_MAX_RETRY_DELAY_MS;
    }

    return delay;
}

/* Helper: Execute step with retry logic */
int step_execute_with_retry(workflow_controller_t* workflow,
                            const char* json, jsmntok_t* tokens,
                            int step_index, step_execute_fn execute_fn) {
    if (!workflow || !json || !tokens || !execute_fn) {
        argo_report_error(E_INPUT_NULL, "step_execute_with_retry", "parameter is NULL");
        return E_INPUT_NULL;
    }

    /* Extract retry configuration */
    retry_config_t config;
    int result = step_extract_retry_config(json, tokens, step_index, &config);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* If no retries configured, execute once */
    if (config.max_retries == 0) {
        return execute_fn(workflow, json, tokens, step_index);
    }

    /* Execute with retry logic */
    int attempt = 0;
    while (attempt <= config.max_retries) {
        result = execute_fn(workflow, json, tokens, step_index);

        if (result == ARGO_SUCCESS) {
            if (attempt > 0) {
                LOG_INFO("Step succeeded on retry attempt %d", attempt);
            }
            return ARGO_SUCCESS;
        }

        /* Check if we should retry */
        if (attempt < config.max_retries) {
            int delay_ms = step_calculate_retry_delay(&config, attempt);
            LOG_INFO("Step failed (error %d), retrying in %d ms (attempt %d/%d)",
                    result, delay_ms, attempt + 1, config.max_retries);

            /* Sleep for delay */
            struct timespec ts;
            ts.tv_sec = delay_ms / MS_PER_SECOND;
            ts.tv_nsec = (delay_ms % MS_PER_SECOND) * MS_PER_SECOND * MS_PER_SECOND;
            nanosleep(&ts, NULL);

            attempt++;
        } else {
            /* Max retries reached */
            LOG_ERROR("Step failed after %d retry attempts (error %d)", config.max_retries, result);
            return result;
        }
    }

    return result;
}

/* Helper: Handle step execution error */
int step_handle_error(workflow_controller_t* workflow,
                      const char* json, jsmntok_t* tokens,
                      int step_index, int error_code,
                      char* next_step, size_t next_step_size) {
    if (!workflow || !json || !tokens || !next_step) {
        argo_report_error(E_INPUT_NULL, "step_handle_error", "parameter is NULL");
        return E_INPUT_NULL;
    }

    /* Check for on_error field */
    int on_error_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_ON_ERROR);
    if (on_error_idx < 0) {
        /* No error handler - propagate error */
        return error_code;
    }

    /* on_error can be an object with action and target */
    if (tokens[on_error_idx].type == JSMN_OBJECT) {
        /* Get action */
        int action_idx = workflow_json_find_field(json, tokens, on_error_idx, STEP_FIELD_ERROR_ACTION);
        if (action_idx >= 0) {
            char action[STEP_ACTION_BUFFER_SIZE];
            workflow_json_extract_string(json, &tokens[action_idx], action, sizeof(action));

            if (strcmp(action, ERROR_ACTION_SKIP) == 0) {
                /* Skip to next step */
                int next_idx = workflow_json_find_field(json, tokens, step_index, WORKFLOW_JSON_FIELD_NEXT_STEP);
                if (next_idx >= 0) {
                    workflow_json_extract_string(json, &tokens[next_idx], next_step, next_step_size);
                    LOG_INFO("Error handled: skipping to next step");
                    return ARGO_SUCCESS;
                }
            } else if (strcmp(action, ERROR_ACTION_GOTO) == 0) {
                /* Go to specific step */
                int target_idx = workflow_json_find_field(json, tokens, on_error_idx, STEP_FIELD_ERROR_TARGET);
                if (target_idx >= 0) {
                    workflow_json_extract_string(json, &tokens[target_idx], next_step, next_step_size);
                    LOG_INFO("Error handled: jumping to step %s", next_step);
                    return ARGO_SUCCESS;
                }
            } else if (strcmp(action, ERROR_ACTION_FAIL) == 0) {
                /* Explicitly fail */
                LOG_ERROR("Error handler: explicit failure");
                return error_code;
            }
        }
    } else if (tokens[on_error_idx].type == JSMN_STRING) {
        /* Simple string value - treat as goto target */
        workflow_json_extract_string(json, &tokens[on_error_idx], next_step, next_step_size);
        LOG_INFO("Error handled: jumping to step %s", next_step);
        return ARGO_SUCCESS;
    }

    /* Unable to handle error */
    return error_code;
}

/* Step: workflow_call */
int step_workflow_call(workflow_controller_t* workflow,
                       const char* json, jsmntok_t* tokens, int step_index) {
    if (!workflow || !json || !tokens) {
        argo_report_error(E_INPUT_NULL, "step_workflow_call", "parameter is NULL");
        return E_INPUT_NULL;
    }

    /* Check recursion depth */
    if (workflow->recursion_depth >= WORKFLOW_MAX_RECURSION_DEPTH) {
        argo_report_error(E_INPUT_INVALID, "step_workflow_call", "max recursion depth exceeded");
        return E_INPUT_INVALID;
    }

    workflow_context_t* parent_ctx = workflow->context;

    /* Find workflow path field */
    int workflow_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_WORKFLOW);
    if (workflow_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_workflow_call", "missing workflow field");
        return E_PROTOCOL_FORMAT;
    }

    char workflow_path[STEP_DESTINATION_BUFFER_SIZE];
    int result = workflow_json_extract_string(json, &tokens[workflow_idx],
                                              workflow_path, sizeof(workflow_path));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Find save_to field */
    int save_to_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_SAVE_TO);
    if (save_to_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_workflow_call", "missing save_to field");
        return E_PROTOCOL_FORMAT;
    }

    char save_to[STEP_SAVE_TO_BUFFER_SIZE];
    result = workflow_json_extract_string(json, &tokens[save_to_idx], save_to, sizeof(save_to));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Create child workflow */
    LOG_DEBUG("Calling child workflow: %s (depth=%d)", workflow_path, workflow->recursion_depth + 1);
    workflow_controller_t* child = workflow_create(workflow->registry,
                                                   workflow->lifecycle,
                                                   workflow_path);
    if (!child) {
        return E_SYSTEM_MEMORY;
    }

    /* Inherit provider and personas from parent */
    child->provider = workflow->provider;
    child->personas = workflow->personas;
    child->recursion_depth = workflow->recursion_depth + 1;

    /* Load child workflow */
    result = workflow_load_json(child, workflow_path);
    if (result != ARGO_SUCCESS) {
        workflow_destroy(child);
        return result;
    }

    /* Find input field (optional) */
    int input_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_INPUT);
    if (input_idx >= 0 && tokens[input_idx].type == JSMN_OBJECT) {
        /* Copy input fields from parent context to child context */
        jsmntok_t* input_obj = &tokens[input_idx];
        int field_count = input_obj->size;
        int field_token = input_idx + 1;

        for (int i = 0; i < field_count; i++) {
            /* Get key name */
            if (tokens[field_token].type != JSMN_STRING) {
                field_token++;
                continue;
            }

            char key[STEP_SAVE_TO_BUFFER_SIZE];
            int key_len = tokens[field_token].end - tokens[field_token].start;
            if (key_len >= (int)sizeof(key)) {
                key_len = sizeof(key) - 1;
            }
            strncpy(key, json + tokens[field_token].start, key_len);
            key[key_len] = '\0';
            field_token++;

            /* Get value (can be string or substituted from parent context) */
            char value_template[STEP_OUTPUT_BUFFER_SIZE];
            workflow_json_extract_string(json, &tokens[field_token],
                                        value_template, sizeof(value_template));

            /* Substitute variables from parent context */
            char value[STEP_OUTPUT_BUFFER_SIZE];
            result = workflow_context_substitute(parent_ctx, value_template,
                                                value, sizeof(value));
            if (result != ARGO_SUCCESS) {
                workflow_destroy(child);
                return result;
            }

            /* Set in child context */
            workflow_context_set(child->context, key, value);

            field_token++;
        }
    }

    /* Execute child workflow */
    LOG_INFO("Executing child workflow: %s", workflow_path);
    result = workflow_execute_all_steps(child);
    if (result != ARGO_SUCCESS) {
        LOG_ERROR("Child workflow failed with error %d", result);
        workflow_destroy(child);
        return result;
    }

    /* Save child workflow result to parent context */
    /* Future enhancement: Full context serialization would allow child workflows
     * to return complex data structures back to parent. Currently saves simple
     * success indicator which is sufficient for workflow chaining without data flow. */
    result = workflow_context_set(parent_ctx, save_to, "{\"status\": \"success\"}");

    /* Cleanup child workflow */
    workflow_destroy(child);

    LOG_DEBUG("Child workflow completed successfully");
    return result;
}

/* Step: parallel
 *
 * IMPORTANT: This is currently a SIMULATION ONLY implementation.
 * Steps are not actually executed - only validated and logged.
 *
 * Real parallel execution requires:
 * 1. Thread pool or async task queue
 * 2. Step lookup and execution infrastructure
 * 3. Result collection and synchronization
 * 4. Error aggregation from parallel failures
 *
 * Current behavior: Validates parallel_steps array exists and contains
 * valid step IDs, then returns success. Use for workflow structure testing.
 */
int step_parallel(workflow_controller_t* workflow,
                  const char* json, jsmntok_t* tokens, int step_index) {
    if (!workflow || !json || !tokens) {
        argo_report_error(E_INPUT_NULL, "step_parallel", "parameter is NULL");
        return E_INPUT_NULL;
    }

    /* Find parallel_steps array */
    int steps_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_PARALLEL_STEPS);
    if (steps_idx < 0 || tokens[steps_idx].type != JSMN_ARRAY) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_parallel", "missing or invalid parallel_steps");
        return E_PROTOCOL_FORMAT;
    }

    int step_count = tokens[steps_idx].size;
    if (step_count == 0) {
        LOG_DEBUG("Parallel step has no sub-steps, continuing");
        return ARGO_SUCCESS;
    }

    LOG_INFO("SIMULATION: Validating %d parallel steps (not executing)", step_count);

    /* Validate parallel_steps array (simulation only - no actual execution) */
    int step_token = steps_idx + 1;
    int first_error = ARGO_SUCCESS;
    int success_count = 0;
    int error_count = 0;

    for (int i = 0; i < step_count; i++) {
        /* Validate step ID format */
        char step_id[STEP_ID_BUFFER_SIZE];
        int result = workflow_json_extract_string(json, &tokens[step_token], step_id, sizeof(step_id));
        if (result != ARGO_SUCCESS) {
            LOG_ERROR("Failed to extract parallel step ID at index %d", i);
            error_count++;
            if (first_error == ARGO_SUCCESS) {
                first_error = result;
            }
            step_token++;
            continue;
        }

        LOG_DEBUG("SIMULATION: Validated parallel step %d/%d: %s", i + 1, step_count, step_id);

        /* SIMULATION ONLY: Real implementation would execute step here */
        success_count++;

        step_token++;
    }

    LOG_INFO("SIMULATION: Parallel validation complete: %d valid, %d invalid", success_count, error_count);

    /* Return first error if any occurred, otherwise success */
    if (first_error != ARGO_SUCCESS) {
        LOG_ERROR("SIMULATION: Parallel validation had errors, returning first error: %d", first_error);
    }

    return first_error;
}

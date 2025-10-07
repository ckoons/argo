/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* External library - header only for struct definitions */
#define JSMN_HEADER
#include "jsmn.h"

/* Project includes */
#include "argo_workflow_steps.h"
#include "argo_workflow.h"
#include "argo_workflow_persona.h"
#include "argo_workflow_json.h"
#include "argo_provider.h"
#include "argo_error.h"
#include "argo_log.h"

/* Helper: Simple callback to capture AI response */
typedef struct {
    char* buffer;
    size_t buffer_size;
    size_t bytes_written;
} response_capture_t;

static void capture_response_callback(const ci_response_t* response, void* userdata) {
    response_capture_t* capture = (response_capture_t*)userdata;
    if (!response || !capture || !capture->buffer) return;

    /* Capture both successful and failed responses for logging */
    if (!response->content) return;

    size_t response_len = response->content_len;
    size_t available = capture->buffer_size - capture->bytes_written - 1;

    if (response_len > available) {
        response_len = available;
    }

    if (response_len > 0) {
        memcpy(capture->buffer + capture->bytes_written, response->content, response_len);
        capture->bytes_written += response_len;
        capture->buffer[capture->bytes_written] = '\0';
    }

    /* Log failed responses with their content */
    if (!response->success) {
        LOG_ERROR("Provider returned error response: %s", capture->buffer);
        /* Also print to stdout so it appears in workflow log */
        fprintf(stderr, "\n[ERROR] Provider returned error response:\n%s\n", capture->buffer);
        fflush(stderr);
    }
}

/* Helper: Build AI prompt with persona context */
static int build_ai_prompt_with_persona(workflow_persona_t* persona,
                                         const char* prompt,
                                         char* output,
                                         size_t output_size) {
    if (!prompt || !output) {
        argo_report_error(E_INPUT_NULL, "build_ai_prompt_with_persona", "parameter is NULL");
        return E_INPUT_NULL;
    }

    if (!persona) {
        /* No persona - use prompt directly */
        if (strlen(prompt) >= output_size) {
            argo_report_error(E_INPUT_TOO_LARGE, "build_ai_prompt_with_persona", "prompt too large");
            return E_INPUT_TOO_LARGE;
        }
        strncpy(output, prompt, output_size - 1);
        output[output_size - 1] = '\0';
        return ARGO_SUCCESS;
    }

    /* Build prompt with persona context */
    int written = snprintf(output, output_size,
                          "You are %s, a %s. Your communication style is: %s.\n\n%s",
                          persona->name, persona->role, persona->style, prompt);

    if (written < 0 || (size_t)written >= output_size) {
        argo_report_error(E_INPUT_TOO_LARGE, "build_ai_prompt_with_persona", "constructed prompt too large");
        return E_INPUT_TOO_LARGE;
    }

    return ARGO_SUCCESS;
}

/* Step: ci_ask */
int step_ci_ask(workflow_controller_t* workflow,
                const char* json, jsmntok_t* tokens, int step_index) {
    if (!workflow || !json || !tokens) {
        argo_report_error(E_INPUT_NULL, "step_ci_ask", "parameter is NULL");
        return E_INPUT_NULL;
    }

    workflow_context_t* ctx = workflow->context;

    /* Find persona field (optional) */
    workflow_persona_t* persona = NULL;
    int persona_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_PERSONA);
    if (persona_idx >= 0) {
        char persona_name[STEP_PERSONA_BUFFER_SIZE];
        workflow_json_extract_string(json, &tokens[persona_idx],
                                     persona_name, sizeof(persona_name));
        persona = persona_registry_find(workflow->personas, persona_name);
        if (!persona) {
            LOG_DEBUG("Persona '%s' not found, using default", persona_name);
            persona = persona_registry_get_default(workflow->personas);
        }
    }

    /* Find prompt_template field */
    int prompt_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_PROMPT_TEMPLATE);
    if (prompt_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_ci_ask", "missing prompt_template");
        return E_PROTOCOL_FORMAT;
    }

    char prompt_template[STEP_PROMPT_BUFFER_SIZE];
    int result = workflow_json_extract_string(json, &tokens[prompt_idx],
                                              prompt_template, sizeof(prompt_template));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Substitute variables in prompt */
    char prompt[STEP_OUTPUT_BUFFER_SIZE];
    result = workflow_context_substitute(ctx, prompt_template, prompt, sizeof(prompt));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Find save_to field */
    int save_to_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_SAVE_TO);
    if (save_to_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_ci_ask", "missing save_to");
        return E_PROTOCOL_FORMAT;
    }

    char save_to[STEP_SAVE_TO_BUFFER_SIZE];
    result = workflow_json_extract_string(json, &tokens[save_to_idx], save_to, sizeof(save_to));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Show persona greeting if available */
    if (persona && persona->greeting[0] != '\0') {
        printf("%s\n", persona->greeting);
    }

    /* Optionally use AI to present a more conversational prompt */
    char final_prompt[STEP_OUTPUT_BUFFER_SIZE];
    if (workflow->provider && persona) {
        /* Build AI prompt to generate conversational question */
        char ai_prompt[STEP_AI_PROMPT_BUFFER_SIZE];
        snprintf(ai_prompt, sizeof(ai_prompt),
                "You are %s, a %s. Your communication style is: %s.\n\n"
                "Present this question to the user in a natural, conversational way that matches your persona:\n\n%s\n\n"
                "Respond with ONLY the question itself, no additional commentary.",
                persona->name, persona->role, persona->style, prompt);

        /* Query AI using callback */
        char response[STEP_CI_RESPONSE_BUFFER_SIZE] = {0};
        response_capture_t capture = {
            .buffer = response,
            .buffer_size = sizeof(response),
            .bytes_written = 0
        };

        result = workflow->provider->query(workflow->provider, ai_prompt,
                                          capture_response_callback, &capture);

        if (result == ARGO_SUCCESS && capture.bytes_written > 0) {
            /* Use AI-generated conversational prompt */
            snprintf(final_prompt, sizeof(final_prompt), "[%s] %s ", persona->name, response);
        } else {
            /* Fall back to template prompt */
            if (result != ARGO_SUCCESS) {
                LOG_ERROR("AI query failed (error %d), response: %s",
                         result, capture.bytes_written > 0 ? response : "(empty)");
                fprintf(stderr, "\n[ERROR] AI query failed (error %d), response: %s\n",
                        result, capture.bytes_written > 0 ? response : "(empty)");
                fflush(stderr);
            }
            if (persona->name[0] != '\0') {
                snprintf(final_prompt, sizeof(final_prompt), "[%s] %s ", persona->name, prompt);
            } else {
                snprintf(final_prompt, sizeof(final_prompt), "%s ", prompt);
            }
        }
    } else {
        /* No AI provider - use template prompt directly */
        if (persona && persona->name[0] != '\0') {
            snprintf(final_prompt, sizeof(final_prompt), "[%s] %s ", persona->name, prompt);
        } else {
            snprintf(final_prompt, sizeof(final_prompt), "%s ", prompt);
        }
    }

    printf("%s", final_prompt);
    fflush(stdout);

    /* Read user input */
    char input[STEP_INPUT_BUFFER_SIZE];
    if (!fgets(input, sizeof(input), stdin)) {
        argo_report_error(E_INPUT_INVALID, "step_ci_ask", "failed to read input");
        return E_INPUT_INVALID;
    }

    /* Remove trailing newline */
    size_t len = strlen(input);
    if (len > 0 && input[len - 1] == '\n') {
        input[len - 1] = '\0';
    }

    /* Save to context */
    result = workflow_context_set(ctx, save_to, input);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    LOG_DEBUG("CI ask: persona=%s, saved to '%s': %s",
              persona ? persona->name : "none", save_to, input);
    return ARGO_SUCCESS;
}

/* Step: ci_analyze */
int step_ci_analyze(workflow_controller_t* workflow,
                    const char* json, jsmntok_t* tokens, int step_index) {
    if (!workflow || !json || !tokens) {
        argo_report_error(E_INPUT_NULL, "step_ci_analyze", "parameter is NULL");
        return E_INPUT_NULL;
    }

    workflow_context_t* ctx = workflow->context;

    /* Find persona field (optional) */
    workflow_persona_t* persona = NULL;
    int persona_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_PERSONA);
    if (persona_idx >= 0) {
        char persona_name[STEP_PERSONA_BUFFER_SIZE];
        workflow_json_extract_string(json, &tokens[persona_idx],
                                     persona_name, sizeof(persona_name));
        persona = persona_registry_find(workflow->personas, persona_name);
        if (!persona) {
            LOG_DEBUG("Persona '%s' not found, using default", persona_name);
            persona = persona_registry_get_default(workflow->personas);
        }
    }

    /* Find task field */
    int task_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_TASK);
    if (task_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_ci_analyze", "missing task");
        return E_PROTOCOL_FORMAT;
    }

    char task[STEP_TASK_BUFFER_SIZE];
    int result = workflow_json_extract_string(json, &tokens[task_idx], task, sizeof(task));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Find save_to field */
    int save_to_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_SAVE_TO);
    if (save_to_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_ci_analyze", "missing save_to");
        return E_PROTOCOL_FORMAT;
    }

    char save_to[STEP_SAVE_TO_BUFFER_SIZE];
    result = workflow_json_extract_string(json, &tokens[save_to_idx], save_to, sizeof(save_to));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Show analysis starting */
    if (persona && persona->name[0] != '\0') {
        printf("[%s - Analysis] %s\n", persona->name, task);
    } else {
        printf("[CI Analysis] %s\n", task);
    }

    /* If provider available, use AI to perform analysis */
    if (workflow->provider) {
        /* Build AI prompt with persona context and task */
        char ai_prompt[STEP_AI_PROMPT_BUFFER_SIZE];
        result = build_ai_prompt_with_persona(persona, task, ai_prompt, sizeof(ai_prompt));
        if (result != ARGO_SUCCESS) {
            return result;
        }

        /* Query AI using callback */
        char response[STEP_CI_RESPONSE_BUFFER_SIZE] = {0};
        response_capture_t capture = {
            .buffer = response,
            .buffer_size = sizeof(response),
            .bytes_written = 0
        };

        result = workflow->provider->query(workflow->provider, ai_prompt,
                                          capture_response_callback, &capture);

        if (result != ARGO_SUCCESS) {
            LOG_ERROR("AI query failed (error %d), response: %s",
                     result, capture.bytes_written > 0 ? response : "(empty)");
            fprintf(stderr, "\n[ERROR] AI query failed (error %d), response: %s\n",
                    result, capture.bytes_written > 0 ? response : "(empty)");
            fflush(stderr);
            /* Fall back to placeholder result */
            result = workflow_context_set(ctx, save_to, "{\"analyzed\": true}");
        } else {
            /* Save AI response to context */
            printf("\n[AI Response]\n%s\n", response);
            result = workflow_context_set(ctx, save_to, response);
        }
    } else {
        /* No provider - save placeholder result */
        LOG_DEBUG("No AI provider available, using placeholder result");
        result = workflow_context_set(ctx, save_to, "{\"analyzed\": true}");
    }

    LOG_DEBUG("CI analyze: persona=%s, task='%s', save_to='%s'",
              persona ? persona->name : "none", task, save_to);

    return result;
}

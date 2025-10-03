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
#include "argo_workflow_persona.h"
#include "argo_workflow_json.h"
#include "argo_workflow_conditions.h"
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
    if (!response->success || !response->content) return;

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
}

/* Step: user_ask */
int step_user_ask(const char* json, jsmntok_t* tokens, int step_index,
                  workflow_context_t* ctx) {
    if (!json || !tokens || !ctx) {
        argo_report_error(E_INPUT_NULL, "step_user_ask", "parameter is NULL");
        return E_INPUT_NULL;
    }

    /* Find prompt field */
    int prompt_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_PROMPT);
    if (prompt_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_user_ask", "missing prompt");
        return E_PROTOCOL_FORMAT;
    }

    char prompt[STEP_PROMPT_BUFFER_SIZE];
    int result = workflow_json_extract_string(json, &tokens[prompt_idx], prompt, sizeof(prompt));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Find save_to field */
    int save_to_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_SAVE_TO);
    if (save_to_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_user_ask", "missing save_to");
        return E_PROTOCOL_FORMAT;
    }

    char save_to[STEP_SAVE_TO_BUFFER_SIZE];
    result = workflow_json_extract_string(json, &tokens[save_to_idx], save_to, sizeof(save_to));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Show prompt */
    printf("%s ", prompt);
    fflush(stdout);

    /* Read user input */
    char input[STEP_INPUT_BUFFER_SIZE];
    if (!fgets(input, sizeof(input), stdin)) {
        argo_report_error(E_INPUT_INVALID, "step_user_ask", "failed to read input");
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

    LOG_DEBUG("User input saved to '%s': %s", save_to, input);
    return ARGO_SUCCESS;
}

/* Step: display */
int step_display(const char* json, jsmntok_t* tokens, int step_index,
                 workflow_context_t* ctx) {
    if (!json || !tokens || !ctx) {
        argo_report_error(E_INPUT_NULL, "step_display", "parameter is NULL");
        return E_INPUT_NULL;
    }

    /* Find message field */
    int message_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_MESSAGE);
    if (message_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_display", "missing message");
        return E_PROTOCOL_FORMAT;
    }

    char template[STEP_OUTPUT_BUFFER_SIZE];
    int result = workflow_json_extract_string(json, &tokens[message_idx],
                                              template, sizeof(template));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Substitute variables */
    char output[STEP_OUTPUT_BUFFER_SIZE];
    result = workflow_context_substitute(ctx, template, output, sizeof(output));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Display message */
    printf("%s\n", output);

    LOG_DEBUG("Displayed message: %s", output);
    return ARGO_SUCCESS;
}

/* Step: save_file */
int step_save_file(const char* json, jsmntok_t* tokens, int step_index,
                   workflow_context_t* ctx) {
    if (!json || !tokens || !ctx) {
        argo_report_error(E_INPUT_NULL, "step_save_file", "parameter is NULL");
        return E_INPUT_NULL;
    }

    /* Find destination field */
    int dest_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_DESTINATION);
    if (dest_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_save_file", "missing destination");
        return E_PROTOCOL_FORMAT;
    }

    char destination[STEP_DESTINATION_BUFFER_SIZE];
    int result = workflow_json_extract_string(json, &tokens[dest_idx],
                                              destination, sizeof(destination));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Find data field */
    int data_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_DATA);
    if (data_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_save_file", "missing data");
        return E_PROTOCOL_FORMAT;
    }

    /* Add timestamp to context if not present */
    if (!workflow_context_has(ctx, WORKFLOW_JSON_FIELD_TIMESTAMP)) {
        time_t now = time(NULL);
        char timestamp[STEP_TIMESTAMP_BUFFER_SIZE];
        snprintf(timestamp, sizeof(timestamp), "%ld", (long)now);
        workflow_context_set(ctx, WORKFLOW_JSON_FIELD_TIMESTAMP, timestamp);
    }

    /* Extract data object content (without outer braces) */
    jsmntok_t* data_token = &tokens[data_idx];

    /* Skip opening '{' and closing '}' */
    const char* content_start = json + data_token->start + 1;
    int content_len = data_token->end - data_token->start - 2;

    char data_str[STEP_OUTPUT_BUFFER_SIZE];
    if (content_len >= (int)sizeof(data_str) - 3) {  /* -3 for { } \0 */
        argo_report_error(E_INPUT_TOO_LARGE, "step_save_file", "data object too large");
        return E_INPUT_TOO_LARGE;
    }

    strncpy(data_str, content_start, content_len);
    data_str[content_len] = '\0';

    /* Perform variable substitution on content */
    char substituted[STEP_OUTPUT_BUFFER_SIZE];
    result = workflow_context_substitute(ctx, data_str, substituted, sizeof(substituted) - 3);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Write to file with braces restored */
    FILE* f = fopen(destination, "w");
    if (!f) {
        argo_report_error(E_SYSTEM_FILE, "step_save_file", destination);
        return E_SYSTEM_FILE;
    }

    fprintf(f, "{%s}\n", substituted);
    fclose(f);

    printf("Saved workflow data to: %s\n", destination);
    LOG_DEBUG("Saved file: %s", destination);
    return ARGO_SUCCESS;
}

/* Step: decide */
int step_decide(const char* json, jsmntok_t* tokens, int step_index,
                workflow_context_t* ctx,
                char* next_step, size_t next_step_size) {
    if (!json || !tokens || !ctx || !next_step) {
        argo_report_error(E_INPUT_NULL, "step_decide", "parameter is NULL");
        return E_INPUT_NULL;
    }

    /* Find condition field */
    int condition_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_CONDITION);
    if (condition_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_decide", "missing condition");
        return E_PROTOCOL_FORMAT;
    }

    char condition[STEP_OUTPUT_BUFFER_SIZE];
    int result = workflow_json_extract_string(json, &tokens[condition_idx],
                                              condition, sizeof(condition));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Evaluate condition */
    int condition_result = 0;
    result = workflow_evaluate_condition(ctx, condition, &condition_result);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Find appropriate next step */
    const char* next_field = condition_result ? STEP_FIELD_IF_TRUE : STEP_FIELD_IF_FALSE;
    int next_idx = workflow_json_find_field(json, tokens, step_index, next_field);
    if (next_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_decide", next_field);
        return E_PROTOCOL_FORMAT;
    }

    /* Extract next step (can be string or number) */
    if (tokens[next_idx].type == JSMN_STRING) {
        result = workflow_json_extract_string(json, &tokens[next_idx], next_step, next_step_size);
    } else if (tokens[next_idx].type == JSMN_PRIMITIVE) {
        int next_num;
        result = workflow_json_extract_int(json, &tokens[next_idx], &next_num);
        if (result == ARGO_SUCCESS) {
            snprintf(next_step, next_step_size, "%d", next_num);
        }
    } else {
        argo_report_error(E_INPUT_INVALID, "step_decide", "invalid next_step type");
        return E_INPUT_INVALID;
    }

    LOG_DEBUG("Decide: condition=%d, next_step=%s", condition_result, next_step);
    return result;
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

/* Step: user_choose */
int step_user_choose(const char* json, jsmntok_t* tokens, int step_index,
                     workflow_context_t* ctx,
                     char* next_step, size_t next_step_size) {
    if (!json || !tokens || !ctx || !next_step) {
        argo_report_error(E_INPUT_NULL, "step_user_choose", "parameter is NULL");
        return E_INPUT_NULL;
    }

    /* Find prompt field */
    int prompt_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_PROMPT);
    if (prompt_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_user_choose", "missing prompt");
        return E_PROTOCOL_FORMAT;
    }

    char prompt[STEP_PROMPT_BUFFER_SIZE];
    int result = workflow_json_extract_string(json, &tokens[prompt_idx],
                                              prompt, sizeof(prompt));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Find options array */
    int options_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_OPTIONS);
    if (options_idx < 0 || tokens[options_idx].type != JSMN_ARRAY) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_user_choose", "missing or invalid options");
        return E_PROTOCOL_FORMAT;
    }

    int option_count = tokens[options_idx].size;
    if (option_count == 0) {
        argo_report_error(E_INPUT_INVALID, "step_user_choose", "no options provided");
        return E_INPUT_INVALID;
    }

    /* Display prompt and options */
    printf("\n%s\n", prompt);

    int option_token = options_idx + 1;
    for (int i = 0; i < option_count; i++) {
        if (tokens[option_token].type != JSMN_OBJECT) {
            continue;
        }

        /* Get label */
        int label_idx = workflow_json_find_field(json, tokens, option_token, STEP_FIELD_LABEL);
        if (label_idx >= 0) {
            char label[STEP_SAVE_TO_BUFFER_SIZE];
            workflow_json_extract_string(json, &tokens[label_idx], label, sizeof(label));
            printf("  %d. %s\n", i + 1, label);
        }

        /* Skip to next option */
        int option_tokens = workflow_json_count_tokens(tokens, option_token);
        option_token += option_tokens;
    }

    /* Read user input */
    printf("\nSelect option (1-%d): ", option_count);
    fflush(stdout);

    char input[STEP_INPUT_BUFFER_SIZE];
    if (!fgets(input, sizeof(input), stdin)) {
        argo_report_error(E_INPUT_INVALID, "step_user_choose", "failed to read input");
        return E_INPUT_INVALID;
    }

    /* Remove trailing newline */
    size_t len = strlen(input);
    if (len > 0 && input[len - 1] == '\n') {
        input[len - 1] = '\0';
    }

    /* Parse selection */
    char* endptr;
    long selection = strtol(input, &endptr, 10);
    if (*endptr != '\0' || selection < 1 || selection > option_count) {
        argo_report_error(E_INPUT_INVALID, "step_user_choose", "invalid selection");
        return E_INPUT_INVALID;
    }

    /* Find selected option's next_step */
    option_token = options_idx + 1;
    for (int i = 0; i < option_count; i++) {
        if (i == (int)(selection - 1)) {
            /* This is the selected option */
            int next_idx = workflow_json_find_field(json, tokens, option_token, WORKFLOW_JSON_FIELD_NEXT_STEP);
            if (next_idx < 0) {
                argo_report_error(E_PROTOCOL_FORMAT, "step_user_choose", "option missing next_step");
                return E_PROTOCOL_FORMAT;
            }

            /* Extract next step */
            if (tokens[next_idx].type == JSMN_STRING) {
                result = workflow_json_extract_string(json, &tokens[next_idx], next_step, next_step_size);
            } else if (tokens[next_idx].type == JSMN_PRIMITIVE) {
                int next_num;
                result = workflow_json_extract_int(json, &tokens[next_idx], &next_num);
                if (result == ARGO_SUCCESS) {
                    snprintf(next_step, next_step_size, "%d", next_num);
                }
            } else {
                argo_report_error(E_INPUT_INVALID, "step_user_choose", "invalid next_step type");
                return E_INPUT_INVALID;
            }

            LOG_DEBUG("User selected option %ld, next_step=%s", selection, next_step);
            return result;
        }

        /* Skip to next option */
        int option_tokens = workflow_json_count_tokens(tokens, option_token);
        option_token += option_tokens;
    }

    /* Should never reach here */
    argo_report_error(E_INTERNAL_LOGIC, "step_user_choose", "failed to find selected option");
    return E_INTERNAL_LOGIC;
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
            LOG_ERROR("AI query failed with error %d", result);
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

/* Step: ci_ask_series */
int step_ci_ask_series(workflow_controller_t* workflow,
                       const char* json, jsmntok_t* tokens, int step_index) {
    if (!workflow || !json || !tokens) {
        argo_report_error(E_INPUT_NULL, "step_ci_ask_series", "parameter is NULL");
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

    /* Show persona greeting if available */
    if (persona && persona->greeting[0] != '\0') {
        printf("\n%s\n", persona->greeting);
    }

    /* Find optional intro field */
    int intro_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_INTRO);
    if (intro_idx >= 0) {
        char intro[STEP_PROMPT_BUFFER_SIZE];
        workflow_json_extract_string(json, &tokens[intro_idx], intro, sizeof(intro));
        if (persona && persona->name[0] != '\0') {
            printf("[%s] %s\n", persona->name, intro);
        } else {
            printf("\n%s\n", intro);
        }
    }

    /* Find questions array */
    int questions_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_QUESTIONS);
    if (questions_idx < 0 || tokens[questions_idx].type != JSMN_ARRAY) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_ci_ask_series", "missing or invalid questions");
        return E_PROTOCOL_FORMAT;
    }

    int question_count = tokens[questions_idx].size;
    if (question_count == 0) {
        argo_report_error(E_INPUT_INVALID, "step_ci_ask_series", "no questions provided");
        return E_INPUT_INVALID;
    }

    /* Find save_to field */
    int save_to_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_SAVE_TO);
    if (save_to_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_ci_ask_series", "missing save_to");
        return E_PROTOCOL_FORMAT;
    }

    char save_to[STEP_SAVE_TO_BUFFER_SIZE];
    int result = workflow_json_extract_string(json, &tokens[save_to_idx], save_to, sizeof(save_to));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Iterate through questions */
    int question_token = questions_idx + 1;
    for (int i = 0; i < question_count; i++) {
        if (tokens[question_token].type != JSMN_OBJECT) {
            question_token++;
            continue;
        }

        /* Get question text */
        int q_idx = workflow_json_find_field(json, tokens, question_token, "question");
        if (q_idx >= 0) {
            char question[STEP_PROMPT_BUFFER_SIZE];
            workflow_json_extract_string(json, &tokens[q_idx], question, sizeof(question));

            /* Optionally use AI to present a more conversational question */
            char final_question[STEP_OUTPUT_BUFFER_SIZE];
            if (workflow->provider && persona) {
                /* Build AI prompt to generate conversational question */
                char ai_prompt[STEP_AI_PROMPT_BUFFER_SIZE];
                snprintf(ai_prompt, sizeof(ai_prompt),
                        "You are %s, a %s. Your communication style is: %s.\n\n"
                        "Present this question to the user in a natural, conversational way that matches your persona:\n\n%s\n\n"
                        "Respond with ONLY the question itself, no additional commentary.",
                        persona->name, persona->role, persona->style, question);

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
                    /* Use AI-generated conversational question */
                    snprintf(final_question, sizeof(final_question), "\n[%s] %d. %s ",
                            persona->name, i + 1, response);
                } else {
                    /* Fall back to template question */
                    snprintf(final_question, sizeof(final_question), "\n[%s] %d. %s ",
                            persona->name, i + 1, question);
                }
            } else {
                /* No AI provider - use template question directly */
                if (persona && persona->name[0] != '\0') {
                    snprintf(final_question, sizeof(final_question), "\n[%s] %d. %s ",
                            persona->name, i + 1, question);
                } else {
                    snprintf(final_question, sizeof(final_question), "\n%d. %s ", i + 1, question);
                }
            }

            printf("%s", final_question);
            fflush(stdout);

            /* Read answer */
            char answer[STEP_INPUT_BUFFER_SIZE];
            if (fgets(answer, sizeof(answer), stdin)) {
                size_t len = strlen(answer);
                if (len > 0 && answer[len - 1] == '\n') {
                    answer[len - 1] = '\0';
                }

                /* Get question ID and save answer */
                int id_idx = workflow_json_find_field(json, tokens, question_token, "id");
                if (id_idx >= 0) {
                    char id[STEP_SAVE_TO_BUFFER_SIZE];
                    workflow_json_extract_string(json, &tokens[id_idx], id, sizeof(id));

                    /* Build context path: save_to.id */
                    char full_path[STEP_SAVE_TO_BUFFER_SIZE * 2];
                    snprintf(full_path, sizeof(full_path), "%s.%s", save_to, id);
                    workflow_context_set(ctx, full_path, answer);
                }
            }
        }

        /* Skip to next question */
        int question_tokens = workflow_json_count_tokens(tokens, question_token);
        question_token += question_tokens;
    }

    LOG_DEBUG("CI ask_series: persona=%s, completed %d questions, saved to '%s'",
              persona ? persona->name : "none", question_count, save_to);
    printf("\n");
    return ARGO_SUCCESS;
}

/* Step: ci_present */
int step_ci_present(workflow_controller_t* workflow,
                    const char* json, jsmntok_t* tokens, int step_index) {
    if (!workflow || !json || !tokens) {
        argo_report_error(E_INPUT_NULL, "step_ci_present", "parameter is NULL");
        return E_INPUT_NULL;
    }

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

    /* Find data field (context path to data) */
    int data_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_DATA);
    if (data_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_ci_present", "missing data");
        return E_PROTOCOL_FORMAT;
    }

    char data_path[STEP_SAVE_TO_BUFFER_SIZE];
    int result = workflow_json_extract_string(json, &tokens[data_idx], data_path, sizeof(data_path));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Find format field */
    int format_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_FORMAT);
    char format[STEP_SAVE_TO_BUFFER_SIZE] = "text";
    if (format_idx >= 0) {
        workflow_json_extract_string(json, &tokens[format_idx], format, sizeof(format));
    }

    /* Show presentation header */
    printf("\n");
    printf("========================================\n");
    if (persona && persona->name[0] != '\0') {
        printf("[%s] PRESENTATION (%s format)\n", persona->name, format);
    } else {
        printf("PRESENTATION (%s format)\n", format);
    }
    printf("========================================\n");

    /* If provider available, use AI to format and present */
    if (workflow->provider) {
        /* Get data from context */
        const char* data_value = workflow_context_get(workflow->context, data_path);
        if (!data_value) {
            LOG_DEBUG("Data path '%s' not found in context, using path as value", data_path);
            data_value = data_path;
        }

        /* Build presentation task */
        char task[STEP_AI_PROMPT_BUFFER_SIZE];
        int written = snprintf(task, sizeof(task),
                              "Present the following data in %s format:\n\n%s",
                              format, data_value);
        if (written < 0 || (size_t)written >= sizeof(task)) {
            argo_report_error(E_INPUT_TOO_LARGE, "step_ci_present", "task too large");
            return E_INPUT_TOO_LARGE;
        }

        /* Build AI prompt with persona */
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

        if (result == ARGO_SUCCESS) {
            /* Display AI-formatted presentation */
            printf("\n%s\n", response);
        } else {
            LOG_ERROR("AI query failed with error %d", result);
            printf("\nData source: %s\n", data_path);
            printf("(AI formatting unavailable)\n");
        }
    } else {
        /* No provider - basic display */
        LOG_DEBUG("No AI provider available for presentation");
        printf("\nData source: %s\n", data_path);
        printf("(No AI provider configured for formatting)\n");
    }

    printf("========================================\n");
    printf("\n");

    LOG_DEBUG("CI present: persona=%s, format='%s', data='%s'",
              persona ? persona->name : "none", format, data_path);
    return ARGO_SUCCESS;
}

/* Helper: Build AI prompt with persona context */
int build_ai_prompt_with_persona(workflow_persona_t* persona,
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
    printf("[Workflow Call] Executing: %s\n", workflow_path);
    result = workflow_execute_all_steps(child);
    if (result != ARGO_SUCCESS) {
        LOG_ERROR("Child workflow failed with error %d", result);
        workflow_destroy(child);
        return result;
    }

    /* Save entire child context to parent under save_to */
    /* For now, save a simple success indicator */
    /* TODO: Implement context serialization for full result capture */
    result = workflow_context_set(parent_ctx, save_to, "{\"status\": \"success\"}");

    /* Cleanup child workflow */
    workflow_destroy(child);

    LOG_DEBUG("Child workflow completed successfully");
    return result;
}

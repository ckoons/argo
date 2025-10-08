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
#include "argo_workflow_input.h"
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
        argo_report_error(E_CI_TIMEOUT, "capture_response_callback",
                         "Provider returned error response: %s", capture->buffer);
    }
}


/* Helper: Generate conversational question using AI */
static int generate_conversational_question(ci_provider_t* provider,
                                           workflow_persona_t* persona,
                                           const char* question,
                                           char* output,
                                           size_t output_size) {
    if (!provider || !question || !output) {
        return E_INVALID_PARAMS;
    }

    /* Build AI prompt to generate conversational question */
    char ai_prompt[STEP_AI_PROMPT_BUFFER_SIZE];
    snprintf(ai_prompt, sizeof(ai_prompt),
            "You are %s, a %s. Your communication style is: %s.\n\n"
            "Present this question to the user in a natural, conversational way that matches your persona:\n\n%s\n\n"
            "Respond with ONLY the question itself, no additional commentary.",
            persona ? persona->name : "Assistant",
            persona ? persona->role : "helper",
            persona ? persona->style : "friendly",
            question);

    /* Query AI using callback */
    char response[STEP_CI_RESPONSE_BUFFER_SIZE] = {0};
    response_capture_t capture = {
        .buffer = response,
        .buffer_size = sizeof(response),
        .bytes_written = 0
    };

    int result = provider->query(provider, ai_prompt, capture_response_callback, &capture);
    if (result == ARGO_SUCCESS && capture.bytes_written > 0) {
        snprintf(output, output_size, "%s", response);
        return ARGO_SUCCESS;
    }

    /* Fallback to original question */
    argo_report_error(result, "generate_conversational_question",
                     "AI query failed, response: %s",
                     capture.bytes_written > 0 ? response : "(empty)");
    snprintf(output, output_size, "%s", question);
    return E_CI_TIMEOUT;  /* Query failed, use timeout as closest match */
}

/* Helper: Format question with persona */
static void format_question_with_persona(workflow_persona_t* persona,
                                         int question_num,
                                         const char* question,
                                         char* output,
                                         size_t output_size) {
    if (persona && persona->name[0] != '\0') {
        snprintf(output, output_size, "\n[%s] %d. %s ", persona->name, question_num, question);
    } else {
        snprintf(output, output_size, "\n%d. %s ", question_num, question);
    }
}

/* Helper: Execute one question iteration in series */
static int execute_series_iteration(workflow_controller_t* workflow,
                                    workflow_persona_t* persona,
                                    const char* json,
                                    jsmntok_t* tokens,
                                    int question_token,
                                    int question_num,
                                    const char* save_to) {
    /* Get question text */
    int q_idx = workflow_json_find_field(json, tokens, question_token, "question");
    if (q_idx < 0) {
        return ARGO_SUCCESS;  /* Skip missing questions */
    }

    char question[STEP_PROMPT_BUFFER_SIZE];
    workflow_json_extract_string(json, &tokens[q_idx], question, sizeof(question));

    /* Generate conversational question or use template */
    char final_question[STEP_OUTPUT_BUFFER_SIZE];
    if (workflow->provider && persona) {
        char conversational[STEP_CI_RESPONSE_BUFFER_SIZE];
        if (generate_conversational_question(workflow->provider, persona, question,
                                           conversational, sizeof(conversational)) == ARGO_SUCCESS) {
            format_question_with_persona(persona, question_num, conversational,
                                        final_question, sizeof(final_question));
        } else {
            format_question_with_persona(persona, question_num, question,
                                        final_question, sizeof(final_question));
        }
    } else {
        format_question_with_persona(persona, question_num, question,
                                    final_question, sizeof(final_question));
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
            workflow_context_set(workflow->context, full_path, answer);
        }
    }

    return ARGO_SUCCESS;
}

/* Step: ci_ask_series */
int step_ci_ask_series(workflow_controller_t* workflow,
                       const char* json, jsmntok_t* tokens, int step_index) {
    if (!workflow || !json || !tokens) {
        argo_report_error(E_INPUT_NULL, "step_ci_ask_series", "parameter is NULL");
        return E_INPUT_NULL;
    }

    /* Find persona */
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

    /* Show greeting and intro */
    if (persona && persona->greeting[0] != '\0') {
        printf("\n%s\n", persona->greeting);
    }

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

        execute_series_iteration(workflow, persona, json, tokens, question_token, i + 1, save_to);

        /* Move to next question */
        question_token += workflow_json_count_tokens(tokens, question_token);
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
        result = workflow_persona_build_prompt(persona, task, ai_prompt, sizeof(ai_prompt));
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
            argo_report_error(result, "step_ci_present",
                             "AI query failed, response: %s",
                             capture.bytes_written > 0 ? response : "(empty)");
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

/* Step: user_ci_chat */
int step_user_ci_chat(workflow_controller_t* workflow,
                      const char* json, jsmntok_t* tokens, int step_index) {
    if (!workflow || !json || !tokens) {
        argo_report_error(E_INPUT_NULL, "step_user_ci_chat", "parameter is NULL");
        return E_INPUT_NULL;
    }

    if (!workflow->provider) {
        argo_report_error(E_CI_NO_PROVIDER, "step_user_ci_chat", "no AI provider configured");
        return E_CI_NO_PROVIDER;
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

    /* Find initial_prompt field (optional) */
    char initial_prompt[STEP_PROMPT_BUFFER_SIZE] = {0};
    int prompt_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_PROMPT_TEMPLATE);
    if (prompt_idx >= 0) {
        workflow_json_extract_string(json, &tokens[prompt_idx],
                                     initial_prompt, sizeof(initial_prompt));

        /* Substitute variables in prompt */
        char substituted[STEP_OUTPUT_BUFFER_SIZE];
        int result = workflow_context_substitute(workflow->context, initial_prompt,
                                                 substituted, sizeof(substituted));
        if (result == ARGO_SUCCESS) {
            strncpy(initial_prompt, substituted, sizeof(initial_prompt) - 1);
        }
    }

    /* Find save_to field (optional) */
    char save_to[STEP_SAVE_TO_BUFFER_SIZE] = {0};
    int save_to_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_SAVE_TO);
    if (save_to_idx >= 0) {
        workflow_json_extract_string(json, &tokens[save_to_idx], save_to, sizeof(save_to));
    }

    /* Show greeting */
    if (persona && persona->greeting[0] != '\0') {
        printf("\n%s\n", persona->greeting);
    }

    printf("\n========================================\n");
    if (persona && persona->name[0] != '\0') {
        printf("[%s] Interactive Chat Session\n", persona->name);
    } else {
        printf("Interactive Chat Session\n");
    }
    printf("========================================\n");
    printf("(Press Enter with no input to end chat)\n\n");

    /* Send initial prompt if provided */
    if (initial_prompt[0] != '\0') {
        printf("> %s\n\n", initial_prompt);

        char response[STEP_CI_RESPONSE_BUFFER_SIZE] = {0};
        response_capture_t capture = {
            .buffer = response,
            .buffer_size = sizeof(response),
            .bytes_written = 0
        };

        int result = workflow->provider->query(workflow->provider, initial_prompt,
                                              capture_response_callback, &capture);
        if (result != ARGO_SUCCESS) {
            argo_report_error(result, "step_user_ci_chat",
                             "AI query failed, response: %s",
                             capture.bytes_written > 0 ? response : "(empty)");
            return result;
        }

        printf("[AI Response]\n%s\n\n", response);

        /* Save initial exchange if save_to provided */
        if (save_to[0] != '\0') {
            char exchange[STEP_OUTPUT_BUFFER_SIZE];
            snprintf(exchange, sizeof(exchange), "User: %s\nAI: %s\n",
                    initial_prompt, response);
            workflow_context_set(workflow->context, save_to, exchange);
        }
    }

    /* Create input socket for interactive communication */
    workflow_input_socket_t* input_socket = workflow_input_create(workflow->workflow_id);
    if (!input_socket) {
        argo_report_error(E_SYSTEM_SOCKET, "step_user_ci_chat", "failed to create input socket");
        return E_SYSTEM_SOCKET;
    }

    /* Interactive chat loop */
    int turn = 1;
    while (1) {
        /* Log that we're waiting for input (arc attach will detect this) */
        if (persona && persona->name[0] != '\0') {
            workflow_input_log_waiting(persona->name);
        } else {
            workflow_input_log_waiting("You");
        }

        /* Read user input from socket */
        char input[STEP_INPUT_BUFFER_SIZE];
        int len = workflow_input_read_line(input_socket, input, sizeof(input));
        if (len < 0) {
            /* Error reading */
            argo_report_error(E_SYSTEM_SOCKET, "step_user_ci_chat", "failed to read input");
            workflow_input_destroy(input_socket);
            return E_SYSTEM_SOCKET;
        }

        if (len == 0) {
            /* Empty input = exit chat */
            printf("\n[Chat ended]\n");
            break;
        }

        /* Send to AI */
        char response[STEP_CI_RESPONSE_BUFFER_SIZE] = {0};
        response_capture_t capture = {
            .buffer = response,
            .buffer_size = sizeof(response),
            .bytes_written = 0
        };

        int result = workflow->provider->query(workflow->provider, input,
                                              capture_response_callback, &capture);
        if (result != ARGO_SUCCESS) {
            printf("\n[AI Error: %s]\n", argo_error_message(result));
            continue;  /* Allow retry */
        }

        /* Show AI response */
        printf("\n");
        if (persona && persona->name[0] != '\0') {
            printf("[%s]\n", persona->name);
        } else {
            printf("[AI]\n");
        }
        printf("%s\n\n", response);

        /* Append to conversation history if save_to provided */
        if (save_to[0] != '\0') {
            const char* existing = workflow_context_get(workflow->context, save_to);
            char updated[STEP_OUTPUT_BUFFER_SIZE];

            if (existing) {
                snprintf(updated, sizeof(updated), "%sUser: %s\nAI: %s\n",
                        existing, input, response);
            } else {
                snprintf(updated, sizeof(updated), "User: %s\nAI: %s\n",
                        input, response);
            }
            workflow_context_set(workflow->context, save_to, updated);
        }

        turn++;
    }

    /* Cleanup socket */
    workflow_input_destroy(input_socket);

    printf("========================================\n\n");
    LOG_DEBUG("CI chat: persona=%s, turns=%d, saved to '%s'",
              persona ? persona->name : "none", turn, save_to[0] ? save_to : "none");

    return ARGO_SUCCESS;
}

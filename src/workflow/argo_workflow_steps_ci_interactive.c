/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>

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
#include "argo_limits.h"
#include "argo_urls.h"
#include "argo_io_channel.h"

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

/* Helper: CURL write callback for HTTP response */
static size_t http_response_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    response_capture_t* capture = (response_capture_t*)userp;

    size_t available = capture->buffer_size - capture->bytes_written - 1;
    if (realsize > available) {
        realsize = available;
    }

    if (realsize > 0) {
        memcpy(capture->buffer + capture->bytes_written, contents, realsize);
        capture->bytes_written += realsize;
        capture->buffer[capture->bytes_written] = '\0';
    }

    return size * nmemb;  /* Return original size to avoid curl error */
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

    /* Check for I/O channel */
    if (!workflow->context->io_channel) {
        argo_report_error(E_IO_INVALID, "execute_series_iteration",
                         "no I/O channel available (executor running detached)");
        return E_IO_INVALID;
    }

    /* Send question through I/O channel */
    int result = io_channel_write_str(workflow->context->io_channel, final_question);
    result = io_channel_flush(workflow->context->io_channel);
    if (result != ARGO_SUCCESS) {
        argo_report_error(result, "execute_series_iteration", "failed to flush question");
        return result;
    }

    /* Read answer with polling */
    char answer[STEP_INPUT_BUFFER_SIZE];
    int attempts = 0;
    while (attempts < IO_HTTP_POLL_MAX_ATTEMPTS) {
        result = io_channel_read_line(workflow->context->io_channel, answer, sizeof(answer));

        if (result == ARGO_SUCCESS) {
            break;
        }

        if (result == E_IO_EOF) {
            argo_report_error(E_INPUT_INVALID, "execute_series_iteration", "EOF reading answer");
            return E_INPUT_INVALID;
        }

        if (result == E_IO_WOULDBLOCK) {
            usleep(IO_HTTP_POLL_DELAY_USEC);
            attempts++;
            continue;
        }

        argo_report_error(result, "execute_series_iteration", "failed to read answer");
        return result;
    }

    if (attempts >= IO_HTTP_POLL_MAX_ATTEMPTS) {
        argo_report_error(E_SYSTEM_TIMEOUT, "execute_series_iteration", "timeout waiting for answer");
        return E_SYSTEM_TIMEOUT;
    }

    /* Remove trailing newline */
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

    /* Check for I/O channel */
    workflow_context_t* ctx = workflow->context;
    if (!ctx->io_channel) {
        argo_report_error(E_IO_INVALID, "step_ci_ask_series",
                         "no I/O channel available (executor running detached)");
        return E_IO_INVALID;
    }

    /* Show greeting and intro through I/O channel */
    if (persona && persona->greeting[0] != '\0') {
        io_channel_write_str(ctx->io_channel, "\n");
        io_channel_write_str(ctx->io_channel, persona->greeting);
        io_channel_write_str(ctx->io_channel, "\n");
        io_channel_flush(ctx->io_channel);
    }

    int intro_idx = workflow_json_find_field(json, tokens, step_index, STEP_FIELD_INTRO);
    if (intro_idx >= 0) {
        char intro[STEP_PROMPT_BUFFER_SIZE];
        workflow_json_extract_string(json, &tokens[intro_idx], intro, sizeof(intro));

        char intro_msg[STEP_OUTPUT_BUFFER_SIZE];
        if (persona && persona->name[0] != '\0') {
            snprintf(intro_msg, sizeof(intro_msg), "[%s] %s\n", persona->name, intro);
        } else {
            snprintf(intro_msg, sizeof(intro_msg), "\n%s\n", intro);
        }
        io_channel_write_str(ctx->io_channel, intro_msg);
        io_channel_flush(ctx->io_channel);
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

    /* Send final newline through I/O channel */
    io_channel_write_str(ctx->io_channel, "\n");
    io_channel_flush(ctx->io_channel);

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

    /* Check for I/O channel */
    workflow_context_t* ctx = workflow->context;
    if (!ctx->io_channel) {
        argo_report_error(E_IO_INVALID, "step_ci_present",
                         "no I/O channel available (executor running detached)");
        return E_IO_INVALID;
    }

    /* Show presentation header through I/O channel */
    io_channel_write_str(ctx->io_channel, "\n");
    io_channel_write_str(ctx->io_channel, "========================================\n");

    char header[ARGO_BUFFER_MEDIUM];
    if (persona && persona->name[0] != '\0') {
        snprintf(header, sizeof(header), "[%s] PRESENTATION (%s format)\n", persona->name, format);
    } else {
        snprintf(header, sizeof(header), "PRESENTATION (%s format)\n", format);
    }
    io_channel_write_str(ctx->io_channel, header);
    io_channel_write_str(ctx->io_channel, "========================================\n");
    io_channel_flush(ctx->io_channel);

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
            /* Display AI-formatted presentation through I/O channel */
            io_channel_write_str(ctx->io_channel, "\n");
            io_channel_write_str(ctx->io_channel, response);
            io_channel_write_str(ctx->io_channel, "\n");
        } else {
            argo_report_error(result, "step_ci_present",
                             "AI query failed, response: %s",
                             capture.bytes_written > 0 ? response : "(empty)");

            char fallback[ARGO_BUFFER_MEDIUM];
            snprintf(fallback, sizeof(fallback), "\nData source: %s\n(AI formatting unavailable)\n", data_path);
            io_channel_write_str(ctx->io_channel, fallback);
        }
    } else {
        /* No provider - basic display */
        LOG_DEBUG("No AI provider available for presentation");

        char fallback[ARGO_BUFFER_MEDIUM];
        snprintf(fallback, sizeof(fallback), "\nData source: %s\n(No AI provider configured for formatting)\n", data_path);
        io_channel_write_str(ctx->io_channel, fallback);
    }

    io_channel_write_str(ctx->io_channel, "========================================\n");
    io_channel_write_str(ctx->io_channel, "\n");
    io_channel_flush(ctx->io_channel);

    LOG_DEBUG("CI present: persona=%s, format='%s', data='%s'",
              persona ? persona->name : "none", format, data_path);
    return ARGO_SUCCESS;
}

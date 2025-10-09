/* © 2025 Casey Koons All rights reserved */
/* CI interactive chat workflow step - user-AI conversation sessions */

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

/* Helper: Poll daemon for workflow input via HTTP */
static int poll_daemon_for_input(const char* workflow_id, char* input_buffer, size_t buffer_size) {
    if (!workflow_id || !input_buffer || buffer_size == 0) {
        return E_INVALID_PARAMS;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        return E_SYSTEM_MEMORY;
    }

    /* Build URL: GET /api/workflow/input/{workflow_id} */
    char url[ARGO_PATH_MAX];
    snprintf(url, sizeof(url), "http://%s:%d/api/workflow/input/%s",
            DEFAULT_DAEMON_HOST, DEFAULT_DAEMON_PORT, workflow_id);

    /* Setup response capture */
    char response_buffer[ARGO_BUFFER_STANDARD] = {0};
    response_capture_t capture = {
        .buffer = response_buffer,
        .buffer_size = sizeof(response_buffer),
        .bytes_written = 0
    };

    /* Configure CURL */
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_response_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &capture);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);

    /* Perform request */
    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        return E_SYSTEM_NETWORK;
    }

    /* Check HTTP status */
    if (http_code == 204) {
        /* No content - queue is empty */
        return E_NOT_FOUND;
    } else if (http_code != 200) {
        return E_SYSTEM_NETWORK;
    }

    /* Extract input field from JSON response: {"workflow_id":"...", "input":"..."} */
    const char* input_field = strstr(response_buffer, "\"input\"");
    if (!input_field) {
        return E_INVALID_PARAMS;
    }

    /* Extract input value (simple JSON parsing) */
    const char* value_start = strchr(input_field, ':');
    if (!value_start) {
        return E_INVALID_PARAMS;
    }
    value_start = strchr(value_start, '"');
    if (!value_start) {
        return E_INVALID_PARAMS;
    }
    value_start++;  /* Skip opening quote */

    const char* value_end = strchr(value_start, '"');
    if (!value_end) {
        return E_INVALID_PARAMS;
    }

    size_t value_len = value_end - value_start;
    if (value_len >= buffer_size) {
        value_len = buffer_size - 1;
    }

    memcpy(input_buffer, value_start, value_len);
    input_buffer[value_len] = '\0';

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

    /* Check for I/O channel */
    workflow_context_t* ctx = workflow->context;
    if (!ctx->io_channel) {
        argo_report_error(E_IO_INVALID, "step_user_ci_chat",
                         "no I/O channel available (executor running detached)");
        return E_IO_INVALID;
    }

    /* Show greeting through I/O channel */
    if (persona && persona->greeting[0] != '\0') {
        io_channel_write_str(ctx->io_channel, "\n");
        io_channel_write_str(ctx->io_channel, persona->greeting);
        io_channel_write_str(ctx->io_channel, "\n");
    }

    io_channel_write_str(ctx->io_channel, "\n========================================\n");

    char session_header[ARGO_BUFFER_MEDIUM];
    if (persona && persona->name[0] != '\0') {
        snprintf(session_header, sizeof(session_header), "[%s] Interactive Chat Session\n", persona->name);
    } else {
        snprintf(session_header, sizeof(session_header), "Interactive Chat Session\n");
    }
    io_channel_write_str(ctx->io_channel, session_header);
    io_channel_write_str(ctx->io_channel, "========================================\n");
    io_channel_write_str(ctx->io_channel, "(Press Enter with no input to end chat)\n\n");
    io_channel_flush(ctx->io_channel);

    /* Send initial prompt if provided */
    if (initial_prompt[0] != '\0') {
        char prompt_msg[ARGO_BUFFER_MEDIUM];
        snprintf(prompt_msg, sizeof(prompt_msg), "> %s\n\n", initial_prompt);
        io_channel_write_str(ctx->io_channel, prompt_msg);
        io_channel_flush(ctx->io_channel);

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

        io_channel_write_str(ctx->io_channel, "[AI Response]\n");
        io_channel_write_str(ctx->io_channel, response);
        io_channel_write_str(ctx->io_channel, "\n\n");
        io_channel_flush(ctx->io_channel);

        /* Save initial exchange if save_to provided */
        if (save_to[0] != '\0') {
            char exchange[STEP_OUTPUT_BUFFER_SIZE];
            snprintf(exchange, sizeof(exchange), "User: %s\nAI: %s\n",
                    initial_prompt, response);
            workflow_context_set(workflow->context, save_to, exchange);
        }
    }

    /* Interactive chat loop - poll daemon for user input */
    int turn = 1;
    while (1) {
        /* Log that we're waiting for input (arc attach will detect this) */
        if (persona && persona->name[0] != '\0') {
            workflow_input_log_waiting(persona->name);
        } else {
            workflow_input_log_waiting("You");
        }

        /* Poll daemon for user input via HTTP */
        char input[STEP_INPUT_BUFFER_SIZE];
        int result;
        while (1) {
            result = poll_daemon_for_input(workflow->workflow_id, input, sizeof(input));
            if (result == ARGO_SUCCESS) {
                break;  /* Got input */
            } else if (result == E_NOT_FOUND) {
                /* Queue empty, poll again after short delay */
                usleep(500 * MICROSECONDS_PER_MILLISECOND);  /* 500ms */
                continue;
            } else {
                /* Error polling */
                argo_report_error(result, "step_user_ci_chat", "failed to poll for input");
                return result;
            }
        }

        /* Check for empty input (exit chat) */
        if (input[0] == '\0' || strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
            io_channel_write_str(ctx->io_channel, "\n[Chat ended]\n");
            io_channel_flush(ctx->io_channel);
            break;
        }

        /* Send to AI */
        char response[STEP_CI_RESPONSE_BUFFER_SIZE] = {0};
        response_capture_t capture = {
            .buffer = response,
            .buffer_size = sizeof(response),
            .bytes_written = 0
        };

        int query_result = workflow->provider->query(workflow->provider, input,
                                                     capture_response_callback, &capture);
        if (query_result != ARGO_SUCCESS) {
            char error_msg[ARGO_BUFFER_MEDIUM];
            snprintf(error_msg, sizeof(error_msg), "\n[AI Error: %s]\n", argo_error_message(query_result));
            io_channel_write_str(ctx->io_channel, error_msg);
            io_channel_flush(ctx->io_channel);
            continue;  /* Allow retry */
        }

        /* Show AI response through I/O channel */
        io_channel_write_str(ctx->io_channel, "\n");

        char ai_header[ARGO_BUFFER_SMALL];
        if (persona && persona->name[0] != '\0') {
            snprintf(ai_header, sizeof(ai_header), "[%s]\n", persona->name);
        } else {
            snprintf(ai_header, sizeof(ai_header), "[AI]\n");
        }
        io_channel_write_str(ctx->io_channel, ai_header);
        io_channel_write_str(ctx->io_channel, response);
        io_channel_write_str(ctx->io_channel, "\n\n");
        io_channel_flush(ctx->io_channel);

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

    io_channel_write_str(ctx->io_channel, "========================================\n\n");
    io_channel_flush(ctx->io_channel);

    LOG_DEBUG("CI chat: persona=%s, turns=%d, saved to '%s'",
              persona ? persona->name : "none", turn, save_to[0] ? save_to : "none");

    return ARGO_SUCCESS;
}

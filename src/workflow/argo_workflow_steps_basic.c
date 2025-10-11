/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* External library - header only for struct definitions */
#define JSMN_HEADER
#include "jsmn.h"

/* Project includes */
#include "argo_workflow_steps.h"
#include "argo_workflow.h"
#include "argo_workflow_json.h"
#include "argo_workflow_conditions.h"
#include "argo_error.h"
#include "argo_log.h"
#include "argo_limits.h"
#include "argo_io_channel.h"

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

    /* Check if we have an I/O channel (required for interactive workflows) */
    if (!ctx->io_channel) {
        argo_report_error(E_IO_INVALID, "step_user_ask",
                         "no I/O channel available (executor running detached)");
        return E_IO_INVALID;
    }

    LOG_DEBUG("step_user_ask: Sending prompt to stdout (log file): '%s'", prompt);

    /* Send prompt to stdout (which is redirected to log file by daemon) */
    printf("%s ", prompt);
    fflush(stdout);

    /* Read user input from I/O channel with polling */
    char input[STEP_INPUT_BUFFER_SIZE];
    int attempts = 0;

    LOG_DEBUG("step_user_ask: Polling for user input (max attempts: %d)", IO_HTTP_POLL_MAX_ATTEMPTS);

    while (attempts < IO_HTTP_POLL_MAX_ATTEMPTS) {
        LOG_DEBUG("step_user_ask: Poll attempt %d/%d", attempts + 1, IO_HTTP_POLL_MAX_ATTEMPTS);
        result = io_channel_read_line(ctx->io_channel, input, sizeof(input));

        if (result == ARGO_SUCCESS) {
            LOG_DEBUG("step_user_ask: Got input: '%s'", input);
            break;  /* Got input successfully */
        }

        if (result == E_IO_EOF) {
            argo_report_error(E_INPUT_INVALID, "step_user_ask", "EOF reading input");
            return E_INPUT_INVALID;
        }

        if (result == E_IO_WOULDBLOCK) {
            /* No data available yet - wait and retry */
            usleep(IO_HTTP_POLL_DELAY_USEC);
            attempts++;
            continue;
        }

        /* Other errors */
        argo_report_error(result, "step_user_ask", "failed to read input");
        return result;
    }

    if (attempts >= IO_HTTP_POLL_MAX_ATTEMPTS) {
        argo_report_error(E_SYSTEM_TIMEOUT, "step_user_ask", "timeout waiting for user input");
        return E_SYSTEM_TIMEOUT;
    }

    /* Save to context */
    result = workflow_context_set(ctx, save_to, input);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    LOG_DEBUG("User input saved to '%s': %s", save_to, input);
    return ARGO_SUCCESS;
}

/* Helper: Process escape sequences in string */
static void process_escape_sequences(char* str) {
    if (!str) return;

    char* src = str;
    char* dst = str;

    while (*src) {
        if (*src == '\\' && *(src + 1)) {
            switch (*(src + 1)) {
                case 'n':  *dst++ = '\n'; src += 2; break;
                case 't':  *dst++ = '\t'; src += 2; break;
                case 'r':  *dst++ = '\r'; src += 2; break;
                case '\\': *dst++ = '\\'; src += 2; break;
                case '"':  *dst++ = '"';  src += 2; break;
                default:   *dst++ = *src++; break;
            }
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
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

    /* Process escape sequences (\n, \t, etc.) */
    process_escape_sequences(output);

    /* Display message to stdout (redirected to log file by daemon) */
    printf("%s", output);
    fflush(stdout);

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

    /* Check if we have an I/O channel (required for interactive workflows) */
    if (!ctx->io_channel) {
        argo_report_error(E_IO_INVALID, "step_user_choose",
                         "no I/O channel available (executor running detached)");
        return E_IO_INVALID;
    }

    /* Display prompt and options through I/O channel */
    result = io_channel_write_str(ctx->io_channel, "\n");
    result = io_channel_write_str(ctx->io_channel, prompt);
    result = io_channel_write_str(ctx->io_channel, "\n");

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

            char option_line[ARGO_BUFFER_STANDARD];
            snprintf(option_line, sizeof(option_line), "  %d. %s\n", i + 1, label);
            result = io_channel_write_str(ctx->io_channel, option_line);
        }

        /* Skip to next option */
        int option_tokens = workflow_json_count_tokens(tokens, option_token);
        option_token += option_tokens;
    }

    /* Send selection prompt */
    char selection_prompt[ARGO_BUFFER_SMALL];
    snprintf(selection_prompt, sizeof(selection_prompt), "\nSelect option (1-%d): ", option_count);
    result = io_channel_write_str(ctx->io_channel, selection_prompt);
    result = io_channel_flush(ctx->io_channel);
    if (result != ARGO_SUCCESS) {
        argo_report_error(result, "step_user_choose", "failed to flush prompt");
        return result;
    }

    /* Read user input with polling */
    char input[STEP_INPUT_BUFFER_SIZE];
    int attempts = 0;

    while (attempts < IO_HTTP_POLL_MAX_ATTEMPTS) {
        result = io_channel_read_line(ctx->io_channel, input, sizeof(input));

        if (result == ARGO_SUCCESS) {
            break;  /* Got input successfully */
        }

        if (result == E_IO_EOF) {
            argo_report_error(E_INPUT_INVALID, "step_user_choose", "EOF reading input");
            return E_INPUT_INVALID;
        }

        if (result == E_IO_WOULDBLOCK) {
            /* No data available yet - wait and retry */
            usleep(IO_HTTP_POLL_DELAY_USEC);
            attempts++;
            continue;
        }

        /* Other errors */
        argo_report_error(result, "step_user_choose", "failed to read input");
        return result;
    }

    if (attempts >= IO_HTTP_POLL_MAX_ATTEMPTS) {
        argo_report_error(E_SYSTEM_TIMEOUT, "step_user_choose", "timeout waiting for user input");
        return E_SYSTEM_TIMEOUT;
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

            LOG_DEBUG("User chose option %ld, next_step=%s", selection, next_step);
            return result;
        }

        /* Skip to next option */
        int option_tokens = workflow_json_count_tokens(tokens, option_token);
        option_token += option_tokens;
    }

    /* Should never reach here */
    argo_report_error(E_INTERNAL_LOGIC, "step_user_choose", "option not found");
    return E_INTERNAL_LOGIC;
}

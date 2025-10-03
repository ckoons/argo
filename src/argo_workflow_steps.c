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
#include "argo_workflow_json.h"
#include "argo_error.h"
#include "argo_log.h"

/* Step: user_ask */
int step_user_ask(const char* json, jsmntok_t* tokens, int step_index,
                  workflow_context_t* ctx) {
    if (!json || !tokens || !ctx) {
        argo_report_error(E_INPUT_NULL, "step_user_ask", "parameter is NULL");
        return E_INPUT_NULL;
    }

    /* Find prompt field */
    int prompt_idx = workflow_json_find_field(json, tokens, step_index, "prompt");
    if (prompt_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_user_ask", "missing prompt");
        return E_PROTOCOL_FORMAT;
    }

    char prompt[512];
    int result = workflow_json_extract_string(json, &tokens[prompt_idx], prompt, sizeof(prompt));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Find save_to field */
    int save_to_idx = workflow_json_find_field(json, tokens, step_index, "save_to");
    if (save_to_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_user_ask", "missing save_to");
        return E_PROTOCOL_FORMAT;
    }

    char save_to[256];
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
    int message_idx = workflow_json_find_field(json, tokens, step_index, "message");
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
    int dest_idx = workflow_json_find_field(json, tokens, step_index, "destination");
    if (dest_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_save_file", "missing destination");
        return E_PROTOCOL_FORMAT;
    }

    char destination[512];
    int result = workflow_json_extract_string(json, &tokens[dest_idx],
                                              destination, sizeof(destination));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Find data field */
    int data_idx = workflow_json_find_field(json, tokens, step_index, "data");
    if (data_idx < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "step_save_file", "missing data");
        return E_PROTOCOL_FORMAT;
    }

    /* Add timestamp to context if not present */
    if (!workflow_context_has(ctx, "timestamp")) {
        time_t now = time(NULL);
        char timestamp[64];
        snprintf(timestamp, sizeof(timestamp), "%ld", (long)now);
        workflow_context_set(ctx, "timestamp", timestamp);
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

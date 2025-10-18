/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Project includes */
#include "ci_commands.h"
#include "ci_http_client.h"
#include "ci_constants.h"
#include "argo_output.h"
#include "argo_error.h"
#include "argo_json.h"

/* Check if stdin has data available (piped input) */
static int has_stdin_data(void) {
    return !isatty(STDIN_FILENO);
}

/* Read all data from stdin */
static char* read_stdin_data(void) {
    size_t capacity = CI_READ_CHUNK_SIZE;
    size_t size = 0;
    char* buffer = malloc(capacity);
    if (!buffer) {
        return NULL;
    }

    while (!feof(stdin)) {
        size_t bytes_read = fread(buffer + size, 1, CI_READ_CHUNK_SIZE, stdin);
        size += bytes_read;

        if (size + CI_READ_CHUNK_SIZE > capacity) {
            capacity *= 2;
            char* new_buffer = realloc(buffer, capacity);
            if (!new_buffer) {
                free(buffer);
                return NULL;
            }
            buffer = new_buffer;
        }
    }

    buffer[size] = '\0';
    return buffer;
}

/* Build prompt from args and stdin */
static char* build_prompt(int argc, char** argv) {
    char* stdin_data = NULL;
    char* prompt = malloc(CI_QUERY_MAX);
    if (!prompt) {
        return NULL;
    }

    prompt[0] = '\0';

    /* GUIDELINE_APPROVED - Query prompt formatting (separators) */
    /* Read stdin if available */
    if (has_stdin_data()) {
        stdin_data = read_stdin_data();
        if (stdin_data) {
            strncat(prompt, stdin_data, CI_QUERY_MAX - strlen(prompt) - 1);
            strncat(prompt, "\n\n", CI_QUERY_MAX - strlen(prompt) - 1);
            free(stdin_data);
        }
    }

    /* Add command line arguments */
    for (int i = 0; i < argc; i++) {
        /* Skip --provider and --model options */
        if (strcmp(argv[i], "--provider") == 0 || strcmp(argv[i], "--model") == 0) {
            i++;  /* Skip next arg too */
            continue;
        }

        if (strlen(prompt) > 0) {
            strncat(prompt, " ", CI_QUERY_MAX - strlen(prompt) - 1);
        }
        strncat(prompt, argv[i], CI_QUERY_MAX - strlen(prompt) - 1);
    }
    /* GUIDELINE_APPROVED_END */

    /* Check if we got any input */
    if (strlen(prompt) == 0) {
        free(prompt);
        return NULL;
    }

    return prompt;
}

/* ci query command handler */
int ci_cmd_query(int argc, char** argv) {
    int result = CI_EXIT_SUCCESS;
    char* prompt = NULL;
    char* json_request = NULL;
    ci_http_response_t* response = NULL;

    /* Build prompt from args + stdin */
    prompt = build_prompt(argc, argv);
    if (!prompt) {
        LOG_USER_ERROR("No query provided. Use: ci query \"your question\"\n");
        LOG_USER_INFO("  Or pipe input: echo \"data\" | ci query\n");
        return CI_EXIT_ERROR;
    }

    /* Build JSON request with escaped prompt */
    size_t json_size = strlen(prompt) * CI_JSON_SIZE_MULTIPLIER + CI_JSON_OVERHEAD;  /* Extra space for escaping + JSON structure */
    json_request = malloc(json_size);
    if (!json_request) {
        LOG_USER_ERROR("Out of memory\n");
        result = CI_EXIT_ERROR;
        goto cleanup;
    }

    /* Parse --provider and --model options */
    const char* provider = "claude_code";  /* Default */
    const char* model = NULL;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--provider") == 0 && i + 1 < argc) {
            provider = argv[i + 1];
        } else if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) {
            model = argv[i + 1];
        }
    }

    /* Build JSON with proper escaping */
    size_t offset = 0;
    if (model) {
        offset += snprintf(json_request + offset, json_size - offset,
                          "{\"query\":\"");
        if (json_escape_string(json_request, json_size, &offset, prompt) != ARGO_SUCCESS) {
            LOG_USER_ERROR("Failed to prepare query\n");
            result = CI_EXIT_ERROR;
            goto cleanup;
        }
        offset += snprintf(json_request + offset, json_size - offset,
                          "\",\"provider\":\"%s\",\"model\":\"%s\"}",
                          provider, model);
    } else {
        offset += snprintf(json_request + offset, json_size - offset,
                          "{\"query\":\"");
        if (json_escape_string(json_request, json_size, &offset, prompt) != ARGO_SUCCESS) {
            LOG_USER_ERROR("Failed to prepare query\n");
            result = CI_EXIT_ERROR;
            goto cleanup;
        }
        offset += snprintf(json_request + offset, json_size - offset,
                          "\",\"provider\":\"%s\"}",
                          provider);
    }

    /* Send HTTP request to daemon */
    int http_result = ci_http_post("/api/ci/query", json_request, &response);
    if (http_result != ARGO_SUCCESS) {
        LOG_USER_ERROR("Failed to connect to daemon\n");
        LOG_USER_INFO("  Make sure daemon is running: argo-daemon --port %d\n", CI_DEFAULT_DAEMON_PORT);
        result = CI_EXIT_ERROR;
        goto cleanup;
    }

    /* Check HTTP status */
    if (response->status_code != CI_HTTP_STATUS_OK) {
        LOG_USER_ERROR("Query failed (HTTP %d)\n", response->status_code);
        if (response->body) {
            LOG_USER_INFO("  %s\n", response->body);
        }
        result = CI_EXIT_ERROR;
        goto cleanup;
    }

    /* Extract and display response */
    if (response->body) {
        const char* field_path[] = {"response"};
        char* ai_response = NULL;
        size_t response_len = 0;

        if (json_extract_nested_string(response->body, field_path, 1, &ai_response, &response_len) == ARGO_SUCCESS) {
            printf("%s\n", ai_response);
            free(ai_response);
        } else {
            /* Fallback - just print raw body */
            printf("%s\n", response->body);
        }
    }

cleanup:
    free(prompt);
    free(json_request);
    ci_http_response_free(response);
    return result;
}

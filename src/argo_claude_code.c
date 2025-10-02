/* © 2025 Casey Koons All rights reserved */

/* Claude Code Prompt Mode Implementation
 * This provider communicates with Claude Code via prompt files
 * rather than using the Claude CLI directly.
 */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>

/* Project includes */
#include "argo_ci.h"
#include "argo_ci_defaults.h"
#include "argo_ci_common.h"
#include "argo_api_common.h"
#include "argo_error.h"
#include "argo_error_messages.h"
#include "argo_log.h"
#include "argo_filesystem.h"
#include "argo_claude.h"

/* Claude Code context structure */
typedef struct claude_code_context {
    /* Session info */
    char session_id[CLAUDE_CODE_SESSION_ID_SIZE];
    char prompt_file[CLAUDE_CODE_PATH_SIZE];
    char response_file[CLAUDE_CODE_PATH_SIZE];

    /* Communication state */
    bool connected;
    int prompt_counter;

    /* Response buffer */
    char* response_content;
    size_t response_size;
    size_t response_capacity;

    /* Statistics */
    uint64_t total_queries;
    time_t last_query;

    /* Provider interface */
    ci_provider_t provider;
} claude_code_context_t;

/* Static function declarations */
static int claude_code_init(ci_provider_t* provider);
static int claude_code_connect(ci_provider_t* provider);
static int claude_code_query(ci_provider_t* provider, const char* prompt,
                            ci_response_callback callback, void* userdata);
static int claude_code_stream(ci_provider_t* provider, const char* prompt,
                             ci_stream_callback callback, void* userdata);
static void claude_code_cleanup(ci_provider_t* provider);

/* Helper functions - will be added when interactive mode is implemented */

/* Create Claude Code provider */
ci_provider_t* claude_code_create_provider(const char* ci_name) {
    claude_code_context_t* ctx = calloc(1, sizeof(claude_code_context_t));
    if (!ctx) {
        argo_report_error(E_SYSTEM_MEMORY, "claude_code_create_provider", ERR_MSG_MEMORY_ALLOC_FAILED);
        return NULL;
    }

    /* Setup provider interface */
    init_provider_base(&ctx->provider, ctx, claude_code_init, claude_code_connect,
                      claude_code_query, claude_code_stream, claude_code_cleanup);

    /* Set provider info */
    strncpy(ctx->provider.name, "claude_code", sizeof(ctx->provider.name) - 1);
    strncpy(ctx->provider.model, "claude-sonnet-4-5", sizeof(ctx->provider.model) - 1);
    ctx->provider.supports_streaming = true;
    ctx->provider.supports_memory = true;
    ctx->provider.supports_sunset_sunrise = true;
    ctx->provider.max_context = CLAUDE_CODE_MAX_CONTEXT;

    /* Set session info */
    if (ci_name) {
        strncpy(ctx->session_id, ci_name, sizeof(ctx->session_id) - 1);
    } else {
        snprintf(ctx->session_id, sizeof(ctx->session_id), "claude_code_%ld", time(NULL));
    }

    /* Setup file paths - use .argo directory */
    snprintf(ctx->prompt_file, sizeof(ctx->prompt_file),
             ".argo/prompts/%s_prompt.txt", ctx->session_id);
    snprintf(ctx->response_file, sizeof(ctx->response_file),
             ".argo/prompts/%s_response.txt", ctx->session_id);

    LOG_INFO("Created Claude Code provider for session %s", ctx->session_id);
    return &ctx->provider;
}

/* Initialize Claude Code provider */
static int claude_code_init(ci_provider_t* provider) {
    ARGO_GET_CONTEXT(provider, claude_code_context_t, ctx);

    /* Create directories if needed */
    mkdir(".argo", ARGO_DIR_MODE_STANDARD);
    mkdir(".argo/prompts", ARGO_DIR_MODE_STANDARD);

    /* Allocate response buffer using common utility */
    int result = api_allocate_response_buffer(&ctx->response_content,
                                              &ctx->response_capacity,
                                              CLAUDE_CODE_RESPONSE_CAPACITY);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Clear any old files */
    unlink(ctx->prompt_file);
    unlink(ctx->response_file);

    LOG_DEBUG("Claude Code provider initialized");
    return ARGO_SUCCESS;
}

/* Connect to Claude Code (no-op for prompt mode) */
static int claude_code_connect(ci_provider_t* provider) {
    ARGO_GET_CONTEXT(provider, claude_code_context_t, ctx);

    ctx->connected = true;
    LOG_INFO("Claude Code provider ready for prompts");
    return ARGO_SUCCESS;
}

/* Write prompt to file */
static int write_prompt_file(claude_code_context_t* ctx, const char* prompt) {
    FILE* fp = fopen(ctx->prompt_file, "w");
    if (!fp) {
        argo_report_error(E_SYSTEM_FILE, "write_prompt_file", ERR_FMT_FAILED_TO_OPEN, ctx->prompt_file);
        return E_SYSTEM_FILE;
    }

    fprintf(fp, "%s\n", prompt);
    fclose(fp);
    return ARGO_SUCCESS;
}

/* Read response file */
static int read_response_file(claude_code_context_t* ctx) {
    FILE* fp = fopen(ctx->response_file, "r");
    if (!fp) {
        return E_SYSTEM_FILE;
    }

    /* Read file into buffer */
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(fp);
        return E_PROTOCOL_FORMAT;
    }

    /* Ensure buffer capacity */
    int result = ensure_buffer_capacity(&ctx->response_content, &ctx->response_capacity,
                                       file_size + 1);
    if (result != ARGO_SUCCESS) {
        fclose(fp);
        return result;
    }

    size_t read = fread(ctx->response_content, 1, file_size, fp);
    ctx->response_content[read] = '\0';
    ctx->response_size = read;

    fclose(fp);
    return ARGO_SUCCESS;
}

/* Query Claude Code via prompt file */
static int claude_code_query(ci_provider_t* provider, const char* prompt,
                            ci_response_callback callback, void* userdata) {
    ARGO_CHECK_NULL(prompt);
    ARGO_CHECK_NULL(callback);
    ARGO_GET_CONTEXT(provider, claude_code_context_t, ctx);

    /* Increment counter for unique prompt */
    ctx->prompt_counter++;

    /* Write prompt to file */
    int result = write_prompt_file(ctx, prompt);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Display the prompt to Claude Code (me) */
    printf("\n");
    printf("========================================\n");
    printf("CLAUDE CODE PROMPT MODE - Request #%d\n", ctx->prompt_counter);
    printf("========================================\n");
    printf("Session: %s\n", ctx->session_id);
    printf("Prompt file: %s\n", ctx->prompt_file);
    printf("Response file: %s\n", ctx->response_file);
    printf("----------------------------------------\n");
    printf("PROMPT:\n%s\n", prompt);
    printf("----------------------------------------\n");
    printf("Please write your response to: %s\n", ctx->response_file);
    printf("========================================\n\n");

    /* Wait for response file (with timeout) */
    int timeout = CLAUDE_CODE_TIMEOUT_SECONDS;
    int waited = 0;

    while (waited < timeout) {
        if (access(ctx->response_file, F_OK) == 0) {
            /* Response file exists, read it */
            result = read_response_file(ctx);
            if (result == ARGO_SUCCESS) {
                break;
            }
        }
        sleep(1);
        waited++;
    }

    if (waited >= timeout) {
        argo_report_error(E_CI_TIMEOUT, "claude_code_query", ERR_MSG_CI_TIMEOUT);
        return E_CI_TIMEOUT;
    }

    /* Build response */
    ci_response_t response;
    build_ci_response(&response, true, ARGO_SUCCESS,
                     ctx->response_content, "claude-code-prompt");

    /* Update statistics */
    ARGO_UPDATE_STATS(ctx);

    /* Invoke callback */
    callback(&response, userdata);

    /* Clean up files for next request */
    unlink(ctx->prompt_file);
    unlink(ctx->response_file);

    LOG_DEBUG("Claude Code prompt #%d completed", ctx->prompt_counter);
    return ARGO_SUCCESS;
}

/* Stream from Claude Code - reads response file as it's written */
static int claude_code_stream(ci_provider_t* provider, const char* prompt,
                             ci_stream_callback callback, void* userdata) {
    ARGO_CHECK_NULL(prompt);
    ARGO_CHECK_NULL(callback);
    ARGO_GET_CONTEXT(provider, claude_code_context_t, ctx);

    /* Increment counter */
    ctx->prompt_counter++;

    /* Write prompt to file */
    int result = write_prompt_file(ctx, prompt);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Display prompt */
    printf("\n");
    printf("========================================\n");
    printf("CLAUDE CODE STREAMING MODE - Request #%d\n", ctx->prompt_counter);
    printf("========================================\n");
    printf("Session: %s\n", ctx->session_id);
    printf("Response file: %s\n", ctx->response_file);
    printf("----------------------------------------\n");
    printf("PROMPT:\n%s\n", prompt);
    printf("----------------------------------------\n");
    printf("Please write your response to: %s\n", ctx->response_file);
    printf("Response will be streamed as you write.\n");
    printf("========================================\n\n");

    /* Stream response as file is written */
    int timeout = CLAUDE_CODE_TIMEOUT_SECONDS;
    int waited = 0;
    long last_size = 0;
    char chunk_buffer[CLAUDE_CODE_CHUNK_BUFFER_SIZE];

    while (waited < timeout) {
        FILE* fp = fopen(ctx->response_file, "r");
        if (fp) {
            /* Get file size */
            fseek(fp, 0, SEEK_END);
            long current_size = ftell(fp);

            if (current_size > last_size) {
                /* New content available */
                fseek(fp, last_size, SEEK_SET);
                size_t to_read = current_size - last_size;

                while (to_read > 0) {
                    size_t chunk_size = to_read < sizeof(chunk_buffer) ?
                                       to_read : sizeof(chunk_buffer);
                    size_t read_bytes = fread(chunk_buffer, 1, chunk_size, fp);

                    if (read_bytes > 0) {
                        /* Call streaming callback with chunk */
                        callback(chunk_buffer, read_bytes, userdata);
                        to_read -= read_bytes;
                    } else {
                        break;
                    }
                }

                last_size = current_size;
            }

            fclose(fp);

            /* Check for completion marker in file */
            if (current_size > 0) {
                fp = fopen(ctx->response_file, "r");
                if (fp) {
                    fseek(fp, 0, SEEK_END);
                    long size = ftell(fp);
                    if (size > CLAUDE_CODE_MIN_FILE_SIZE) {
                        fclose(fp);
                        /* Assume complete after no changes for specified delay */
                        if (last_size == current_size) {
                            sleep(CLAUDE_CODE_COMPLETION_CHECK_DELAY);
                            /* Recheck */
                            fp = fopen(ctx->response_file, "r");
                            if (fp) {
                                fseek(fp, 0, SEEK_END);
                                long recheck_size = ftell(fp);
                                fclose(fp);
                                if (recheck_size == last_size) {
                                    /* No new content, assume complete */
                                    break;
                                }
                            }
                        }
                    } else {
                        fclose(fp);
                    }
                }
            }
        }

        sleep(1);
        waited++;
    }

    /* Update statistics */
    ARGO_UPDATE_STATS(ctx);

    /* Clean up files */
    unlink(ctx->prompt_file);
    unlink(ctx->response_file);

    LOG_DEBUG("Claude Code streaming #%d completed", ctx->prompt_counter);
    return waited >= timeout ? E_CI_TIMEOUT : ARGO_SUCCESS;
}

/* Cleanup Claude Code provider */
static void claude_code_cleanup(ci_provider_t* provider) {
    if (!provider) return;

    claude_code_context_t* ctx = (claude_code_context_t*)provider->context;
    if (!ctx) return;

    /* Free response buffer */
    if (ctx->response_content) {
        free(ctx->response_content);
    }

    /* Remove files */
    unlink(ctx->prompt_file);
    unlink(ctx->response_file);

    LOG_INFO("Claude Code provider cleanup: queries=%llu", ctx->total_queries);

    free(ctx);
}

/* Future: These functions will be implemented when interactive file mode is added */

/* Check if Claude Code is available */
bool claude_code_is_available(void) {
    /* Claude Code prompt mode is always available */
    return true;
}
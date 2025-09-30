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
#include "argo_error.h"
#include "argo_log.h"
#include "argo_claude.h"

/* Claude Code context structure */
typedef struct claude_code_context {
    /* Session info */
    char session_id[64];
    char prompt_file[256];
    char response_file[256];

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
        LOG_ERROR("Failed to allocate Claude Code context");
        return NULL;
    }

    /* Setup provider interface */
    init_provider_base(&ctx->provider, ctx, claude_code_init, claude_code_connect,
                      claude_code_query, claude_code_stream, claude_code_cleanup);

    /* Set provider info */
    strncpy(ctx->provider.name, "claude_code", sizeof(ctx->provider.name) - 1);
    strncpy(ctx->provider.model, "claude-opus-4.1", sizeof(ctx->provider.model) - 1);
    ctx->provider.supports_streaming = false;  /* Start with non-streaming */
    ctx->provider.supports_memory = true;
    ctx->provider.max_context = 200000;  /* Claude's context window */

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
    mkdir(".argo", 0755);
    mkdir(".argo/prompts", 0755);

    /* Allocate response buffer */
    ctx->response_capacity = 65536;  /* Start with 64KB */
    ctx->response_content = malloc(ctx->response_capacity);
    if (!ctx->response_content) {
        return E_SYSTEM_MEMORY;
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

/* Query Claude Code via prompt file */
static int claude_code_query(ci_provider_t* provider, const char* prompt,
                            ci_response_callback callback, void* userdata) {
    ARGO_CHECK_NULL(prompt);
    ARGO_CHECK_NULL(callback);
    ARGO_GET_CONTEXT(provider, claude_code_context_t, ctx);

    /* Increment counter for unique prompt */
    ctx->prompt_counter++;

    /* Display the prompt to Claude Code (me) */
    printf("\n");
    printf("========================================\n");
    printf("CLAUDE CODE PROMPT MODE - Request #%d\n", ctx->prompt_counter);
    printf("========================================\n");
    printf("Session: %s\n", ctx->session_id);
    printf("----------------------------------------\n");
    printf("PROMPT:\n%s\n", prompt);
    printf("----------------------------------------\n");
    printf("Please respond with your analysis/answer.\n");
    printf("When complete, type: END_RESPONSE\n");
    printf("========================================\n\n");

    /* For now, simulate a response for testing */
    /* In production, this would wait for actual Claude Code input */
    const char* simulated_response =
        "I understand your request. As Claude Code in prompt mode, "
        "I would analyze this and provide a thoughtful response. "
        "This is a test implementation showing the communication pathway works.";

    /* Copy response to context buffer */
    size_t resp_len = strlen(simulated_response);
    int result = ensure_buffer_capacity(&ctx->response_content, &ctx->response_capacity,
                                       resp_len + 1);
    if (result != ARGO_SUCCESS) {
        return result;
    }
    strcpy(ctx->response_content, simulated_response);
    ctx->response_size = resp_len;

    /* Build response */
    ci_response_t response;
    build_ci_response(&response, true, ARGO_SUCCESS,
                     ctx->response_content, "claude-code-prompt");

    /* Update statistics */
    ARGO_UPDATE_STATS(ctx);

    /* Invoke callback */
    callback(&response, userdata);

    LOG_DEBUG("Claude Code prompt #%d completed", ctx->prompt_counter);
    return ARGO_SUCCESS;
}

/* Stream from Claude Code (not implemented yet) */
static int claude_code_stream(ci_provider_t* provider, const char* prompt,
                             ci_stream_callback callback, void* userdata) {
    (void)provider;
    (void)prompt;
    (void)callback;
    (void)userdata;

    /* Claude Code prompt mode doesn't support streaming yet */
    return E_INTERNAL_NOTIMPL;
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
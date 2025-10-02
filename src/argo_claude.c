/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Project includes */
#include "argo_ci.h"
#include "argo_ci_defaults.h"
#include "argo_error.h"
#include "argo_error_messages.h"
#include "argo_log.h"
#include "argo_claude.h"
#include "argo_claude_internal.h"
#include "argo_claude_process.h"
#include "argo_claude_memory.h"

/* Static function declarations */
static int claude_init(ci_provider_t* provider);
static int claude_connect(ci_provider_t* provider);
static int claude_query(ci_provider_t* provider, const char* prompt,
                       ci_response_callback callback, void* userdata);
static int claude_stream(ci_provider_t* provider, const char* prompt,
                        ci_stream_callback callback, void* userdata);
static void claude_cleanup(ci_provider_t* provider);

/* Sunset/sunrise handling */
static bool check_context_limit(claude_context_t* ctx, size_t new_tokens);
static int trigger_sunset(claude_context_t* ctx);

/* Error response helper */
static void respond_with_error(ci_response_callback callback, void* userdata,
                              int error_code, const char* message) {
    ci_response_t response = {
        .success = false,
        .error_code = error_code,
        .content = (char*)message,
        .model_used = "claude-3.5-sonnet",
        .timestamp = time(NULL)
    };
    callback(&response, userdata);
}

/* Create Claude provider */
ci_provider_t* claude_create_provider(const char* ci_name) {
    claude_context_t* ctx = calloc(1, sizeof(claude_context_t));
    if (!ctx) {
        argo_report_error(E_SYSTEM_MEMORY, "claude_create_provider", ERR_MSG_MEMORY_ALLOC_FAILED);
        return NULL;
    }

    /* Setup provider interface */
    ctx->provider.init = claude_init;
    ctx->provider.connect = claude_connect;
    ctx->provider.query = claude_query;
    ctx->provider.stream = claude_stream;
    ctx->provider.cleanup = claude_cleanup;
    ctx->provider.context = ctx;

    /* Set Claude configuration */
    strncpy(ctx->provider.name, "claude", sizeof(ctx->provider.name) - 1);
    strncpy(ctx->provider.model, "claude-3.5-sonnet", sizeof(ctx->provider.model) - 1);

    /* Look up model defaults */
    const ci_model_config_t* config = ci_get_model_defaults("claude-3.5-sonnet");
    if (config) {
        ctx->provider.supports_streaming = false;  /* Not with CLI */
        ctx->provider.supports_memory = true;
        ctx->provider.supports_sunset_sunrise = true;
        ctx->provider.max_context = config->max_context;
        ctx->context_limit = config->max_context;
    } else {
        ctx->context_limit = 200000;  /* Default 200K tokens */
    }

    /* Initialize pipes to invalid */
    ctx->stdin_pipe[0] = ctx->stdin_pipe[1] = -1;
    ctx->stdout_pipe[0] = ctx->stdout_pipe[1] = -1;
    ctx->stderr_pipe[0] = ctx->stderr_pipe[1] = -1;
    ctx->claude_pid = -1;

    /* Build session path */
    snprintf(ctx->session_path, sizeof(ctx->session_path),
             CLAUDE_SESSION_PATH, ci_name ? ci_name : "default");

    LOG_INFO("Created Claude provider for %s", ci_name ? ci_name : "default");
    return &ctx->provider;
}

/* Initialize Claude provider */
static int claude_init(ci_provider_t* provider) {
    ARGO_CHECK_NULL(provider);
    claude_context_t* ctx = (claude_context_t*)provider->context;
    ARGO_CHECK_NULL(ctx);

    /* Allocate response buffer */
    ctx->response_capacity = ARGO_BUFFER_LARGE;
    ctx->response_buffer = malloc(ctx->response_capacity);
    if (!ctx->response_buffer) {
        return E_SYSTEM_MEMORY;
    }

    /* Setup working memory */
    int result = setup_working_memory(ctx, ctx->provider.name);
    if (result != ARGO_SUCCESS) {
        free(ctx->response_buffer);
        return result;
    }

    /* Try to load existing session */
    if (load_working_memory(ctx) == ARGO_SUCCESS) {
        LOG_INFO("Loaded existing Claude session from %s", ctx->session_path);
    }

    ctx->session_start = time(NULL);
    LOG_DEBUG("Claude provider initialized");
    return ARGO_SUCCESS;
}

/* Connect (spawn Claude process) */
static int claude_connect(ci_provider_t* provider) {
    ARGO_CHECK_NULL(provider);
    claude_context_t* ctx = (claude_context_t*)provider->context;
    ARGO_CHECK_NULL(ctx);

    if (ctx->claude_pid > 0) {
        /* Already connected */
        return ARGO_SUCCESS;
    }

    return spawn_claude_process(ctx);
}

/* Query Claude */
static int claude_query(ci_provider_t* provider, const char* prompt,
                       ci_response_callback callback, void* userdata) {
    ARGO_CHECK_NULL(provider);
    ARGO_CHECK_NULL(prompt);
    ARGO_CHECK_NULL(callback);

    claude_context_t* ctx = (claude_context_t*)provider->context;
    ARGO_CHECK_NULL(ctx);

    /* Check context limit */
    size_t prompt_tokens = strlen(prompt) / 4;  /* Rough estimate */
    if (check_context_limit(ctx, prompt_tokens)) {
        LOG_INFO("Approaching context limit, triggering sunset");
        trigger_sunset(ctx);
    }

    /* Build full context with working memory */
    char* full_prompt = build_context_with_memory(ctx, prompt);
    if (!full_prompt) {
        respond_with_error(callback, userdata, E_SYSTEM_MEMORY, "Failed to build context");
        return E_SYSTEM_MEMORY;
    }

    /* Spawn Claude if not running */
    if (ctx->claude_pid <= 0) {
        int result = spawn_claude_process(ctx);
        if (result != ARGO_SUCCESS) {
            free(full_prompt);
            respond_with_error(callback, userdata, result, "Failed to spawn Claude process");
            return result;
        }
    }

    /* Send prompt to Claude */
    int result = write_to_claude(ctx, full_prompt);
    free(full_prompt);

    if (result != ARGO_SUCCESS) {
        respond_with_error(callback, userdata, result, "Failed to send prompt to Claude");
        return result;
    }

    /* Read response */
    char* output = NULL;
    result = read_from_claude(ctx, &output, CLAUDE_TIMEOUT_MS);

    if (result != ARGO_SUCCESS) {
        respond_with_error(callback, userdata, result, "Failed to read Claude response");
        return result;
    }

    /* Update working memory session tracking */
    claude_memory_update_turn(ctx);

    /* Build response */
    ci_response_t response = {
        .success = true,
        .error_code = ARGO_SUCCESS,
        .content = output,
        .model_used = "claude-3.5-sonnet",
        .timestamp = time(NULL)
    };

    /* Update statistics */
    ctx->total_queries++;
    ctx->last_query = time(NULL);
    ctx->tokens_used += strlen(output) / 4;  /* Rough estimate */

    /* Save working memory */
    save_working_memory(ctx);

    /* Invoke callback */
    callback(&response, userdata);

    LOG_DEBUG("Claude query completed, response size: %zu", strlen(output));
    return ARGO_SUCCESS;
}

/* Streaming not supported with CLI */
static int claude_stream(ci_provider_t* provider, const char* prompt,
                        ci_stream_callback callback, void* userdata) {
    (void)provider;
    (void)prompt;
    (void)callback;
    (void)userdata;
    return E_INTERNAL_NOTIMPL;
}

/* Cleanup Claude provider */
static void claude_cleanup(ci_provider_t* provider) {
    if (!provider) return;

    claude_context_t* ctx = (claude_context_t*)provider->context;
    if (!ctx) return;

    /* Kill Claude process if running */
    if (ctx->claude_pid > 0) {
        kill_claude_process(ctx);
    }

    /* Save and cleanup working memory */
    save_working_memory(ctx);
    cleanup_working_memory(ctx);

    /* Free buffers */
    if (ctx->response_buffer) {
        free(ctx->response_buffer);
    }
    if (ctx->sunset_notes) {
        free(ctx->sunset_notes);
    }

    LOG_INFO("Claude provider cleanup: queries=%llu tokens=%zu",
             ctx->total_queries, ctx->tokens_used);

    free(ctx);
}

/* Check context limit */
static bool check_context_limit(claude_context_t* ctx, size_t new_tokens) {
    /* Simple check - real implementation would be more sophisticated */
    return (ctx->tokens_used + new_tokens) > (ctx->context_limit * 0.8);
}

/* Trigger sunset */
static int trigger_sunset(claude_context_t* ctx) {
    /* Would implement sunset protocol here */
    LOG_INFO("Sunset triggered at %zu tokens", ctx->tokens_used);
    return ARGO_SUCCESS;
}

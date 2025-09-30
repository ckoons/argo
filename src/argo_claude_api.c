/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Project includes */
#include "argo_ci.h"
#include "argo_ci_common.h"
#include "argo_api_providers.h"
#include "argo_api_keys.h"
#include "argo_http.h"
#include "argo_error.h"
#include "argo_log.h"

/* Claude API context */
typedef struct {
    char model[64];
    char* response_content;
    size_t response_capacity;
    uint64_t total_queries;
    time_t last_query;
    ci_provider_t provider;
} claude_api_context_t;

/* Static functions */
static int claude_api_init(ci_provider_t* provider);
static int claude_api_connect(ci_provider_t* provider);
static int claude_api_query(ci_provider_t* provider, const char* prompt,
                           ci_response_callback callback, void* userdata);
static int claude_api_stream(ci_provider_t* provider, const char* prompt,
                            ci_stream_callback callback, void* userdata);
static void claude_api_cleanup(ci_provider_t* provider);

/* Create Claude API provider */
ci_provider_t* claude_api_create_provider(const char* model) {
    claude_api_context_t* ctx = calloc(1, sizeof(claude_api_context_t));
    if (!ctx) {
        LOG_ERROR("Failed to allocate Claude API context");
        return NULL;
    }

    /* Setup provider interface */
    init_provider_base(&ctx->provider, ctx, claude_api_init, claude_api_connect,
                      claude_api_query, claude_api_stream, claude_api_cleanup);

    /* Set model */
    strncpy(ctx->model, model ? model : CLAUDE_DEFAULT_MODEL, sizeof(ctx->model) - 1);

    /* Set provider info */
    strncpy(ctx->provider.name, "claude-api", sizeof(ctx->provider.name) - 1);
    strncpy(ctx->provider.model, ctx->model, sizeof(ctx->provider.model) - 1);
    ctx->provider.supports_streaming = true;
    ctx->provider.supports_memory = false;
    ctx->provider.max_context = 200000;

    LOG_INFO("Created Claude API provider for model %s", ctx->model);
    return &ctx->provider;
}

/* Initialize */
static int claude_api_init(ci_provider_t* provider) {
    ARGO_GET_CONTEXT(provider, claude_api_context_t, ctx);

    /* Allocate response buffer */
    ctx->response_capacity = 65536;
    ctx->response_content = malloc(ctx->response_capacity);
    if (!ctx->response_content) {
        return E_SYSTEM_MEMORY;
    }

    /* Initialize HTTP client */
    return http_init();
}

/* Connect (no-op for API) */
static int claude_api_connect(ci_provider_t* provider) {
    (void)provider;
    return ARGO_SUCCESS;
}

/* Query Claude API */
static int claude_api_query(ci_provider_t* provider, const char* prompt,
                           ci_response_callback callback, void* userdata) {
    ARGO_CHECK_NULL(prompt);
    ARGO_CHECK_NULL(callback);
    ARGO_GET_CONTEXT(provider, claude_api_context_t, ctx);

    /* Build request JSON */
    char json_body[8192];
    snprintf(json_body, sizeof(json_body),
            "{"
            "\"model\":\"%s\","
            "\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],"
            "\"max_tokens\":4096"
            "}",
            ctx->model, prompt);

    /* Create HTTP request */
    http_request_t* req = http_request_new(HTTP_POST, ANTHROPIC_API_URL);
    http_request_add_header(req, "Content-Type", "application/json");
    http_request_add_header(req, "x-api-key", ANTHROPIC_API_KEY);
    http_request_add_header(req, "anthropic-version", "2023-06-01");
    http_request_set_body(req, json_body, strlen(json_body));

    /* Execute request */
    http_response_t* resp = NULL;
    int result = http_execute(req, &resp);
    http_request_free(req);

    if (result != ARGO_SUCCESS) {
        LOG_ERROR("Failed to execute Claude API request");
        return result;
    }

    if (resp->status_code != 200) {
        LOG_ERROR("Claude API returned status %d: %s", resp->status_code, resp->body);
        http_response_free(resp);
        return E_PROTOCOL_HTTP;
    }

    /* Extract content from response JSON (simplified) */
    char* content_start = strstr(resp->body, "\"text\":\"");
    if (!content_start) {
        LOG_ERROR("No content in Claude API response");
        http_response_free(resp);
        return E_PROTOCOL_FORMAT;
    }

    content_start += 8;  /* Skip past "text":" */
    char* content_end = strchr(content_start, '"');
    if (!content_end) {
        http_response_free(resp);
        return E_PROTOCOL_FORMAT;
    }

    /* Copy content */
    size_t content_len = content_end - content_start;
    result = ensure_buffer_capacity(&ctx->response_content, &ctx->response_capacity,
                                   content_len + 1);
    if (result != ARGO_SUCCESS) {
        http_response_free(resp);
        return result;
    }

    strncpy(ctx->response_content, content_start, content_len);
    ctx->response_content[content_len] = '\0';

    http_response_free(resp);

    /* Build response */
    ci_response_t response;
    build_ci_response(&response, true, ARGO_SUCCESS,
                     ctx->response_content, ctx->model);

    /* Update statistics */
    ARGO_UPDATE_STATS(ctx);

    /* Invoke callback */
    callback(&response, userdata);

    return ARGO_SUCCESS;
}

/* Helper for streaming wrapper */
static void stream_wrapper_callback(const ci_response_t* resp, void* ud) {
    struct { ci_stream_callback cb; void* ud; } *sctx = ud;
    if (resp->success && resp->content) {
        sctx->cb(resp->content, strlen(resp->content), sctx->ud);
    }
}

/* Stream from Claude API */
static int claude_api_stream(ci_provider_t* provider, const char* prompt,
                            ci_stream_callback callback, void* userdata) {
    /* For now, just use non-streaming and send as one chunk */
    ARGO_CHECK_NULL(prompt);
    ARGO_CHECK_NULL(callback);
    ARGO_GET_CONTEXT(provider, claude_api_context_t, ctx);

    /* Use query and wrap in streaming callback */
    struct {
        ci_stream_callback callback;
        void* userdata;
    } stream_ctx = { callback, userdata };

    return claude_api_query(provider, prompt, stream_wrapper_callback, &stream_ctx);
}

/* Cleanup */
static void claude_api_cleanup(ci_provider_t* provider) {
    if (!provider) return;

    claude_api_context_t* ctx = (claude_api_context_t*)provider->context;
    if (!ctx) return;

    free(ctx->response_content);
    LOG_INFO("Claude API cleanup: queries=%llu", ctx->total_queries);
    free(ctx);
}

/* Check availability */
bool claude_api_is_available(void) {
    return strlen(ANTHROPIC_API_KEY) > 10;
}
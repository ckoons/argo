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

/* Gemini API context */
typedef struct {
    char model[64];
    char* response_content;
    size_t response_capacity;
    uint64_t total_queries;
    time_t last_query;
    ci_provider_t provider;
} gemini_api_context_t;

/* Static functions */
static int gemini_api_init(ci_provider_t* provider);
static int gemini_api_connect(ci_provider_t* provider);
static int gemini_api_query(ci_provider_t* provider, const char* prompt,
                           ci_response_callback callback, void* userdata);
static int gemini_api_stream(ci_provider_t* provider, const char* prompt,
                            ci_stream_callback callback, void* userdata);
static void gemini_api_cleanup(ci_provider_t* provider);

/* Create Gemini API provider */
ci_provider_t* gemini_api_create_provider(const char* model) {
    gemini_api_context_t* ctx = calloc(1, sizeof(gemini_api_context_t));
    if (!ctx) {
        LOG_ERROR("Failed to allocate Gemini API context");
        return NULL;
    }

    /* Setup provider interface */
    init_provider_base(&ctx->provider, ctx, gemini_api_init, gemini_api_connect,
                      gemini_api_query, gemini_api_stream, gemini_api_cleanup);

    /* Set model */
    strncpy(ctx->model, model ? model : GEMINI_DEFAULT_MODEL, sizeof(ctx->model) - 1);

    /* Set provider info */
    strncpy(ctx->provider.name, "gemini-api", sizeof(ctx->provider.name) - 1);
    strncpy(ctx->provider.model, ctx->model, sizeof(ctx->provider.model) - 1);
    ctx->provider.supports_streaming = true;
    ctx->provider.supports_memory = false;
    ctx->provider.max_context = 2097152;  /* Gemini 1.5 has 2M context */

    LOG_INFO("Created Gemini API provider for model %s", ctx->model);
    return &ctx->provider;
}

/* Initialize */
static int gemini_api_init(ci_provider_t* provider) {
    ARGO_GET_CONTEXT(provider, gemini_api_context_t, ctx);

    /* Allocate response buffer */
    ctx->response_capacity = 65536;
    ctx->response_content = malloc(ctx->response_capacity);
    if (!ctx->response_content) {
        return E_SYSTEM_MEMORY;
    }

    return http_init();
}

/* Connect (no-op for API) */
static int gemini_api_connect(ci_provider_t* provider) {
    (void)provider;
    return ARGO_SUCCESS;
}

/* Query Gemini API */
static int gemini_api_query(ci_provider_t* provider, const char* prompt,
                           ci_response_callback callback, void* userdata) {
    ARGO_CHECK_NULL(prompt);
    ARGO_CHECK_NULL(callback);
    ARGO_GET_CONTEXT(provider, gemini_api_context_t, ctx);

    /* Build Gemini-specific JSON format */
    char json_body[8192];
    snprintf(json_body, sizeof(json_body),
            "{"
            "\"contents\":[{"
            "\"parts\":[{"
            "\"text\":\"%s\""
            "}]"
            "}],"
            "\"generationConfig\":{"
            "\"maxOutputTokens\":4096,"
            "\"temperature\":0.7"
            "}"
            "}",
            prompt);

    /* Build URL with model and API key */
    char url[512];
    snprintf(url, sizeof(url), "%s/%s:generateContent?key=%s",
            GEMINI_API_URL, ctx->model, GEMINI_API_KEY);

    /* Create HTTP request */
    http_request_t* req = http_request_new(HTTP_POST, url);
    http_request_add_header(req, "Content-Type", "application/json");
    http_request_set_body(req, json_body, strlen(json_body));

    /* Execute request */
    http_response_t* resp = NULL;
    int result = http_execute(req, &resp);
    http_request_free(req);

    if (result != ARGO_SUCCESS) {
        LOG_ERROR("Failed to execute Gemini API request");
        return result;
    }

    if (resp->status_code != 200) {
        LOG_ERROR("Gemini API returned status %d: %s", resp->status_code, resp->body);
        http_response_free(resp);
        return E_PROTOCOL_HTTP;
    }

    /* Extract content from Gemini response format */
    char* content_start = strstr(resp->body, "\"text\":\"");
    if (!content_start) {
        LOG_ERROR("No text in Gemini API response");
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

/* Stream from Gemini API */
static int gemini_api_stream(ci_provider_t* provider, const char* prompt,
                            ci_stream_callback callback, void* userdata) {
    /* For now, use non-streaming */
    ARGO_CHECK_NULL(prompt);
    ARGO_CHECK_NULL(callback);

    struct {
        ci_stream_callback callback;
        void* userdata;
    } stream_ctx = { callback, userdata };

    return gemini_api_query(provider, prompt, stream_wrapper_callback, &stream_ctx);
}

/* Cleanup */
static void gemini_api_cleanup(ci_provider_t* provider) {
    if (!provider) return;

    gemini_api_context_t* ctx = (gemini_api_context_t*)provider->context;
    if (!ctx) return;

    free(ctx->response_content);
    LOG_INFO("Gemini API cleanup: queries=%llu", ctx->total_queries);
    free(ctx);
}

/* Check availability */
bool gemini_api_is_available(void) {
    return strlen(GEMINI_API_KEY) > 10;
}
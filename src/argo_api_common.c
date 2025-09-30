/* © 2025 Casey Koons All rights reserved */

/* Common API provider implementation for Grok, DeepSeek, and OpenRouter */
/* These all use OpenAI-compatible APIs */

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

/* Shared API context for OpenAI-compatible providers */
typedef struct {
    char model[64];
    char api_url[256];
    char api_key[256];
    char provider_name[32];
    char* response_content;
    size_t response_capacity;
    uint64_t total_queries;
    time_t last_query;
    ci_provider_t provider;
} api_context_t;

/* Forward declarations */
static int api_init(ci_provider_t* provider);
static int api_connect(ci_provider_t* provider);
static int api_query(ci_provider_t* provider, const char* prompt,
                    ci_response_callback callback, void* userdata);
static int api_stream(ci_provider_t* provider, const char* prompt,
                     ci_stream_callback callback, void* userdata);
static void api_cleanup(ci_provider_t* provider);

/* Generic provider creator for OpenAI-compatible APIs */
static ci_provider_t* create_api_provider(const char* provider_name,
                                         const char* model,
                                         const char* api_url,
                                         const char* api_key,
                                         int max_context) {
    api_context_t* ctx = calloc(1, sizeof(api_context_t));
    if (!ctx) {
        LOG_ERROR("Failed to allocate API context for %s", provider_name);
        return NULL;
    }

    /* Setup provider interface */
    init_provider_base(&ctx->provider, ctx, api_init, api_connect,
                      api_query, api_stream, api_cleanup);

    /* Set configuration */
    strncpy(ctx->model, model, sizeof(ctx->model) - 1);
    strncpy(ctx->api_url, api_url, sizeof(ctx->api_url) - 1);
    strncpy(ctx->api_key, api_key, sizeof(ctx->api_key) - 1);
    strncpy(ctx->provider_name, provider_name, sizeof(ctx->provider_name) - 1);

    /* Set provider info */
    snprintf(ctx->provider.name, sizeof(ctx->provider.name), "%s-api", provider_name);
    strncpy(ctx->provider.model, ctx->model, sizeof(ctx->provider.model) - 1);
    ctx->provider.supports_streaming = true;
    ctx->provider.supports_memory = false;
    ctx->provider.max_context = max_context;

    LOG_INFO("Created %s API provider for model %s", provider_name, ctx->model);
    return &ctx->provider;
}

/* === GROK === */
ci_provider_t* grok_api_create_provider(const char* model) {
    return create_api_provider("grok",
                             model ? model : GROK_DEFAULT_MODEL,
                             GROK_API_URL,
                             GROK_API_KEY,
                             32768);  /* Grok context size */
}

bool grok_api_is_available(void) {
    return strlen(GROK_API_KEY) > 10;
}

/* === DEEPSEEK === */
ci_provider_t* deepseek_api_create_provider(const char* model) {
    return create_api_provider("deepseek",
                             model ? model : DEEPSEEK_DEFAULT_MODEL,
                             DEEPSEEK_API_URL,
                             DEEPSEEK_API_KEY,
                             128000);  /* DeepSeek context size */
}

bool deepseek_api_is_available(void) {
    return strlen(DEEPSEEK_API_KEY) > 10;
}

/* === OPENROUTER === */
ci_provider_t* openrouter_create_provider(const char* model) {
    return create_api_provider("openrouter",
                             model ? model : OPENROUTER_DEFAULT_MODEL,
                             OPENROUTER_API_URL,
                             OPENROUTER_API_KEY,
                             200000);  /* Varies by model */
}

bool openrouter_is_available(void) {
    return strlen(OPENROUTER_API_KEY) > 10;
}

/* === SHARED IMPLEMENTATION === */

/* Initialize */
static int api_init(ci_provider_t* provider) {
    ARGO_GET_CONTEXT(provider, api_context_t, ctx);

    /* Allocate response buffer */
    ctx->response_capacity = 65536;
    ctx->response_content = malloc(ctx->response_capacity);
    if (!ctx->response_content) {
        return E_SYSTEM_MEMORY;
    }

    return http_init();
}

/* Connect (no-op for API) */
static int api_connect(ci_provider_t* provider) {
    (void)provider;
    return ARGO_SUCCESS;
}

/* Query API */
static int api_query(ci_provider_t* provider, const char* prompt,
                    ci_response_callback callback, void* userdata) {
    ARGO_CHECK_NULL(prompt);
    ARGO_CHECK_NULL(callback);
    ARGO_GET_CONTEXT(provider, api_context_t, ctx);

    /* Build request JSON */
    char json_body[8192];
    snprintf(json_body, sizeof(json_body),
            "{"
            "\"model\":\"%s\","
            "\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],"
            "\"max_tokens\":4096,"
            "\"temperature\":0.7"
            "}",
            ctx->model, prompt);

    /* Create HTTP request */
    http_request_t* req = http_request_new(HTTP_POST, ctx->api_url);
    http_request_add_header(req, "Content-Type", "application/json");

    /* Add authorization header */
    char auth_header[512];
    if (strcmp(ctx->provider_name, "deepseek") == 0) {
        /* DeepSeek uses different auth format */
        snprintf(auth_header, sizeof(auth_header), "Bearer %s", ctx->api_key);
    } else {
        snprintf(auth_header, sizeof(auth_header), "Bearer %s", ctx->api_key);
    }
    http_request_add_header(req, "Authorization", auth_header);

    /* OpenRouter needs additional headers */
    if (strcmp(ctx->provider_name, "openrouter") == 0) {
        http_request_add_header(req, "HTTP-Referer", "https://github.com/cskoons/argo");
        http_request_add_header(req, "X-Title", "Argo CI System");
    }

    http_request_set_body(req, json_body, strlen(json_body));

    /* Execute request */
    http_response_t* resp = NULL;
    int result = http_execute(req, &resp);
    http_request_free(req);

    if (result != ARGO_SUCCESS) {
        LOG_ERROR("Failed to execute %s API request", ctx->provider_name);
        return result;
    }

    if (resp->status_code != 200) {
        LOG_ERROR("%s API returned status %d: %s",
                 ctx->provider_name, resp->status_code, resp->body);
        http_response_free(resp);
        return E_PROTOCOL_HTTP;
    }

    /* Extract content from response JSON */
    char* content_start = strstr(resp->body, "\"content\":\"");
    if (!content_start) {
        LOG_ERROR("No content in %s API response", ctx->provider_name);
        http_response_free(resp);
        return E_PROTOCOL_FORMAT;
    }

    content_start += 11;  /* Skip past "content":" */
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

/* Stream from API */
static int api_stream(ci_provider_t* provider, const char* prompt,
                     ci_stream_callback callback, void* userdata) {
    /* For now, use non-streaming */
    ARGO_CHECK_NULL(prompt);
    ARGO_CHECK_NULL(callback);

    struct {
        ci_stream_callback callback;
        void* userdata;
    } stream_ctx = { callback, userdata };

    return api_query(provider, prompt, stream_wrapper_callback, &stream_ctx);
}

/* Cleanup */
static void api_cleanup(ci_provider_t* provider) {
    if (!provider) return;

    api_context_t* ctx = (api_context_t*)provider->context;
    if (!ctx) return;

    free(ctx->response_content);
    LOG_INFO("%s API cleanup: queries=%llu", ctx->provider_name, ctx->total_queries);
    free(ctx);
}
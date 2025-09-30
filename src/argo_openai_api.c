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

/* OpenAI API context */
typedef struct {
    char model[API_MODEL_NAME_SIZE];
    char* response_content;
    size_t response_capacity;
    uint64_t total_queries;
    time_t last_query;
    ci_provider_t provider;
} openai_api_context_t;

/* Static functions */
static int openai_api_init(ci_provider_t* provider);
static int openai_api_connect(ci_provider_t* provider);
static int openai_api_query(ci_provider_t* provider, const char* prompt,
                           ci_response_callback callback, void* userdata);
static int openai_api_stream(ci_provider_t* provider, const char* prompt,
                            ci_stream_callback callback, void* userdata);
static void openai_api_cleanup(ci_provider_t* provider);

/* Create OpenAI API provider */
ci_provider_t* openai_api_create_provider(const char* model) {
    openai_api_context_t* ctx = calloc(1, sizeof(openai_api_context_t));
    if (!ctx) {
        LOG_ERROR("Failed to allocate OpenAI API context");
        return NULL;
    }

    /* Setup provider interface */
    init_provider_base(&ctx->provider, ctx, openai_api_init, openai_api_connect,
                      openai_api_query, openai_api_stream, openai_api_cleanup);

    /* Set model */
    strncpy(ctx->model, model ? model : OPENAI_DEFAULT_MODEL, sizeof(ctx->model) - 1);

    /* Set provider info */
    strncpy(ctx->provider.name, "openai-api", sizeof(ctx->provider.name) - 1);
    strncpy(ctx->provider.model, ctx->model, sizeof(ctx->provider.model) - 1);
    ctx->provider.supports_streaming = true;
    ctx->provider.supports_memory = false;
    ctx->provider.max_context = OPENAI_MAX_CONTEXT;

    LOG_INFO("Created OpenAI API provider for model %s", ctx->model);
    return &ctx->provider;
}

/* Initialize */
static int openai_api_init(ci_provider_t* provider) {
    ARGO_GET_CONTEXT(provider, openai_api_context_t, ctx);

    /* Allocate response buffer */
    ctx->response_capacity = API_RESPONSE_CAPACITY;
    ctx->response_content = malloc(ctx->response_capacity);
    if (!ctx->response_content) {
        return E_SYSTEM_MEMORY;
    }

    return http_init();
}

/* Connect (no-op for API) */
static int openai_api_connect(ci_provider_t* provider) {
    (void)provider;
    return ARGO_SUCCESS;
}

/* Query OpenAI API */
static int openai_api_query(ci_provider_t* provider, const char* prompt,
                           ci_response_callback callback, void* userdata) {
    ARGO_CHECK_NULL(prompt);
    ARGO_CHECK_NULL(callback);
    ARGO_GET_CONTEXT(provider, openai_api_context_t, ctx);

    /* Build request JSON */
    char json_body[API_REQUEST_BUFFER_SIZE];
    snprintf(json_body, sizeof(json_body),
            "{"
            "\"model\":\"%s\","
            "\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],"
            "\"max_tokens\":%d,"
            "\"temperature\":0.7"
            "}",
            ctx->model, prompt, API_MAX_TOKENS);

    /* Create HTTP request */
    http_request_t* req = http_request_new(HTTP_POST, OPENAI_API_URL);
    http_request_add_header(req, "Content-Type", "application/json");

    char auth_header[API_AUTH_HEADER_SIZE];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", OPENAI_API_KEY);
    http_request_add_header(req, "Authorization", auth_header);

    http_request_set_body(req, json_body, strlen(json_body));

    /* Execute request */
    http_response_t* resp = NULL;
    int result = http_execute(req, &resp);
    http_request_free(req);

    if (result != ARGO_SUCCESS) {
        LOG_ERROR("Failed to execute OpenAI API request");
        return result;
    }

    if (resp->status_code != API_HTTP_OK) {
        LOG_ERROR("OpenAI API returned status %d: %s", resp->status_code, resp->body);
        http_response_free(resp);
        return E_PROTOCOL_HTTP;
    }

    /* Extract content from response JSON - find "content" in message */
    char* message_start = strstr(resp->body, "\"message\"");
    if (!message_start) {
        LOG_ERROR("No message in OpenAI API response: %s", resp->body);
        http_response_free(resp);
        return E_PROTOCOL_FORMAT;
    }

    char* content_key = strstr(message_start, "\"content\"");
    if (!content_key) {
        LOG_ERROR("No content in OpenAI message: %s", resp->body);
        http_response_free(resp);
        return E_PROTOCOL_FORMAT;
    }

    /* Find the opening quote after "content": (skip colon and possible space) */
    char* content_start = strchr(content_key + 9, '"');
    if (!content_start) {
        http_response_free(resp);
        return E_PROTOCOL_FORMAT;
    }
    content_start++;  /* Move past the opening quote */

    /* Find closing quote */
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

/* Stream from OpenAI API */
static int openai_api_stream(ci_provider_t* provider, const char* prompt,
                            ci_stream_callback callback, void* userdata) {
    /* For now, use non-streaming */
    ARGO_CHECK_NULL(prompt);
    ARGO_CHECK_NULL(callback);

    struct {
        ci_stream_callback callback;
        void* userdata;
    } stream_ctx = { callback, userdata };

    return openai_api_query(provider, prompt, stream_wrapper_callback, &stream_ctx);
}

/* Cleanup */
static void openai_api_cleanup(ci_provider_t* provider) {
    if (!provider) return;

    openai_api_context_t* ctx = (openai_api_context_t*)provider->context;
    if (!ctx) return;

    free(ctx->response_content);
    LOG_INFO("OpenAI API cleanup: queries=%llu", ctx->total_queries);
    free(ctx);
}

/* Check availability */
bool openai_api_is_available(void) {
    return strlen(OPENAI_API_KEY) > API_KEY_MIN_LENGTH;
}
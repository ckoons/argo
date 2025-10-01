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
#include "argo_api_common.h"
#include "argo_json.h"
#include "argo_http.h"
#include "argo_error.h"
#include "argo_log.h"

/* Claude API context */
typedef struct {
    char model[API_MODEL_NAME_SIZE];
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
        argo_report_error(E_SYSTEM_MEMORY, "claude_api_create_provider", "");
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
    ctx->provider.max_context = CLAUDE_MAX_CONTEXT;

    LOG_INFO("Created Claude API provider for model %s", ctx->model);
    return &ctx->provider;
}

/* Initialize */
static int claude_api_init(ci_provider_t* provider) {
    ARGO_GET_CONTEXT(provider, claude_api_context_t, ctx);

    /* Allocate response buffer using common utility */
    int result = api_allocate_response_buffer(&ctx->response_content,
                                              &ctx->response_capacity,
                                              API_RESPONSE_CAPACITY);
    if (result != ARGO_SUCCESS) {
        return result;
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
    char json_body[API_REQUEST_BUFFER_SIZE];
    snprintf(json_body, sizeof(json_body),
            "{"
            "\"model\":\"%s\","
            "\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],"
            "\"max_tokens\":%d"
            "}",
            ctx->model, prompt, API_MAX_TOKENS);

    /* Setup authentication */
    api_auth_config_t auth = {
        .type = API_AUTH_HEADER,
        .header_name = "x-api-key",
        .value = ANTHROPIC_API_KEY
    };

    /* Extra headers for Claude */
    const char* extra_headers[] = {
        "anthropic-version", ANTHROPIC_API_VERSION,
        NULL
    };

    /* Execute HTTP request using common utility */
    http_response_t* resp = NULL;
    int result = api_http_post_json(ANTHROPIC_API_URL, json_body, &auth,
                                    extra_headers, &resp);
    if (result != ARGO_SUCCESS) {
        argo_report_error(result, "claude_api_query", "HTTP request failed");
        return result;
    }

    /* Extract content using JSON utility - navigate content[0].text */
    const char* field_path[] = { "content", "text" };
    char* extracted_content = NULL;
    size_t content_len = 0;
    result = json_extract_nested_string(resp->body, field_path, 2,
                                       &extracted_content, &content_len);

    if (result != ARGO_SUCCESS) {
        argo_report_error(result, "claude_api_query", "JSON extraction failed");
        http_response_free(resp);
        return result;
    }

    /* Ensure our buffer is large enough */
    result = ensure_buffer_capacity(&ctx->response_content, &ctx->response_capacity,
                                   content_len + 1);
    if (result != ARGO_SUCCESS) {
        free(extracted_content);
        http_response_free(resp);
        return result;
    }

    /* Copy to context buffer */
    memcpy(ctx->response_content, extracted_content, content_len);
    ctx->response_content[content_len] = '\0';
    free(extracted_content);
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

/* Stream from Claude API */
static int claude_api_stream(ci_provider_t* provider, const char* prompt,
                            ci_stream_callback callback, void* userdata) {
    /* Use common stream adapter */
    ARGO_CHECK_NULL(prompt);
    ARGO_CHECK_NULL(callback);

    return ci_query_to_stream(provider, prompt, claude_api_query,
                             callback, userdata);
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
    return strlen(ANTHROPIC_API_KEY) > API_KEY_MIN_LENGTH;
}
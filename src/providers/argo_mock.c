/* © 2025 Casey Koons All rights reserved */
/* Mock CI provider for testing workflows without real AI */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "argo_mock.h"
#include "argo_ci_common.h"
#include "argo_api_providers.h"
#include "argo_error.h"
#include "argo_log.h"

/* Mock provider context */
typedef struct {
    ci_provider_t provider;        /* Base provider (MUST be first) */
    char model[MOCK_MODEL_SIZE];   /* Model name */
    char* response_content;        /* Response buffer */
    size_t response_capacity;      /* Buffer capacity */
    char* last_prompt;             /* Last prompt received */
    char** responses;              /* Array of canned responses */
    int response_count;            /* Number of canned responses */
    int current_response_index;    /* Current response in cycle */
    uint64_t query_count;          /* Total queries */
    uint64_t total_queries;        /* For ARGO_UPDATE_STATS */
    time_t last_query;             /* For ARGO_UPDATE_STATS */
} mock_context_t;

/* Initialize mock provider */
static int mock_init(ci_provider_t* provider) {
    ARGO_CHECK_NULL(provider);
    ARGO_GET_CONTEXT(provider, mock_context_t, ctx);

    LOG_INFO("Initializing mock provider (model: %s)", ctx->model);
    return ARGO_SUCCESS;
}

/* Connect (no-op for mock) */
static int mock_connect(ci_provider_t* provider) {
    ARGO_CHECK_NULL(provider);
    LOG_DEBUG("Mock provider connected");
    return ARGO_SUCCESS;
}

/* Query mock provider */
static int mock_query(ci_provider_t* provider, const char* prompt,
                     ci_response_callback callback, void* userdata) {
    ARGO_CHECK_NULL(provider);
    ARGO_CHECK_NULL(prompt);
    ARGO_CHECK_NULL(callback);
    ARGO_GET_CONTEXT(provider, mock_context_t, ctx);

    ctx->query_count++;

    /* Save last prompt for verification */
    free(ctx->last_prompt);
    ctx->last_prompt = strdup(prompt);
    if (!ctx->last_prompt) {
        return E_SYSTEM_MEMORY;
    }

    /* Get response */
    const char* response_text = MOCK_DEFAULT_RESPONSE;

    if (ctx->responses && ctx->response_count > 0) {
        /* Use configured responses (cycle through them) */
        response_text = ctx->responses[ctx->current_response_index];
        ctx->current_response_index = (ctx->current_response_index + 1) % ctx->response_count;
    }

    /* Copy to buffer */
    size_t response_len = strlen(response_text);
    int result = ensure_buffer_capacity(&ctx->response_content,
                                       &ctx->response_capacity,
                                       response_len + 1);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    memcpy(ctx->response_content, response_text, response_len);
    ctx->response_content[response_len] = '\0';

    LOG_DEBUG("Mock query: prompt='%s' -> response='%s'", prompt, response_text);
    ARGO_UPDATE_STATS(ctx);

    /* Build response and invoke callback */
    ci_response_t response;
    build_ci_response(&response, true, ARGO_SUCCESS,
                     ctx->response_content, ctx->model);
    callback(&response, userdata);

    return ARGO_SUCCESS;
}

/* Stream (delegates to query) */
static int mock_stream(ci_provider_t* provider, const char* prompt,
                       ci_stream_callback callback, void* userdata) {
    ARGO_CHECK_NULL(provider);
    ARGO_CHECK_NULL(prompt);
    ARGO_CHECK_NULL(callback);

    /* Mock doesn't support streaming, just use query */
    return mock_query(provider, prompt, (ci_response_callback)callback, userdata);
}

/* Cleanup mock provider */
static void mock_cleanup(ci_provider_t* provider) {
    if (!provider) return;

    mock_context_t* ctx = (mock_context_t*)provider->context;
    if (!ctx) return;

    /* Free responses array */
    if (ctx->responses) {
        for (int i = 0; i < ctx->response_count; i++) {
            free(ctx->responses[i]);
        }
        free(ctx->responses);
    }

    free(ctx->response_content);
    free(ctx->last_prompt);
    free(ctx);

    LOG_DEBUG("Mock provider cleaned up");
}

/* Public API */

ci_provider_t* mock_provider_create(const char* model) {
    mock_context_t* ctx = calloc(1, sizeof(mock_context_t));
    if (!ctx) {
        argo_report_error(E_SYSTEM_MEMORY, "mock_provider_create", "allocation failed");
        return NULL;
    }

    /* Initialize provider base */
    init_provider_base(&ctx->provider, ctx, mock_init, mock_connect,
                      mock_query, mock_stream, mock_cleanup);

    /* Set model */
    strncpy(ctx->model, model ? model : MOCK_DEFAULT_MODEL, sizeof(ctx->model) - 1);

    /* Set provider metadata */
    strncpy(ctx->provider.name, MOCK_PROVIDER_NAME, sizeof(ctx->provider.name) - 1);
    strncpy(ctx->provider.model, ctx->model, sizeof(ctx->provider.model) - 1);
    ctx->provider.supports_streaming = true;
    ctx->provider.supports_memory = false;
    ctx->provider.max_context = MOCK_CONTEXT_WINDOW;

    LOG_INFO("Created mock provider for model %s", ctx->model);
    return &ctx->provider;
}

int mock_provider_set_response(ci_provider_t* provider, const char* response) {
    ARGO_CHECK_NULL(provider);
    ARGO_CHECK_NULL(response);
    ARGO_GET_CONTEXT(provider, mock_context_t, ctx);

    /* Clear existing responses */
    if (ctx->responses) {
        for (int i = 0; i < ctx->response_count; i++) {
            free(ctx->responses[i]);
        }
        free(ctx->responses);
    }

    /* Set single response */
    ctx->responses = malloc(sizeof(char*));
    if (!ctx->responses) {
        return E_SYSTEM_MEMORY;
    }

    ctx->responses[0] = strdup(response);
    if (!ctx->responses[0]) {
        free(ctx->responses);
        ctx->responses = NULL;
        return E_SYSTEM_MEMORY;
    }

    ctx->response_count = 1;
    ctx->current_response_index = 0;

    LOG_DEBUG("Mock provider: set single response");
    return ARGO_SUCCESS;
}

int mock_provider_set_responses(ci_provider_t* provider, const char** responses, int count) {
    ARGO_CHECK_NULL(provider);
    ARGO_CHECK_NULL(responses);
    ARGO_GET_CONTEXT(provider, mock_context_t, ctx);

    if (count <= 0) {
        return E_INPUT_INVALID;
    }

    /* Clear existing responses */
    if (ctx->responses) {
        for (int i = 0; i < ctx->response_count; i++) {
            free(ctx->responses[i]);
        }
        free(ctx->responses);
    }

    /* Allocate array */
    ctx->responses = malloc(sizeof(char*) * count);
    if (!ctx->responses) {
        return E_SYSTEM_MEMORY;
    }

    /* Copy responses */
    for (int i = 0; i < count; i++) {
        ctx->responses[i] = strdup(responses[i]);
        if (!ctx->responses[i]) {
            /* Cleanup on failure */
            for (int j = 0; j < i; j++) {
                free(ctx->responses[j]);
            }
            free(ctx->responses);
            ctx->responses = NULL;
            return E_SYSTEM_MEMORY;
        }
    }

    ctx->response_count = count;
    ctx->current_response_index = 0;

    LOG_DEBUG("Mock provider: set %d responses", count);
    return ARGO_SUCCESS;
}

const char* mock_provider_get_last_prompt(ci_provider_t* provider) {
    if (!provider) return NULL;

    mock_context_t* ctx = (mock_context_t*)provider->context;
    if (!ctx) return NULL;

    return ctx->last_prompt;
}

int mock_provider_get_query_count(ci_provider_t* provider) {
    if (!provider) return 0;

    mock_context_t* ctx = (mock_context_t*)provider->context;
    if (!ctx) return 0;

    return (int)ctx->query_count;
}

/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Project includes */
#include "argo_ci.h"
#include "argo_ci_common.h"
#include "argo_api_common.h"
#include "argo_api_providers.h"
#include "argo_json.h"
#include "argo_http.h"
#include "argo_error.h"
#include "argo_error_messages.h"
#include "argo_log.h"

/* Static functions */
static int generic_api_init(ci_provider_t* provider);
static int generic_api_connect(ci_provider_t* provider);
static int generic_api_query(ci_provider_t* provider, const char* prompt,
                            ci_response_callback callback, void* userdata);
static int generic_api_stream(ci_provider_t* provider, const char* prompt,
                             ci_stream_callback callback, void* userdata);
static void generic_api_cleanup(ci_provider_t* provider);

/* Create generic API provider */
ci_provider_t* generic_api_create_provider(const api_provider_config_t* config,
                                           const char* model) {
    if (!config) {
        argo_report_error(E_INPUT_NULL, "generic_api_create_provider", ERR_MSG_CONFIG_IS_NULL);
        return NULL;
    }

    generic_api_context_t* ctx = calloc(1, sizeof(generic_api_context_t));
    if (!ctx) {
        argo_report_error(E_SYSTEM_MEMORY, "generic_api_create_provider", ERR_MSG_MEMORY_ALLOC_FAILED);
        return NULL;
    }

    /* Setup provider interface */
    init_provider_base(&ctx->provider, ctx, generic_api_init, generic_api_connect,
                      generic_api_query, generic_api_stream, generic_api_cleanup);

    /* Store config reference */
    ctx->config = config;

    /* Set model */
    strncpy(ctx->model, model ? model : config->default_model, sizeof(ctx->model) - 1);

    /* Set provider info from config */
    strncpy(ctx->provider.name, config->provider_name, sizeof(ctx->provider.name) - 1);
    strncpy(ctx->provider.model, ctx->model, sizeof(ctx->provider.model) - 1);
    ctx->provider.supports_streaming = config->supports_streaming;
    ctx->provider.supports_memory = true;  /* Generic providers now support memory */
    ctx->provider.max_context = config->max_context;

    /* Initialize memory to NULL */
    ctx->memory = NULL;

    LOG_INFO("Created %s provider for model %s", config->provider_name, ctx->model);
    return &ctx->provider;
}

/* Set memory digest for provider */
int generic_api_set_memory(ci_provider_t* provider, ci_memory_digest_t* memory) {
    ARGO_CHECK_NULL(provider);

    /* Verify this is a generic API provider by checking context type */
    if (!provider->context) {
        argo_report_error(E_INPUT_NULL, "generic_api_set_memory", "Provider context is NULL");
        return E_INPUT_NULL;
    }

    generic_api_context_t* ctx = (generic_api_context_t*)provider->context;
    ctx->memory = memory;

    LOG_DEBUG("Set memory digest for %s provider", provider->name);
    return ARGO_SUCCESS;
}

/* Initialize */
static int generic_api_init(ci_provider_t* provider) {
    ARGO_GET_CONTEXT(provider, generic_api_context_t, ctx);

    /* Allocate response buffer */
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
static int generic_api_connect(ci_provider_t* provider) {
    (void)provider;
    return ARGO_SUCCESS;
}

/* Query generic API */
static int generic_api_query(ci_provider_t* provider, const char* prompt,
                            ci_response_callback callback, void* userdata) {
    ARGO_CHECK_NULL(prompt);
    ARGO_CHECK_NULL(callback);
    ARGO_GET_CONTEXT(provider, generic_api_context_t, ctx);

    const api_provider_config_t* cfg = ctx->config;

    /* Augment prompt with memory if available */
    char* augmented_prompt = NULL;
    const char* final_prompt = prompt;

    if (ctx->memory) {
        int result = api_augment_prompt_with_memory(ctx->memory, prompt, &augmented_prompt);
        if (result == ARGO_SUCCESS && augmented_prompt) {
            final_prompt = augmented_prompt;
            LOG_DEBUG("Augmented prompt with memory context");
        }
        /* If augmentation fails, fall back to original prompt */
    }

    /* Build request JSON using provider-specific builder */
    char json_body[API_REQUEST_BUFFER_SIZE];
    int json_len = cfg->build_request(json_body, sizeof(json_body),
                                      ctx->model, final_prompt);
    if (json_len < 0) {
        free(augmented_prompt);
        argo_report_error(E_PROTOCOL_FORMAT, "generic_api_query", ERR_MSG_JSON_BUILD_FAILED);
        return E_PROTOCOL_FORMAT;
    }

    /* Build URL (append model if needed, like Gemini) */
    char url[API_URL_SIZE];
    if (cfg->url_includes_model) {
        snprintf(url, sizeof(url), "%s/%s:generateContent", cfg->api_url, ctx->model);
    } else {
        strncpy(url, cfg->api_url, sizeof(url) - 1);
        url[sizeof(url) - 1] = '\0';
    }

    /* Execute HTTP request */
    http_response_t* resp = NULL;
    int result = api_http_post_json(url, json_body, &cfg->auth,
                                    cfg->extra_headers, &resp);
    if (result != ARGO_SUCCESS) {
        argo_report_error(result, "generic_api_query", ERR_MSG_HTTP_REQUEST_FAILED);
        free(augmented_prompt);
        return result;
    }

    /* Extract content using provider-specific JSON path */
    char* extracted_content = NULL;
    size_t content_len = 0;
    result = json_extract_nested_string(resp->body, cfg->response_path,
                                       cfg->response_path_depth,
                                       &extracted_content, &content_len);

    if (result != ARGO_SUCCESS) {
        argo_report_error(result, "generic_api_query", ERR_MSG_JSON_EXTRACT_FAILED);
        http_response_free(resp);
        free(augmented_prompt);
        return result;
    }

    /* Ensure our buffer is large enough */
    result = ensure_buffer_capacity(&ctx->response_content, &ctx->response_capacity,
                                   content_len + 1);
    if (result != ARGO_SUCCESS) {
        free(extracted_content);
        http_response_free(resp);
        free(augmented_prompt);
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

    /* Free augmented prompt if allocated */
    free(augmented_prompt);

    return ARGO_SUCCESS;
}

/* Stream from generic API */
static int generic_api_stream(ci_provider_t* provider, const char* prompt,
                             ci_stream_callback callback, void* userdata) {
    ARGO_CHECK_NULL(prompt);
    ARGO_CHECK_NULL(callback);

    return ci_query_to_stream(provider, prompt, generic_api_query,
                             callback, userdata);
}

/* Cleanup */
static void generic_api_cleanup(ci_provider_t* provider) {
    if (!provider || !provider->context) return;

    generic_api_context_t* ctx = (generic_api_context_t*)provider->context;

    free(ctx->response_content);
    LOG_INFO("%s cleanup: queries=%llu", ctx->config->provider_name, ctx->total_queries);
    free(ctx);
}

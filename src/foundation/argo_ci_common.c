/* © 2025 Casey Koons All rights reserved */

/* CI provider common utilities - shared routines for all providers */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Project includes */
#include "argo_ci_common.h"
#include "argo_error.h"
#include "argo_log.h"
#include "argo_limits.h"

/* Initialize provider statistics */
void provider_stats_init(provider_stats_t* stats) {
    if (stats) {
        memset(stats, 0, sizeof(*stats));
        stats->first_query = time(NULL);
    }
}

/* Update provider statistics after query */
void provider_stats_update(provider_stats_t* stats, uint64_t tokens) {
    if (stats) {
        stats->total_queries++;
        stats->total_tokens += tokens;
        stats->last_query = time(NULL);
    }
}

/* Ensure buffer capacity */
int ensure_buffer_capacity(char** buffer, size_t* capacity, size_t required) {
    if (required >= *capacity) {
        size_t new_capacity = required + BUFFER_HEADROOM;
        char* new_buffer = realloc(*buffer, new_capacity);
        if (!new_buffer) {
            argo_report_error(E_SYSTEM_MEMORY, "ensure_buffer_capacity",
                            "failed to realloc %zu bytes", new_capacity);
            return E_SYSTEM_MEMORY;
        }
        *buffer = new_buffer;
        *capacity = new_capacity;
    }
    return ARGO_SUCCESS;
}

/* Extract JSON string field */
int extract_json_string(const char* json, const char* field,
                       char* output, size_t output_size) {
    char search[ARGO_BUFFER_MEDIUM];
    snprintf(search, sizeof(search), "\"%s\":\"", field);

    char* start = strstr(json, search);
    if (!start) {
        return -1;
    }

    start += strlen(search);
    char* end = start;

    /* Find closing quote, handling escapes */
    while (*end) {
        if (*end == '"' && *(end-1) != '\\') {
            break;
        }
        end++;
    }

    size_t len = end - start;
    if (len >= output_size) {
        len = output_size - 1;
    }

    strncpy(output, start, len);
    output[len] = '\0';

    return len;
}

/* Initialize provider base */
void init_provider_base(ci_provider_t* provider,
                       void* context,
                       int (*init)(ci_provider_t*),
                       int (*connect)(ci_provider_t*),
                       int (*query)(ci_provider_t*, const char*,
                                   ci_response_callback, void*),
                       int (*stream)(ci_provider_t*, const char*,
                                    ci_stream_callback, void*),
                       void (*cleanup)(ci_provider_t*)) {
    provider->context = context;
    provider->init = init;
    provider->connect = connect;
    provider->query = query;
    provider->stream = stream;
    provider->cleanup = cleanup;
}

/* Build response helper */
void build_ci_response(ci_response_t* response, bool success,
                      int error_code, const char* content,
                      const char* model) {
    response->success = success;
    response->error_code = error_code;
    response->content = (char*)content;
    response->model_used = (char*)model;
    response->timestamp = time(NULL);
}

/* Stream wrapper callback - converts query response to stream */
void ci_stream_wrapper_callback(const ci_response_t* resp, void* ud) {
    stream_wrapper_context_t* sctx = (stream_wrapper_context_t*)ud;
    if (resp->success && resp->content) {
        sctx->callback(resp->content, strlen(resp->content), sctx->userdata);
    }
}

/* Adapt query function to stream interface */
int ci_query_to_stream(ci_provider_t* provider, const char* prompt,
                       ci_query_func query_fn,
                       ci_stream_callback callback, void* userdata) {
    stream_wrapper_context_t stream_ctx = { callback, userdata };
    return query_fn(provider, prompt, ci_stream_wrapper_callback, &stream_ctx);
}

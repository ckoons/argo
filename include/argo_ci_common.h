/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_CI_COMMON_H
#define ARGO_CI_COMMON_H

#include "argo_ci.h"
#include "argo_error.h"
#include "argo_log.h"

/* Combined null check and context extraction */
#define ARGO_GET_CONTEXT(provider, ctx_type, ctx_var) \
    ARGO_CHECK_NULL(provider); \
    ctx_type* ctx_var = (ctx_type*)provider->context; \
    ARGO_CHECK_NULL(ctx_var)

/* Ensure connection helper */
#define ARGO_ENSURE_CONNECTED(provider, ctx) \
    do { \
        if (!ctx->connected) { \
            int _result = provider->connect(provider); \
            if (_result != ARGO_SUCCESS) { \
                return _result; \
            } \
        } \
    } while(0)

/* Provider statistics structure */
typedef struct {
    uint64_t total_queries;
    uint64_t total_tokens;      /* Optional - 0 if not tracked */
    time_t last_query;
    time_t first_query;
} provider_stats_t;

/* Initialize provider statistics */
static inline void provider_stats_init(provider_stats_t* stats) {
    if (stats) {
        memset(stats, 0, sizeof(*stats));
        stats->first_query = time(NULL);
    }
}

/* Update provider statistics after query */
static inline void provider_stats_update(provider_stats_t* stats, uint64_t tokens) {
    if (stats) {
        stats->total_queries++;
        stats->total_tokens += tokens;
        stats->last_query = time(NULL);
    }
}

/* Legacy macro for backward compatibility */
#define ARGO_UPDATE_STATS(ctx) \
    do { \
        ctx->total_queries++; \
        ctx->last_query = time(NULL); \
    } while(0)

/* Allocate buffer with capacity tracking */
#define ARGO_ALLOC_BUFFER(ctx_ptr, buffer_field, capacity_field, size) \
    do { \
        (ctx_ptr)->buffer_field = malloc(size); \
        if (!(ctx_ptr)->buffer_field) return E_SYSTEM_MEMORY; \
        (ctx_ptr)->capacity_field = (size); \
    } while(0)

/* Ensure buffer capacity */
static inline int ensure_buffer_capacity(char** buffer, size_t* capacity, size_t required) {
    if (required >= *capacity) {
        size_t new_capacity = required + 1024;  /* Add some headroom */
        char* new_buffer = realloc(*buffer, new_capacity);
        if (!new_buffer) {
            return E_SYSTEM_MEMORY;
        }
        *buffer = new_buffer;
        *capacity = new_capacity;
    }
    return ARGO_SUCCESS;
}

/* Extract JSON string field */
static inline int extract_json_string(const char* json, const char* field,
                                      char* output, size_t output_size) {
    char search[256];
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
static inline void init_provider_base(ci_provider_t* provider,
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
static inline void build_ci_response(ci_response_t* response, bool success,
                                     int error_code, const char* content,
                                     const char* model) {
    response->success = success;
    response->error_code = error_code;
    response->content = (char*)content;
    response->model_used = (char*)model;
    response->timestamp = time(NULL);
}

/* Stream wrapper callback context */
typedef struct {
    ci_stream_callback callback;
    void* userdata;
} stream_wrapper_context_t;

/* Stream wrapper callback - converts query response to stream */
static inline void ci_stream_wrapper_callback(const ci_response_t* resp, void* ud) {
    stream_wrapper_context_t* sctx = (stream_wrapper_context_t*)ud;
    if (resp->success && resp->content) {
        sctx->callback(resp->content, strlen(resp->content), sctx->userdata);
    }
}

/* Adapt query function to stream interface */
typedef int (*ci_query_func)(ci_provider_t*, const char*, ci_response_callback, void*);

static inline int ci_query_to_stream(ci_provider_t* provider, const char* prompt,
                                     ci_query_func query_fn,
                                     ci_stream_callback callback, void* userdata) {
    stream_wrapper_context_t stream_ctx = { callback, userdata };
    return query_fn(provider, prompt, ci_stream_wrapper_callback, &stream_ctx);
}

#endif /* ARGO_CI_COMMON_H */
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
void provider_stats_init(provider_stats_t* stats);

/* Update provider statistics after query */
void provider_stats_update(provider_stats_t* stats, uint64_t tokens);

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
        if (!(ctx_ptr)->buffer_field) { \
            argo_report_error(E_SYSTEM_MEMORY, __func__, "failed to allocate %zu bytes", (size_t)(size)); \
            return E_SYSTEM_MEMORY; \
        } \
        (ctx_ptr)->capacity_field = (size); \
    } while(0)

/* Ensure buffer capacity */
#define BUFFER_HEADROOM 1024
int ensure_buffer_capacity(char** buffer, size_t* capacity, size_t required);

/* Extract JSON string field */
int extract_json_string(const char* json, const char* field,
                       char* output, size_t output_size);

/* Initialize provider base */
void init_provider_base(ci_provider_t* provider,
                       void* context,
                       int (*init)(ci_provider_t*),
                       int (*connect)(ci_provider_t*),
                       int (*query)(ci_provider_t*, const char*,
                                   ci_response_callback, void*),
                       int (*stream)(ci_provider_t*, const char*,
                                    ci_stream_callback, void*),
                       void (*cleanup)(ci_provider_t*));

/* Build response helper */
void build_ci_response(ci_response_t* response, bool success,
                      int error_code, const char* content,
                      const char* model);

/* Stream wrapper callback context */
typedef struct {
    ci_stream_callback callback;
    void* userdata;
} stream_wrapper_context_t;

/* Stream wrapper callback - converts query response to stream */
void ci_stream_wrapper_callback(const ci_response_t* resp, void* ud);

/* Adapt query function to stream interface */
typedef int (*ci_query_func)(ci_provider_t*, const char*, ci_response_callback, void*);

int ci_query_to_stream(ci_provider_t* provider, const char* prompt,
                      ci_query_func query_fn,
                      ci_stream_callback callback, void* userdata);

#endif /* ARGO_CI_COMMON_H */
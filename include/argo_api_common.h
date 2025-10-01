/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_API_COMMON_H
#define ARGO_API_COMMON_H

#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include "argo_ci.h"
#include "argo_http.h"

/* API authentication types */
typedef enum {
    API_AUTH_BEARER,        /* Authorization: Bearer <token> */
    API_AUTH_HEADER,        /* Custom header: value */
    API_AUTH_URL_PARAM      /* Append ?key=<value> to URL */
} api_auth_type_t;

/* API authentication configuration */
typedef struct {
    api_auth_type_t type;
    const char* header_name;    /* For API_AUTH_HEADER */
    const char* param_name;     /* For API_AUTH_URL_PARAM */
    const char* value;          /* API key/token value */
} api_auth_config_t;

/* Execute HTTP POST with JSON body and authentication
 *
 * Parameters:
 *   base_url - Base URL for request
 *   json_body - JSON string to send as body
 *   auth - Authentication configuration (NULL for no auth)
 *   extra_headers - Additional headers as key:value pairs, NULL-terminated array
 *   response - Pointer to receive response (caller must free with http_response_free)
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_PROTOCOL_HTTP on HTTP error
 *   Other error codes from http subsystem
 */
int api_http_post_json(const char* base_url, const char* json_body,
                       const api_auth_config_t* auth,
                       const char** extra_headers,
                       http_response_t** response);

/* Allocate response buffer for provider
 *
 * Parameters:
 *   buffer - Pointer to buffer pointer (will be allocated)
 *   capacity - Pointer to capacity variable (will be set)
 *   size - Requested buffer size
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_SYSTEM_MEMORY on allocation failure
 */
int api_allocate_response_buffer(char** buffer, size_t* capacity, size_t size);

/* JSON request builder function pointer type
 * Returns: Number of bytes written to json_body, or negative on error
 */
typedef int (*json_request_builder_t)(char* json_body, size_t buffer_size,
                                      const char* model, const char* prompt);

/* Generic API provider configuration */
typedef struct {
    const char* provider_name;
    const char* default_model;
    const char* api_url;
    bool url_includes_model;        /* True if URL needs model appended (Gemini) */
    api_auth_config_t auth;
    const char** extra_headers;     /* NULL-terminated key-value pairs */
    const char** response_path;     /* JSON field path for content extraction */
    int response_path_depth;
    json_request_builder_t build_request;
    bool supports_streaming;
    int max_context;
} api_provider_config_t;

/* Generic API provider context */
typedef struct {
    char model[64];  /* API_MODEL_NAME_SIZE from argo_api_providers.h */
    char* response_content;
    size_t response_capacity;
    uint64_t total_queries;
    time_t last_query;
    ci_provider_t provider;
    const api_provider_config_t* config;  /* Provider-specific configuration */
} generic_api_context_t;

/* Create generic API provider from configuration
 *
 * Parameters:
 *   config - Provider configuration (must remain valid for lifetime)
 *   model - Model name override (NULL to use config default)
 *
 * Returns:
 *   Pointer to provider interface, or NULL on error
 */
ci_provider_t* generic_api_create_provider(const api_provider_config_t* config,
                                           const char* model);

#endif /* ARGO_API_COMMON_H */

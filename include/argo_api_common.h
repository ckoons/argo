/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_API_COMMON_H
#define ARGO_API_COMMON_H

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

#endif /* ARGO_API_COMMON_H */

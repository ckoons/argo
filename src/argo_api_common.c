/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Project includes */
#include "argo_api_common.h"
#include "argo_api_providers.h"
#include "argo_error.h"
#include "argo_log.h"

/* Execute HTTP POST with JSON and authentication */
int api_http_post_json(const char* base_url, const char* json_body,
                       const api_auth_config_t* auth,
                       const char** extra_headers,
                       http_response_t** response) {
    ARGO_CHECK_NULL(base_url);
    ARGO_CHECK_NULL(json_body);
    ARGO_CHECK_NULL(response);

    /* Build URL with authentication if needed */
    char url[API_URL_SIZE];
    if (auth && auth->type == API_AUTH_URL_PARAM) {
        snprintf(url, sizeof(url), "%s?%s=%s",
                base_url, auth->param_name, auth->value);
    } else {
        strncpy(url, base_url, sizeof(url) - 1);
        url[sizeof(url) - 1] = '\0';
    }

    /* Create request */
    http_request_t* req = http_request_new(HTTP_POST, url);
    if (!req) {
        return E_SYSTEM_MEMORY;
    }

    /* Add standard headers */
    http_request_add_header(req, "Content-Type", "application/json");

    /* Add authentication header if needed */
    if (auth) {
        if (auth->type == API_AUTH_BEARER) {
            char auth_header[API_AUTH_HEADER_SIZE];
            snprintf(auth_header, sizeof(auth_header), "Bearer %s", auth->value);
            http_request_add_header(req, "Authorization", auth_header);
        } else if (auth->type == API_AUTH_HEADER) {
            http_request_add_header(req, auth->header_name, auth->value);
        }
    }

    /* Add extra headers */
    if (extra_headers) {
        for (int i = 0; extra_headers[i]; i += 2) {
            if (extra_headers[i + 1]) {
                http_request_add_header(req, extra_headers[i], extra_headers[i + 1]);
            }
        }
    }

    /* Set body */
    http_request_set_body(req, json_body, strlen(json_body));

    /* Execute request */
    int result = http_execute(req, response);
    http_request_free(req);

    if (result != ARGO_SUCCESS) {
        LOG_ERROR("HTTP POST failed: %s", argo_error_string(result));
        return result;
    }

    /* Check HTTP status */
    if ((*response)->status_code != API_HTTP_OK) {
        LOG_ERROR("API returned status %d: %s",
                 (*response)->status_code, (*response)->body);
        return E_PROTOCOL_HTTP;
    }

    return ARGO_SUCCESS;
}

/* Allocate response buffer */
int api_allocate_response_buffer(char** buffer, size_t* capacity, size_t size) {
    ARGO_CHECK_NULL(buffer);
    ARGO_CHECK_NULL(capacity);

    *buffer = malloc(size);
    if (!*buffer) {
        LOG_ERROR("Failed to allocate response buffer of size %zu", size);
        return E_SYSTEM_MEMORY;
    }

    *capacity = size;
    return ARGO_SUCCESS;
}

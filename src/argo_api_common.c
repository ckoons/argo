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
        argo_report_error(result, "api_http_post_json", "HTTP POST failed");
        return result;
    }

    /* Check HTTP status and map to specific error codes */
    int status = (*response)->status_code;
    if (status != API_HTTP_OK) {
        int error_code = E_PROTOCOL_HTTP;  /* Default */
        const char* status_desc = "Unknown";

        /* Map HTTP status codes to specific errors */
        switch (status) {
            case HTTP_STATUS_BAD_REQUEST:
                error_code = E_HTTP_BAD_REQUEST;
                status_desc = "Bad Request";
                break;
            case HTTP_STATUS_UNAUTHORIZED:
                error_code = E_HTTP_UNAUTHORIZED;
                status_desc = "Unauthorized";
                break;
            case HTTP_STATUS_FORBIDDEN:
                error_code = E_HTTP_FORBIDDEN;
                status_desc = "Forbidden";
                break;
            case HTTP_STATUS_NOT_FOUND:
                error_code = E_HTTP_NOT_FOUND;
                status_desc = "Not Found";
                break;
            case HTTP_STATUS_RATE_LIMIT:
                error_code = E_HTTP_RATE_LIMIT;
                status_desc = "Rate Limit Exceeded";
                break;
            default:
                if (status >= HTTP_STATUS_SERVER_ERROR) {
                    error_code = E_HTTP_SERVER_ERROR;
                    status_desc = "Server Error";
                }
                break;
        }

        argo_report_error(error_code, "api_http_post_json", "HTTP %d (%s)", status, status_desc);
        return error_code;
    }

    return ARGO_SUCCESS;
}

/* Allocate response buffer */
int api_allocate_response_buffer(char** buffer, size_t* capacity, size_t size) {
    ARGO_CHECK_NULL(buffer);
    ARGO_CHECK_NULL(capacity);

    *buffer = malloc(size);
    if (!*buffer) {
        argo_report_error(E_SYSTEM_MEMORY, "api_allocate_response_buffer", "size %zu", size);
        return E_SYSTEM_MEMORY;
    }

    *capacity = size;
    return ARGO_SUCCESS;
}

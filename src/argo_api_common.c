/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Project includes */
#include "argo_api_common.h"
#include "argo_api_providers.h"
#include "argo_error.h"
#include "argo_error_messages.h"
#include "argo_log.h"
#include "argo_memory.h"
#include "argo_limits.h"

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

/* Augment prompt with memory context */
int api_augment_prompt_with_memory(ci_memory_digest_t* memory_digest,
                                   const char* prompt,
                                   char** out_augmented) {
    int result = ARGO_SUCCESS;
    char* augmented = NULL;

    ARGO_CHECK_NULL(prompt);
    ARGO_CHECK_NULL(out_augmented);

    /* If no memory digest, just return a copy of the prompt */
    if (!memory_digest) {
        augmented = strdup(prompt);
        if (!augmented) {
            result = E_SYSTEM_MEMORY;
            goto cleanup;
        }
        *out_augmented = augmented;
        return ARGO_SUCCESS;
    }

    /* Calculate size needed for augmented prompt */
    size_t memory_context_size = 0;
    size_t prompt_size = strlen(prompt);

    /* Add space for sunset notes if present */
    if (memory_digest->sunset_notes) {
        memory_context_size += strlen(memory_digest->sunset_notes) + MEMORY_NOTES_PADDING;
    }

    /* Add space for sunrise brief if present */
    if (memory_digest->sunrise_brief) {
        memory_context_size += strlen(memory_digest->sunrise_brief) + MEMORY_NOTES_PADDING;
    }

    /* Add space for breadcrumbs (estimate) */
    if (memory_digest->breadcrumb_count > 0) {
        memory_context_size += memory_digest->breadcrumb_count * MEMORY_BREADCRUMB_SIZE;
    }

    /* Add space for selected memories (estimate) */
    if (memory_digest->selected_count > 0) {
        memory_context_size += memory_digest->selected_count * MEMORY_SELECTED_SIZE;
    }

    /* Allocate buffer with overhead */
    size_t total_size = prompt_size + memory_context_size + MEMORY_BUFFER_OVERHEAD;
    augmented = malloc(total_size);
    if (!augmented) {
        argo_report_error(E_SYSTEM_MEMORY, "api_augment_prompt_with_memory",
                         ERR_MSG_MEMORY_ALLOC_FAILED);
        result = E_SYSTEM_MEMORY;
        goto cleanup;
    }

    /* Build augmented prompt */
    char* pos = augmented;
    size_t remaining = total_size;
    int written = 0;

    /* Add sunset/sunrise context if available */
    if (memory_digest->sunset_notes) {
        written = snprintf(pos, remaining,
                          "## Previous Session Summary\n%s\n\n",
                          memory_digest->sunset_notes);
        pos += written;
        remaining -= written;
    }

    if (memory_digest->sunrise_brief) {
        written = snprintf(pos, remaining,
                          "## Session Context\n%s\n\n",
                          memory_digest->sunrise_brief);
        pos += written;
        remaining -= written;
    }

    /* Add breadcrumbs if present */
    if (memory_digest->breadcrumb_count > 0) {
        written = snprintf(pos, remaining, "## Progress Breadcrumbs\n");
        pos += written;
        remaining -= written;

        for (int i = 0; i < memory_digest->breadcrumb_count && remaining > 0; i++) {
            written = snprintf(pos, remaining, "- %s\n",
                             memory_digest->breadcrumbs[i]);
            pos += written;
            remaining -= written;
        }
        written = snprintf(pos, remaining, "\n");
        pos += written;
        remaining -= written;
    }

    /* Add selected memories if present */
    if (memory_digest->selected_count > 0) {
        written = snprintf(pos, remaining, "## Relevant Context\n");
        pos += written;
        remaining -= written;

        for (int i = 0; i < memory_digest->selected_count && remaining > 0; i++) {
            memory_item_t* item = memory_digest->selected[i];
            if (item && item->content) {
                const char* type_name = "Unknown";
                switch (item->type) {
                    case MEMORY_TYPE_DECISION: type_name = "Decision"; break;
                    case MEMORY_TYPE_APPROACH: type_name = "Approach"; break;
                    case MEMORY_TYPE_ERROR: type_name = "Error"; break;
                    case MEMORY_TYPE_SUCCESS: type_name = "Success"; break;
                    case MEMORY_TYPE_FACT: type_name = "Fact"; break;
                    case MEMORY_TYPE_RELATIONSHIP: type_name = "Relationship"; break;
                    case MEMORY_TYPE_BREADCRUMB: type_name = "Breadcrumb"; break;
                }
                written = snprintf(pos, remaining, "- [%s] %s\n",
                                 type_name, item->content);
                pos += written;
                remaining -= written;
            }
        }
        written = snprintf(pos, remaining, "\n");
        pos += written;
        remaining -= written;
    }

    /* Add the actual prompt */
    written = snprintf(pos, remaining, "## Current Task\n%s", prompt);

    *out_augmented = augmented;
    augmented = NULL;  /* Transfer ownership */
    LOG_DEBUG("Augmented prompt with memory context (%zu bytes added)",
              total_size - prompt_size);

cleanup:
    free(augmented);
    return result;
}

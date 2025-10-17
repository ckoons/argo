/* © 2025 Casey Koons All rights reserved */
/* Daemon CI Query API - POST /api/ci/query */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Project includes */
#include "argo_daemon_ci_api.h"
#include "argo_daemon.h"
#include "argo_http_server.h"
#include "argo_json.h"
#include "argo_error.h"
#include "argo_limits.h"
#include "argo_log.h"
#include "argo_api_providers.h"
#include "argo_ci.h"
#include "argo_config.h"

/* Callback to capture AI response */
static void response_callback(const ci_response_t* response, void* userdata) {
    char** output = (char**)userdata;
    if (response && response->success && response->content) {
        *output = strdup(response->content);
    }
}

/* Parse CI query request and extract fields */
static int parse_ci_query_request(const char* body, char** query_text,
                                   char** provider_name, char** model_name) {
    int result;

    /* Extract query field */
    const char* query_path[] = {"query"};
    size_t query_len = 0;
    result = json_extract_nested_string(body, query_path, 1, query_text, &query_len);
    if (result != ARGO_SUCCESS || !*query_text) {
        free(*query_text);
        return E_INPUT_FORMAT;
    }

    /* Extract optional provider field (priority: request > config > default) */
    const char* provider_path[] = {"provider"};
    size_t provider_len = 0;
    result = json_extract_nested_string(body, provider_path, 1, provider_name, &provider_len);
    if (result != ARGO_SUCCESS || !*provider_name) {
        /* Not in request - check config for CI_DEFAULT_PROVIDER */
        const char* config_provider = argo_config_get("CI_DEFAULT_PROVIDER");
        if (config_provider) {
            *provider_name = strdup(config_provider);
            LOG_INFO("Using provider from config: %s", *provider_name);
        } else {
            /* Fall back to built-in default */
            *provider_name = strdup("claude_code");
            LOG_INFO("Using built-in default provider: claude_code");
        }
    }

    /* Extract optional model field (priority: request > config > provider default) */
    const char* model_path[] = {"model"};
    size_t model_len = 0;
    result = json_extract_nested_string(body, model_path, 1, model_name, &model_len);
    if (result != ARGO_SUCCESS || !*model_name) {
        /* Not in request - check config for CI_DEFAULT_MODEL */
        const char* config_model = argo_config_get("CI_DEFAULT_MODEL");
        if (config_model) {
            *model_name = strdup(config_model);
            LOG_INFO("Using model from config: %s", *model_name);
        }
        /* else NULL is fine - provider will use its default */
    }

    LOG_INFO("CI Query: provider=%s, model=%s, query_len=%zu",
             *provider_name, *model_name ? *model_name : "default", query_len);

    return ARGO_SUCCESS;
}

/* Create CI provider by name */
static ci_provider_t* create_ci_provider(const char* provider_name, const char* model_name) {
    if (strcmp(provider_name, "claude_code") == 0) {
        return claude_code_create_provider(model_name);
    } else if (strcmp(provider_name, "claude_api") == 0) {
        return claude_api_create_provider(model_name);
    } else if (strcmp(provider_name, "openai_api") == 0) {
        return openai_api_create_provider(model_name);
    } else if (strcmp(provider_name, "gemini_api") == 0) {
        return gemini_api_create_provider(model_name);
    } else if (strcmp(provider_name, "grok_api") == 0) {
        return grok_api_create_provider(model_name);
    } else if (strcmp(provider_name, "deepseek_api") == 0) {
        return deepseek_api_create_provider(model_name);
    } else if (strcmp(provider_name, "openrouter") == 0) {
        return openrouter_create_provider(model_name);
    }
    return NULL;
}

/* Execute provider query with init, connect, query sequence */
static int execute_provider_query(ci_provider_t* provider, const char* query_text,
                                   char** ai_response) {
    int result;

    /* Initialize provider */
    if (provider->init) {
        result = provider->init(provider);
        if (result != ARGO_SUCCESS) {
            return result;
        }
    }

    /* Connect provider */
    if (provider->connect) {
        result = provider->connect(provider);
        if (result != ARGO_SUCCESS) {
            return result;
        }
    }

    /* Execute query with callback */
    result = provider->query(provider, query_text, response_callback, ai_response);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    if (!*ai_response) {
        return E_SYSTEM_PROCESS;
    }

    return ARGO_SUCCESS;
}

/* Format CI response as JSON */
static int format_ci_response(const char* provider_name, const char* ai_response,
                               char** response_json_out) {
    size_t response_size = strlen(ai_response) * RESPONSE_SIZE_MULTIPLIER + RESPONSE_SIZE_OVERHEAD;
    char* response_json = malloc(response_size);
    if (!response_json) {
        return E_SYSTEM_MEMORY;
    }

    /* Build JSON with escaping */
    size_t offset = 0;
    offset += snprintf(response_json + offset, response_size - offset,
                      "{\"status\":\"success\",\"provider\":\"%s\",\"response\":\"",
                      provider_name);

    int result = json_escape_string(response_json, response_size, &offset, ai_response);
    if (result != ARGO_SUCCESS) {
        free(response_json);
        return result;
    }

    offset += snprintf(response_json + offset, response_size - offset, "\"}");

    *response_json_out = response_json;
    return ARGO_SUCCESS;
}

/* POST /api/ci/query - Query AI provider */
int api_ci_query(http_request_t* req, http_response_t* resp) {
    int result = ARGO_SUCCESS;
    char* query_text = NULL;
    char* provider_name = NULL;
    char* model_name = NULL;
    char* ai_response = NULL;
    char* response_json = NULL;
    ci_provider_t* provider = NULL;

    /* Validate request */
    if (!req || !resp) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, DAEMON_ERR_INTERNAL_SERVER);
        return E_SYSTEM_MEMORY;
    }

    if (!req->body) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, DAEMON_ERR_MISSING_REQUEST_BODY);
        return E_INPUT_NULL;
    }

    /* Parse request fields */
    result = parse_ci_query_request(req->body, &query_text, &provider_name, &model_name);
    if (result != ARGO_SUCCESS) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing 'query' field");
        goto cleanup;
    }

    /* Create provider */
    provider = create_ci_provider(provider_name, model_name);
    if (!provider) {
        char error_msg[ARGO_BUFFER_MEDIUM];
        snprintf(error_msg, sizeof(error_msg), "Unknown provider: %s", provider_name);
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, error_msg);
        result = E_INVALID_PARAMS;
        goto cleanup;
    }

    /* Execute query */
    result = execute_provider_query(provider, query_text, &ai_response);
    if (result != ARGO_SUCCESS) {
        const char* error_msg = "Query execution failed";
        if (result == E_SYSTEM_PROCESS) {
            error_msg = "No response from AI provider";
        }
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, error_msg);
        goto cleanup;
    }

    /* Format response */
    result = format_ci_response(provider_name, ai_response, &response_json);
    if (result != ARGO_SUCCESS) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Failed to format response");
        goto cleanup;
    }

    /* Send success response */
    http_response_set_json(resp, HTTP_STATUS_OK, response_json);

cleanup:
    free(query_text);
    free(provider_name);
    free(model_name);
    free(response_json);

    if (provider && provider->cleanup) {
        provider->cleanup(provider);
    }

    return result;
}

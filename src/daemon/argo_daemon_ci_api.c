/* © 2025 Casey Koons All rights reserved */
/* Daemon CI Query API - POST /api/ci/query */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Project includes */
#include "argo_daemon_ci_api.h"
#include "argo_http_server.h"
#include "argo_json.h"
#include "argo_error.h"
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

/* POST /api/ci/query - Query AI provider */
int api_ci_query(http_request_t* req, http_response_t* resp) {
    int result = ARGO_SUCCESS;
    char* query_text = NULL;
    char* provider_name = NULL;
    char* model_name = NULL;
    char* ai_response = NULL;
    ci_provider_t* provider = NULL;

    if (!req || !resp) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    /* Parse JSON body */
    if (!req->body) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing request body");
        return E_INPUT_NULL;
    }

    /* Extract query field */
    const char* query_path[] = {"query"};
    size_t query_len = 0;
    result = json_extract_nested_string(req->body, query_path, 1, &query_text, &query_len);
    if (result != ARGO_SUCCESS || !query_text) {
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, "Missing 'query' field");
        free(query_text);
        return E_INPUT_FORMAT;
    }

    /* Extract optional provider field (priority: request > config > default) */
    const char* provider_path[] = {"provider"};
    size_t provider_len = 0;
    result = json_extract_nested_string(req->body, provider_path, 1, &provider_name, &provider_len);
    if (result != ARGO_SUCCESS || !provider_name) {
        /* Not in request - check config for CI_DEFAULT_PROVIDER */
        const char* config_provider = argo_config_get("CI_DEFAULT_PROVIDER");
        if (config_provider) {
            provider_name = strdup(config_provider);
            LOG_INFO("Using provider from config: %s", provider_name);
        } else {
            /* Fall back to built-in default */
            provider_name = strdup("claude_code");
            LOG_INFO("Using built-in default provider: claude_code");
        }
    }

    /* Extract optional model field (priority: request > config > provider default) */
    const char* model_path[] = {"model"};
    size_t model_len = 0;
    result = json_extract_nested_string(req->body, model_path, 1, &model_name, &model_len);
    if (result != ARGO_SUCCESS || !model_name) {
        /* Not in request - check config for CI_DEFAULT_MODEL */
        const char* config_model = argo_config_get("CI_DEFAULT_MODEL");
        if (config_model) {
            model_name = strdup(config_model);
            LOG_INFO("Using model from config: %s", model_name);
        }
        /* else NULL is fine - provider will use its default */
    }

    LOG_INFO("CI Query: provider=%s, model=%s, query_len=%zu",
             provider_name, model_name ? model_name : "default", query_len);

    /* Create provider */
    if (strcmp(provider_name, "claude_code") == 0) {
        provider = claude_code_create_provider(model_name);
    } else if (strcmp(provider_name, "claude_api") == 0) {
        provider = claude_api_create_provider(model_name);
    } else if (strcmp(provider_name, "openai_api") == 0) {
        provider = openai_api_create_provider(model_name);
    } else if (strcmp(provider_name, "gemini_api") == 0) {
        provider = gemini_api_create_provider(model_name);
    } else if (strcmp(provider_name, "grok_api") == 0) {
        provider = grok_api_create_provider(model_name);
    } else if (strcmp(provider_name, "deepseek_api") == 0) {
        provider = deepseek_api_create_provider(model_name);
    } else if (strcmp(provider_name, "openrouter") == 0) {
        provider = openrouter_create_provider(model_name);
    } else {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg),
                "Unknown provider: %s", provider_name);
        http_response_set_error(resp, HTTP_STATUS_BAD_REQUEST, error_msg);
        result = E_INVALID_PARAMS;
        goto cleanup;
    }

    if (!provider) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR,
                              "Failed to create AI provider");
        result = E_SYSTEM_MEMORY;
        goto cleanup;
    }

    /* Initialize and connect provider */
    if (provider->init) {
        result = provider->init(provider);
        if (result != ARGO_SUCCESS) {
            http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR,
                                  "Failed to initialize provider");
            goto cleanup;
        }
    }

    if (provider->connect) {
        result = provider->connect(provider);
        if (result != ARGO_SUCCESS) {
            http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR,
                                  "Failed to connect to provider");
            goto cleanup;
        }
    }

    /* Execute query with callback */
    result = provider->query(provider, query_text, response_callback, &ai_response);
    if (result != ARGO_SUCCESS) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR,
                              "Query execution failed");
        goto cleanup;
    }

    if (!ai_response) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR,
                              "No response from AI provider");
        result = E_SYSTEM_PROCESS;
        goto cleanup;
    }

    /* Build success response with escaped JSON */
    size_t response_size = strlen(ai_response) * 2 + 512;  /* Extra space for escaping + JSON structure */
    char* response_json = malloc(response_size);
    if (!response_json) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, "Out of memory");
        result = E_SYSTEM_MEMORY;
        goto cleanup;
    }

    /* Build JSON with escaping */
    size_t offset = 0;
    offset += snprintf(response_json + offset, response_size - offset,
                      "{\"status\":\"success\",\"provider\":\"%s\",\"response\":\"",
                      provider_name);

    result = json_escape_string(response_json, response_size, &offset, ai_response);
    if (result != ARGO_SUCCESS) {
        free(response_json);
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR,
                              "Failed to format response");
        goto cleanup;
    }

    offset += snprintf(response_json + offset, response_size - offset, "\"}");

    http_response_set_json(resp, HTTP_STATUS_OK, response_json);

    free(response_json);

cleanup:
    free(query_text);
    free(provider_name);
    free(model_name);

    /* Cleanup provider */
    if (provider && provider->cleanup) {
        provider->cleanup(provider);
    }

    return result;
}

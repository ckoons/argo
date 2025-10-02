/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Project includes */
#include "argo_ci.h"
#include "argo_api_providers.h"
#include "argo_api_keys.h"
#include "argo_api_common.h"
#include "argo_error.h"
#include "argo_log.h"
#include "argo_limits.h"

/* Grok API Configuration */
#define GROK_API_URL "https://api.x.ai/v1/chat/completions"
#define GROK_API_KEY_ENV "XAI_API_KEY"

/* Get API key from environment */
static const char* grok_get_api_key(void) {
    static char api_key[ARGO_BUFFER_MEDIUM] = {0};
    if (api_key[0] == '\0') {
        const char* key = getenv(GROK_API_KEY_ENV);
        if (key) {
            strncpy(api_key, key, sizeof(api_key) - 1);
        }
    }
    return api_key[0] ? api_key : NULL;
}

/* Grok availability check */
bool grok_api_is_available(void) {
    const char* key = grok_get_api_key();
    return (key != NULL && strlen(key) >= API_KEY_MIN_LENGTH);
}

/* Grok-specific JSON request builder */
static int grok_build_request(char* json_body, size_t buffer_size,
                              const char* model, const char* prompt) {
    return snprintf(json_body, buffer_size,
                   "{"
                   "\"model\":\"%s\","
                   "\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],"
                   "\"max_tokens\":%d,"
                   "\"stream\":false"
                   "}",
                   model, prompt, API_MAX_TOKENS);
}

/* Response path for JSON extraction */
static const char* grok_response_path[] = { "choices", "message", "content" };

/* Grok API configuration */
static api_provider_config_t grok_config = {
    .provider_name = "grok-api",
    .default_model = GROK_DEFAULT_MODEL,
    .api_url = GROK_API_URL,
    .url_includes_model = false,
    .auth = {
        .type = API_AUTH_BEARER,
        .value = NULL  /* Set at runtime */
    },
    .extra_headers = NULL,
    .response_path = grok_response_path,
    .response_path_depth = 3,
    .build_request = grok_build_request,
    .supports_streaming = true,
    .max_context = 128000  /* Grok-3 context window */
};

/* Create Grok API provider */
ci_provider_t* grok_api_create_provider(const char* model) {
    const char* api_key = grok_get_api_key();
    if (!api_key) {
        argo_report_error(E_CI_NO_PROVIDER, "grok_api_create_provider",
                         "XAI_API_KEY not set");
        return NULL;
    }

    /* Set API key in config */
    grok_config.auth.value = api_key;

    return generic_api_create_provider(&grok_config, model);
}

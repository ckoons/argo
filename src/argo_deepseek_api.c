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

/* DeepSeek API Configuration */
#define DEEPSEEK_API_URL "https://api.deepseek.com/v1/chat/completions"
#define DEEPSEEK_API_KEY_ENV "DEEPSEEK_API_KEY"

/* Get API key from environment */
static const char* deepseek_get_api_key(void) {
    static char api_key[ARGO_BUFFER_MEDIUM] = {0};
    if (api_key[0] == '\0') {
        const char* key = getenv(DEEPSEEK_API_KEY_ENV);
        if (key) {
            strncpy(api_key, key, sizeof(api_key) - 1);
        }
    }
    return api_key[0] ? api_key : NULL;
}

/* DeepSeek availability check */
bool deepseek_api_is_available(void) {
    const char* key = deepseek_get_api_key();
    return (key != NULL && strlen(key) >= API_KEY_MIN_LENGTH);
}

/* DeepSeek-specific JSON request builder */
static int deepseek_build_request(char* json_body, size_t buffer_size,
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
static const char* deepseek_response_path[] = { "choices", "message", "content" };

/* DeepSeek API configuration */
static api_provider_config_t deepseek_config = {
    .provider_name = "deepseek-api",
    .default_model = DEEPSEEK_DEFAULT_MODEL,
    .api_url = DEEPSEEK_API_URL,
    .url_includes_model = false,
    .auth = {
        .type = API_AUTH_BEARER,
        .value = NULL  /* Set at runtime */
    },
    .extra_headers = NULL,
    .response_path = deepseek_response_path,
    .response_path_depth = 3,
    .build_request = deepseek_build_request,
    .supports_streaming = true,
    .max_context = 64000  /* DeepSeek context window */
};

/* Create DeepSeek API provider */
ci_provider_t* deepseek_api_create_provider(const char* model) {
    const char* api_key = deepseek_get_api_key();
    if (!api_key) {
        argo_report_error(E_CI_NO_PROVIDER, "deepseek_api_create_provider",
                         "DEEPSEEK_API_KEY not set");
        return NULL;
    }

    /* Set API key in config */
    deepseek_config.auth.value = api_key;

    return generic_api_create_provider(&deepseek_config, model);
}

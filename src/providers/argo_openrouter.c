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

/* OpenRouter Configuration */
#define OPENROUTER_API_KEY_ENV "OPENROUTER_API_KEY"

/* Get API key from environment */
static const char* openrouter_get_api_key(void) {
    static char api_key[ARGO_BUFFER_MEDIUM] = {0};
    if (api_key[0] == '\0') {
        const char* key = getenv(OPENROUTER_API_KEY_ENV);
        if (key) {
            strncpy(api_key, key, sizeof(api_key) - 1);
        }
    }
    return api_key[0] ? api_key : NULL;
}

/* OpenRouter availability check */
bool openrouter_is_available(void) {
    const char* key = openrouter_get_api_key();
    return (key != NULL && strlen(key) >= API_KEY_MIN_LENGTH);
}

/* OpenRouter-specific JSON request builder */
static int openrouter_build_request(char* json_body, size_t buffer_size,
                                    const char* model, const char* prompt) {
    return snprintf(json_body, buffer_size,
                   "{" /* GUIDELINE_APPROVED: OpenRouter API JSON template */
                   "\"model\":\"%s\"," /* GUIDELINE_APPROVED: OpenRouter API JSON template */
                   "\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]," /* GUIDELINE_APPROVED: OpenRouter API JSON template */
                   "\"max_tokens\":%d," /* GUIDELINE_APPROVED: OpenRouter API JSON template */
                   "\"stream\":false" /* GUIDELINE_APPROVED: OpenRouter API JSON template */
                   "}", /* GUIDELINE_APPROVED: OpenRouter API JSON template */
                   model, prompt, API_MAX_TOKENS);
}

/* Response path for JSON extraction */
static const char* openrouter_response_path[] = { "choices", "message", "content" };

/* OpenRouter API configuration */
static api_provider_config_t openrouter_config = {
    .provider_name = "openrouter",
    .default_model = OPENROUTER_DEFAULT_MODEL,
    .api_url = OPENROUTER_API_URL,
    .url_includes_model = false,
    .auth = {
        .type = API_AUTH_BEARER,
        .value = NULL  /* Set at runtime */
    },
    .extra_headers = NULL,
    .response_path = openrouter_response_path,
    .response_path_depth = 3,
    .build_request = openrouter_build_request,
    .supports_streaming = true,
    .max_context = OPENROUTER_DEFAULT_CONTEXT
};

/* Create OpenRouter provider */
ci_provider_t* openrouter_create_provider(const char* model) {
    const char* api_key = openrouter_get_api_key();
    if (!api_key) {
        argo_report_error(E_CI_NO_PROVIDER, "openrouter_create_provider",
                         "OPENROUTER_API_KEY not set");
        return NULL;
    }

    /* Set API key in config */
    openrouter_config.auth.value = api_key;

    return generic_api_create_provider(&openrouter_config, model);
}

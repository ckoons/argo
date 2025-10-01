/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Project includes */
#include "argo_ci.h"
#include "argo_api_providers.h"
#include "argo_api_keys.h"
#include "argo_api_common.h"

/* OpenAI-specific JSON request builder */
static int openai_build_request(char* json_body, size_t buffer_size,
                                const char* model, const char* prompt) {
    return snprintf(json_body, buffer_size,
                   "{"
                   "\"model\":\"%s\","
                   "\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],"
                   "\"max_tokens\":%d,"
                   "\"temperature\":0.7"
                   "}",
                   model, prompt, API_MAX_TOKENS);
}

/* OpenAI API configuration */
static const char* openai_response_path[] = {"message", "content"};

static const api_provider_config_t openai_config = {
    .provider_name = "openai-api",
    .default_model = OPENAI_DEFAULT_MODEL,
    .api_url = OPENAI_API_URL,
    .url_includes_model = false,
    .auth = {
        .type = API_AUTH_BEARER,
        .value = OPENAI_API_KEY
    },
    .extra_headers = NULL,
    .response_path = openai_response_path,
    .response_path_depth = 2,
    .build_request = openai_build_request,
    .supports_streaming = true,
    .max_context = OPENAI_MAX_CONTEXT
};

/* Create OpenAI API provider */
ci_provider_t* openai_api_create_provider(const char* model) {
    return generic_api_create_provider(&openai_config, model);
}

/* Check availability */
bool openai_api_is_available(void) {
    return strlen(OPENAI_API_KEY) > API_KEY_MIN_LENGTH;
}

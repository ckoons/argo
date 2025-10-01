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

/* Claude-specific JSON request builder */
static int claude_build_request(char* json_body, size_t buffer_size,
                                const char* model, const char* prompt) {
    return snprintf(json_body, buffer_size,
                   "{"
                   "\"model\":\"%s\","
                   "\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],"
                   "\"max_tokens\":%d"
                   "}",
                   model, prompt, API_MAX_TOKENS);
}

/* Claude API configuration */
static const char* claude_extra_headers[] = {
    "anthropic-version", ANTHROPIC_API_VERSION,
    NULL
};

static const char* claude_response_path[] = {"content", "text"};

static const api_provider_config_t claude_config = {
    .provider_name = "claude-api",
    .default_model = CLAUDE_DEFAULT_MODEL,
    .api_url = ANTHROPIC_API_URL,
    .url_includes_model = false,
    .auth = {
        .type = API_AUTH_HEADER,
        .header_name = "x-api-key",
        .value = ANTHROPIC_API_KEY
    },
    .extra_headers = claude_extra_headers,
    .response_path = claude_response_path,
    .response_path_depth = 2,
    .build_request = claude_build_request,
    .supports_streaming = true,
    .max_context = CLAUDE_MAX_CONTEXT
};

/* Create Claude API provider */
ci_provider_t* claude_api_create_provider(const char* model) {
    return generic_api_create_provider(&claude_config, model);
}

/* Check availability */
bool claude_api_is_available(void) {
    return strlen(ANTHROPIC_API_KEY) > API_KEY_MIN_LENGTH;
}

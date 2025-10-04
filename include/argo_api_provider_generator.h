/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_API_PROVIDER_GENERATOR_H
#define ARGO_API_PROVIDER_GENERATOR_H

#include "argo_ci.h"
#include "argo_api_common.h"
#include "argo_api_keys.h"
#include "argo_limits.h"
#include <stdbool.h>
#include <string.h>

/* Macro to define OpenAI-compatible API providers
 *
 * This generates the complete implementation for providers that use
 * the OpenAI chat completions format (DeepSeek, Grok, etc).
 *
 * Parameters:
 *   NAME - Capitalized name (e.g., DEEPSEEK, GROK)
 *   name - Lowercase name (e.g., deepseek, grok)
 *   API_URL - API endpoint URL
 *   ENV_VAR - Environment variable name for API key
 *   DEFAULT_MODEL - Default model constant name
 *   CONTEXT_WINDOW - Context window size constant
 */
#define DEFINE_OPENAI_COMPATIBLE_PROVIDER(NAME, name, API_URL, ENV_VAR, DEFAULT_MODEL, CONTEXT_WINDOW) \
    /* Get API key from environment */ \
    static const char* name##_get_api_key(void) { \
        static char api_key[ARGO_BUFFER_MEDIUM] = {0}; \
        if (api_key[0] == '\0') { \
            const char* key = getenv(ENV_VAR); \
            if (key) { \
                strncpy(api_key, key, sizeof(api_key) - 1); \
            } \
        } \
        return api_key[0] ? api_key : NULL; \
    } \
    \
    /* Availability check */ \
    bool name##_api_is_available(void) { \
        const char* key = name##_get_api_key(); \
        return (key != NULL && strlen(key) >= API_KEY_MIN_LENGTH); \
    } \
    \
    /* JSON request builder */ \
    static int name##_build_request(char* json_body, size_t buffer_size, \
                                     const char* model, const char* prompt) { \
        return snprintf(json_body, buffer_size, \
                       "{" \
                       "\"model\":\"%s\"," \
                       "\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]," \
                       "\"max_tokens\":%d," \
                       "\"stream\":false" \
                       "}", \
                       model, prompt, API_MAX_TOKENS); \
    } \
    \
    /* Response path for JSON extraction */ \
    static const char* name##_response_path[] = { "choices", "message", "content" }; \
    \
    /* API configuration */ \
    static api_provider_config_t name##_config = { \
        .provider_name = #name "-api", \
        .default_model = DEFAULT_MODEL, \
        .api_url = API_URL, \
        .url_includes_model = false, \
        .auth = { \
            .type = API_AUTH_BEARER, \
            .value = NULL  /* Set at runtime */ \
        }, \
        .extra_headers = NULL, \
        .response_path = name##_response_path, \
        .response_path_depth = 3, \
        .build_request = name##_build_request, \
        .supports_streaming = true, \
        .max_context = CONTEXT_WINDOW \
    }; \
    \
    /* Create provider */ \
    ci_provider_t* name##_api_create_provider(const char* model) { \
        const char* api_key = name##_get_api_key(); \
        if (!api_key) { \
            argo_report_error(E_CI_NO_PROVIDER, #name "_api_create_provider", \
                             ENV_VAR " not set"); \
            return NULL; \
        } \
        \
        /* Set API key in config */ \
        name##_config.auth.value = api_key; \
        \
        return generic_api_create_provider(&name##_config, model); \
    }

#endif /* ARGO_API_PROVIDER_GENERATOR_H */

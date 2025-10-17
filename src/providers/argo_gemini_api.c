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

/* Gemini-specific JSON request builder */
static int gemini_build_request(char* json_body, size_t buffer_size,
                                const char* model, const char* prompt) {
    (void)model;  /* Model is in URL, not request body */
    return snprintf(json_body, buffer_size,
                   "{" /* GUIDELINE_APPROVED: Gemini API JSON template */
                   "\"contents\":[{" /* GUIDELINE_APPROVED: Gemini API JSON template */
                   "\"parts\":[{" /* GUIDELINE_APPROVED: Gemini API JSON template */
                   "\"text\":\"%s\"" /* GUIDELINE_APPROVED: Gemini API JSON template */
                   "}]" /* GUIDELINE_APPROVED: Gemini API JSON template */
                   "}]," /* GUIDELINE_APPROVED: Gemini API JSON template */
                   "\"generationConfig\":{" /* GUIDELINE_APPROVED: Gemini API JSON template */
                   "\"maxOutputTokens\":%d," /* GUIDELINE_APPROVED: Gemini API JSON template */
                   "\"temperature\":0.7" /* GUIDELINE_APPROVED: Gemini API JSON template */
                   "}" /* GUIDELINE_APPROVED: Gemini API JSON template */
                   "}", /* GUIDELINE_APPROVED: Gemini API JSON template */
                   prompt, API_MAX_TOKENS);
}

/* Gemini API configuration */
static const char* gemini_response_path[] = {"candidates", "text"};

static const api_provider_config_t gemini_config = {
    .provider_name = "gemini-api",
    .default_model = GEMINI_DEFAULT_MODEL,
    .api_url = GEMINI_API_URL,
    .url_includes_model = true,  /* Gemini puts model in URL path */
    .auth = {
        .type = API_AUTH_URL_PARAM,
        .param_name = "key",
        .value = GEMINI_API_KEY
    },
    .extra_headers = NULL,
    .response_path = gemini_response_path,
    .response_path_depth = 2,
    .build_request = gemini_build_request,
    .supports_streaming = true,
    .max_context = GEMINI_MAX_CONTEXT
};

/* Create Gemini API provider */
ci_provider_t* gemini_api_create_provider(const char* model) {
    return generic_api_create_provider(&gemini_config, model);
}

/* Check availability */
bool gemini_api_is_available(void) {
    return strlen(GEMINI_API_KEY) > API_KEY_MIN_LENGTH;
}

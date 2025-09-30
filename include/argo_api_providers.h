/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_API_PROVIDERS_H
#define ARGO_API_PROVIDERS_H

#include "argo_ci.h"

/* Provider creation functions */
ci_provider_t* claude_api_create_provider(const char* model);
ci_provider_t* openai_api_create_provider(const char* model);
ci_provider_t* gemini_api_create_provider(const char* model);
ci_provider_t* grok_api_create_provider(const char* model);
ci_provider_t* deepseek_api_create_provider(const char* model);
ci_provider_t* openrouter_create_provider(const char* model);

/* Availability checks */
bool claude_api_is_available(void);
bool openai_api_is_available(void);
bool gemini_api_is_available(void);
bool grok_api_is_available(void);
bool deepseek_api_is_available(void);
bool openrouter_is_available(void);

/* Default models for each provider */
#define CLAUDE_DEFAULT_MODEL "claude-3-5-sonnet-20241022"
#define OPENAI_DEFAULT_MODEL "gpt-4o"
#define GEMINI_DEFAULT_MODEL "gemini-1.5-flash"
#define GROK_DEFAULT_MODEL "grok-beta"
#define DEEPSEEK_DEFAULT_MODEL "deepseek-chat"
#define OPENROUTER_DEFAULT_MODEL "anthropic/claude-3.5-sonnet"

/* Cost tracking structure */
typedef struct {
    const char* provider;
    const char* model;
    float cost_per_1k_input;
    float cost_per_1k_output;
    int max_context;
    bool supports_streaming;
} api_model_info_t;

/* Get model info */
const api_model_info_t* get_api_model_info(const char* provider, const char* model);

#endif /* ARGO_API_PROVIDERS_H */
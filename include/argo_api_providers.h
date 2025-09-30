/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_API_PROVIDERS_H
#define ARGO_API_PROVIDERS_H

#include "argo_ci.h"

/* Common API provider constants */
#define API_MODEL_NAME_SIZE 64
#define API_RESPONSE_CAPACITY 65536
#define API_REQUEST_BUFFER_SIZE 8192
#define API_MAX_TOKENS 4096
#define API_AUTH_HEADER_SIZE 256
#define API_URL_SIZE 512
#define API_HTTP_OK 200
#define API_KEY_MIN_LENGTH 10

/* API Version strings */
#define ANTHROPIC_API_VERSION "2023-06-01"

/* Context sizes */
#define CLAUDE_MAX_CONTEXT 200000
#define OPENAI_MAX_CONTEXT 128000
#define GEMINI_MAX_CONTEXT 2097152

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

/*
 * Default models for each provider
 *
 * These can be overridden by creating include/argo_local_models.h
 * See include/argo_local_models.h.example for template
 * Or run: scripts/update_models.sh
 */

/* Check for local model overrides first */
#if __has_include("argo_local_models.h")
#include "argo_local_models.h"
#else
/* Built-in defaults - Last verified: 2025-09-30 */
#define CLAUDE_DEFAULT_MODEL "claude-sonnet-4-5-20250929"     /* Claude Sonnet 4.5 (latest) */
#define OPENAI_DEFAULT_MODEL "gpt-4o"                          /* GPT-4o (current) */
#define GEMINI_DEFAULT_MODEL "gemini-2.5-flash"                /* Gemini 2.5 Flash */
#define GROK_DEFAULT_MODEL "grok-3"                            /* Grok 3 (current) */
#define DEEPSEEK_DEFAULT_MODEL "deepseek-chat"                 /* DeepSeek Chat (current) */
#define OPENROUTER_DEFAULT_MODEL "anthropic/claude-sonnet-4.5" /* Claude Sonnet 4.5 via OpenRouter */
#endif

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
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

/* Provider name constants */
#define PROVIDER_NAME_CLAUDE_API "claude-api"
#define PROVIDER_NAME_OPENAI_API "openai-api"
#define PROVIDER_NAME_GEMINI_API "gemini-api"
#define PROVIDER_NAME_GROK_API "grok-api"
#define PROVIDER_NAME_DEEPSEEK_API "deepseek-api"
#define PROVIDER_NAME_OPENROUTER "openrouter"
#define PROVIDER_NAME_CLAUDE_CODE "claude-code"
#define PROVIDER_NAME_MOCK "mock"

/* CLI command names */
#define CLAUDE_CLI_COMMAND "claude"
#define CLAUDE_CLI_ARG_PIPE "-p"

/* Claude binary paths */
#define CLAUDE_PATH_USR_LOCAL_BIN "/usr/local/bin/claude"
#define CLAUDE_PATH_USR_BIN "/usr/bin/claude"
#define CLAUDE_PATH_HOMEBREW "/opt/homebrew/bin/claude"

/* Error messages for providers */
#define ERR_MSG_CLAUDE_NOT_IN_PATH "claude command not found in PATH"
#define ERR_MSG_OPENROUTER_KEY_NOT_SET "OPENROUTER_API_KEY not set"

/* Provider-specific constants */
#define CLAUDE_CODE_STREAMING_ID "claude-code-streaming"
#define MOCK_DEFAULT_MODEL "mock-model"
#define MOCK_DEFAULT_RESPONSE "Mock CI response"

/* Context sizes */
#define CLAUDE_MAX_CONTEXT 200000
#define OPENAI_MAX_CONTEXT 128000
#define GEMINI_MAX_CONTEXT 2097152

/* Provider creation functions */
ci_provider_t* claude_code_create_provider(const char* model);
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
#define CLAUDE_CODE_DEFAULT_MODEL "claude-sonnet-4"            /* Claude Code (simple name) */
#define CLAUDE_DEFAULT_MODEL "claude-sonnet-4-5-20250929"      /* Claude Sonnet 4.5 (latest) */
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

/* Claude Code memory management (sundown/sunrise) */
int claude_code_set_sunrise(ci_provider_t* provider, const char* brief);
int claude_code_set_sunset(ci_provider_t* provider, const char* notes);
ci_memory_digest_t* claude_code_get_memory(ci_provider_t* provider);

#endif /* ARGO_API_PROVIDERS_H */
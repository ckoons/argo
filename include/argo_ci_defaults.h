/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_CI_DEFAULTS_H
#define ARGO_CI_DEFAULTS_H

#include <stddef.h>

/* Model configuration structure */
typedef struct ci_model_config {
    const char* model_name;          /* Model identifier */
    const char* provider;            /* Provider name */
    size_t default_context;          /* Default context size */
    size_t max_context;              /* Maximum context size */
    int default_timeout_ms;          /* Default timeout */
    float temperature;               /* Generation temperature */
    float top_p;                     /* Nucleus sampling */
    int max_tokens;                  /* Max response tokens */
    const char* api_endpoint;        /* API endpoint if applicable */
    bool supports_streaming;         /* Streaming support */
    bool supports_tools;             /* Tool/function support */
    bool supports_vision;            /* Image understanding */
} ci_model_config_t;

/* Default configurations for known models */
static const ci_model_config_t CI_MODEL_DEFAULTS[] = {
    /* Ollama models */
    {
        .model_name = "llama3.3:70b",
        .provider = "ollama",
        .default_context = 8192,
        .max_context = 131072,
        .default_timeout_ms = 60000,
        .temperature = 0.7,
        .top_p = 0.9,
        .max_tokens = 4096,
        .api_endpoint = "http://localhost:11434",
        .supports_streaming = true,
        .supports_tools = true,
        .supports_vision = false
    },
    {
        .model_name = "llama3.2:3b",
        .provider = "ollama",
        .default_context = 4096,
        .max_context = 131072,
        .default_timeout_ms = 30000,
        .temperature = 0.7,
        .top_p = 0.9,
        .max_tokens = 2048,
        .api_endpoint = "http://localhost:11434",
        .supports_streaming = true,
        .supports_tools = true,
        .supports_vision = false
    },
    {
        .model_name = "qwen2.5-coder:32b",
        .provider = "ollama",
        .default_context = 32768,
        .max_context = 131072,
        .default_timeout_ms = 45000,
        .temperature = 0.3,
        .top_p = 0.95,
        .max_tokens = 8192,
        .api_endpoint = "http://localhost:11434",
        .supports_streaming = true,
        .supports_tools = true,
        .supports_vision = false
    },
    {
        .model_name = "deepseek-r1:14b",
        .provider = "ollama",
        .default_context = 64000,
        .max_context = 131072,
        .default_timeout_ms = 60000,
        .temperature = 0.3,
        .top_p = 0.95,
        .max_tokens = 8192,
        .api_endpoint = "http://localhost:11434",
        .supports_streaming = true,
        .supports_tools = true,
        .supports_vision = false
    },

    /* Claude models (via Claude Code, not API) */
    {
        .model_name = "claude-3-opus",
        .provider = "claude-code",
        .default_context = 200000,
        .max_context = 200000,
        .default_timeout_ms = 90000,
        .temperature = 0.3,
        .top_p = 0.95,
        .max_tokens = 4096,
        .api_endpoint = NULL,  /* Claude Code doesn't use API */
        .supports_streaming = false,  /* Not with Claude Code approach */
        .supports_tools = true,
        .supports_vision = true
    },
    {
        .model_name = "claude-3.5-sonnet",
        .provider = "claude-code",
        .default_context = 200000,
        .max_context = 200000,
        .default_timeout_ms = 60000,
        .temperature = 0.3,
        .top_p = 0.95,
        .max_tokens = 8192,
        .api_endpoint = NULL,
        .supports_streaming = false,
        .supports_tools = true,
        .supports_vision = true
    },

    /* OpenAI models */
    {
        .model_name = "gpt-4-turbo",
        .provider = "openai",
        .default_context = 128000,
        .max_context = 128000,
        .default_timeout_ms = 60000,
        .temperature = 0.5,
        .top_p = 1.0,
        .max_tokens = 4096,
        .api_endpoint = "https://api.openai.com/v1",
        .supports_streaming = true,
        .supports_tools = true,
        .supports_vision = true
    },
    {
        .model_name = "gpt-4o",
        .provider = "openai",
        .default_context = 128000,
        .max_context = 128000,
        .default_timeout_ms = 45000,
        .temperature = 0.5,
        .top_p = 1.0,
        .max_tokens = 16384,
        .api_endpoint = "https://api.openai.com/v1",
        .supports_streaming = true,
        .supports_tools = true,
        .supports_vision = true
    },

    /* Google models */
    {
        .model_name = "gemini-1.5-pro",
        .provider = "google",
        .default_context = 1000000,
        .max_context = 2000000,
        .default_timeout_ms = 90000,
        .temperature = 0.4,
        .top_p = 0.95,
        .max_tokens = 8192,
        .api_endpoint = "https://generativelanguage.googleapis.com",
        .supports_streaming = true,
        .supports_tools = true,
        .supports_vision = true
    },

    /* xAI models */
    {
        .model_name = "grok-2",
        .provider = "xai",
        .default_context = 131072,
        .max_context = 131072,
        .default_timeout_ms = 60000,
        .temperature = 0.5,
        .top_p = 0.9,
        .max_tokens = 4096,
        .api_endpoint = "https://api.x.ai/v1",
        .supports_streaming = true,
        .supports_tools = false,
        .supports_vision = false
    },

    /* Sentinel value */
    {NULL, NULL, 0, 0, 0, 0, 0, 0, NULL, false, false, false}
};

/* Personality presets for different roles */
typedef struct ci_personality_preset {
    const char* role;
    const char* personality_name;
    const char* description;
    float temperature_adjustment;    /* Adjust from model default */
    const char* system_prompt_addon; /* Added to system prompt */
} ci_personality_preset_t;

static const ci_personality_preset_t CI_PERSONALITY_PRESETS[] = {
    {
        .role = "builder",
        .personality_name = "Argo",
        .description = "The master builder, focused on implementation",
        .temperature_adjustment = -0.2,  /* More deterministic */
        .system_prompt_addon = "You are Argo, a master builder. Focus on clean, efficient implementation following all coding standards."
    },
    {
        .role = "coordinator",
        .personality_name = "Io",
        .description = "The coordinator, managing memory and flow",
        .temperature_adjustment = 0.0,
        .system_prompt_addon = "You are Io, the coordinator. Manage information flow, memory, and inter-CI communication effectively."
    },
    {
        .role = "requirements",
        .personality_name = "Maia",
        .description = "The planner, defining requirements and approach",
        .temperature_adjustment = 0.1,  /* Slightly more creative */
        .system_prompt_addon = "You are Maia, the requirements specialist. Define clear requirements, acceptance criteria, and project plans."
    },
    {
        .role = "analysis",
        .personality_name = "Iris",
        .description = "The analyzer, reviewing and verifying",
        .temperature_adjustment = -0.1,  /* More analytical */
        .system_prompt_addon = "You are Iris, the analyzer. Review code, identify issues, and verify correctness with precision."
    },
    {NULL, NULL, NULL, 0, NULL}
};

/* Utility functions */
const ci_model_config_t* ci_get_model_defaults(const char* model_name);
const ci_personality_preset_t* ci_get_personality_preset(const char* role);
size_t ci_get_default_context_size(const char* model_name);
int ci_get_default_timeout(const char* model_name);
bool ci_query_model_capability(const char* model_name);

/* Model validation */
bool ci_is_model_supported(const char* model_name);
bool ci_validate_context_size(const char* model_name, size_t requested_size);

/* Dynamic configuration */
int ci_update_model_config(const char* model_name,
                          const ci_model_config_t* new_config);
int ci_load_model_configs(const char* config_file);

#endif /* ARGO_CI_DEFAULTS_H */
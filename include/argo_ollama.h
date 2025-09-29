/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_OLLAMA_H
#define ARGO_OLLAMA_H

#include "argo_ci.h"
#include "argo_ci_defaults.h"

/* Ollama configuration */
#define OLLAMA_DEFAULT_ENDPOINT "http://localhost"
#define OLLAMA_DEFAULT_PORT 11434
#define OLLAMA_DEFAULT_MODEL "llama3.3:70b"
#define OLLAMA_TIMEOUT_SECONDS 60
#define OLLAMA_BUFFER_SIZE 8192

/* Request format for Ollama API */
#define OLLAMA_REQUEST_FORMAT "{\"model\":\"%s\",\"prompt\":\"%s\",\"stream\":false}"

/* Ollama provider creation */
ci_provider_t* ollama_create_provider(const char* model_name);

/* Ollama-specific functions */
int ollama_list_models(char*** models, int* count);
int ollama_pull_model(const char* model_name);
bool ollama_is_running(void);
int ollama_get_model_info(const char* model_name, ci_model_config_t* config);

#endif /* ARGO_OLLAMA_H */
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
#define OLLAMA_REQUEST_FORMAT "{\"model\":\"%s\",\"prompt\":\"%s\",\"stream\":%s}"

/* Ollama JSON field names */
#define OLLAMA_JSON_RESPONSE "\"response\":\""

/* Ollama buffer sizes */
#define OLLAMA_ENDPOINT_SIZE 256
#define OLLAMA_MODEL_SIZE 64
#define OLLAMA_RESPONSE_CAPACITY 16384
#define OLLAMA_REQUEST_BUFFER_SIZE 8192
#define OLLAMA_LINE_BUFFER_SIZE 8192
#define OLLAMA_CHUNK_SIZE 1024
#define OLLAMA_JSON_BODY_SIZE 4096
#define OLLAMA_RESPONSE_FIELD_OFFSET 12
#define OLLAMA_POLL_DELAY_US 10000

/* Ollama provider creation */
ci_provider_t* ollama_create_provider(const char* model_name);

/* Ollama-specific functions */
int ollama_list_models(char*** models, int* count);
int ollama_pull_model(const char* model_name);
bool ollama_is_running(void);
int ollama_get_model_info(const char* model_name, ci_model_config_t* config);

/* Streaming control */
void ollama_set_streaming(ci_provider_t* provider, bool enabled);
bool ollama_get_streaming(ci_provider_t* provider);

#endif /* ARGO_OLLAMA_H */
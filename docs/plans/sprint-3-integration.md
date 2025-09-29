© 2025 Casey Koons All rights reserved

# Sprint 3: Integration (Weeks 5-6)

## Objectives

Create the AI provider abstraction layer, implement Claude Code optimizations, package as a library, and build the LSP server for universal IDE integration.

## Deliverables

1. **AI Provider Abstraction**
   - Generic provider interface
   - Claude, OpenAI, Ollama implementations
   - Provider configuration system

2. **Claude Code Optimizations**
   - Parallel session support
   - Artifact management
   - Claude Max account features

3. **Library Packaging**
   - Static library (libargo.a)
   - Dynamic library (libargo.so)
   - Public API headers

4. **LSP Server**
   - Language Server Protocol implementation
   - IDE communication bridge
   - Configuration management

## Task Breakdown

### Week 5: AI Provider Layer

#### Day 21-22: Provider Interface
- [ ] Design argo_ai_provider_t structure
- [ ] Implement base provider functions
- [ ] Create provider registry
- [ ] Configuration file parser

#### Day 23-24: Provider Implementations
- [ ] Claude provider (providers/claude.c)
- [ ] OpenAI provider (providers/openai.c)
- [ ] Ollama provider (providers/ollama.c)
- [ ] Provider switching logic

#### Day 25: Claude Optimizations
- [ ] Parallel session management
- [ ] Artifact handling
- [ ] Rate limit management
- [ ] Project context support

### Week 6: Packaging and LSP

#### Day 26-27: Library Packaging
- [ ] Create libargo build targets
- [ ] Export public API symbols
- [ ] Generate pkg-config file
- [ ] Create installation script

#### Day 28-29: LSP Server
- [ ] Implement LSP message protocol
- [ ] Create initialization handshake
- [ ] Add Argo-specific commands
- [ ] Handle configuration changes

#### Day 30: Integration Testing
- [ ] Test with multiple providers
- [ ] VSCode extension skeleton
- [ ] Performance benchmarks
- [ ] Documentation completion

## Code Samples

### AI Provider Interface (argo_ai.h)
```c
/* argo_ai.h */
/* © 2025 Casey Koons All rights reserved */
#ifndef ARGO_AI_H
#define ARGO_AI_H

typedef struct argo_ai_response {
    char* content;
    size_t content_len;
    int stream_complete;
    void* provider_data;
} argo_ai_response_t;

typedef int (*argo_ai_init_fn)(void* config);
typedef int (*argo_ai_connect_fn)(void* context);
typedef int (*argo_ai_send_fn)(const char* prompt, argo_ai_response_t** response);
typedef int (*argo_ai_stream_fn)(const char* prompt,
                                 void (*callback)(const char* chunk, void* data),
                                 void* user_data);
typedef void (*argo_ai_cleanup_fn)(void);

typedef struct argo_ai_provider {
    char name[ARGO_PROVIDER_NAME_LEN];
    char version[ARGO_VERSION_LEN];
    argo_ai_init_fn init;
    argo_ai_connect_fn connect;
    argo_ai_send_fn send;
    argo_ai_stream_fn stream;
    argo_ai_cleanup_fn cleanup;
    void* provider_context;
} argo_ai_provider_t;

/* Provider registry */
int argo_ai_register_provider(argo_ai_provider_t* provider);
argo_ai_provider_t* argo_ai_get_provider(const char* name);
int argo_ai_set_active_provider(const char* name);
```

### Claude Provider Implementation (providers/claude.c)
```c
/* providers/claude.c */
/* © 2025 Casey Koons All rights reserved */
#include "argo.h"
#include "argo_ai.h"
#include "providers/claude.h"

typedef struct claude_context {
    char api_key[ARGO_API_KEY_LEN];
    int max_sessions;
    int current_sessions;
    int use_artifacts;
    int use_projects;
    void* curl_handle;
} claude_context_t;

static int claude_init(void* config) {
    claude_context_t* ctx = calloc(1, sizeof(claude_context_t));
    if (!ctx) return -1;

    argo_config_t* cfg = (argo_config_t*)config;
    strncpy(ctx->api_key, cfg->api_key, ARGO_API_KEY_LEN - 1);
    ctx->max_sessions = cfg->max_sessions ? cfg->max_sessions : 1;
    ctx->use_artifacts = cfg->use_artifacts;

    /* Initialize CURL for API calls */
    ctx->curl_handle = curl_easy_init();
    if (!ctx->curl_handle) {
        free(ctx);
        return -1;
    }

    return 0;
}

static int claude_send(const char* prompt, argo_ai_response_t** response) {
    claude_context_t* ctx = get_provider_context();

    /* Build Claude API request */
    char request[ARGO_MAX_REQUEST];
    snprintf(request, sizeof(request),
             CLAUDE_API_FORMAT,
             ctx->use_artifacts ? "true" : "false",
             prompt);

    /* Send via CURL */
    char* result = claude_api_call(ctx->curl_handle, request, ctx->api_key);
    if (!result) return -1;

    /* Parse response */
    *response = calloc(1, sizeof(argo_ai_response_t));
    (*response)->content = result;
    (*response)->content_len = strlen(result);

    return 0;
}

static argo_ai_provider_t claude_provider = {
    .name = "claude",
    .version = "1.0.0",
    .init = claude_init,
    .connect = claude_connect,
    .send = claude_send,
    .stream = claude_stream,
    .cleanup = claude_cleanup
};

void argo_claude_register(void) {
    argo_ai_register_provider(&claude_provider);
}
```

### LSP Server Implementation (tools/argo-lsp/lsp.c)
```c
/* tools/argo-lsp/lsp.c */
/* © 2025 Casey Koons All rights reserved */
#include <stdio.h>
#include "argo.h"
#include "lsp_protocol.h"

typedef struct lsp_server {
    argo_context_t* argo_ctx;
    int client_fd;
    char* root_uri;
    int initialized;
} lsp_server_t;

static void handle_initialize(lsp_server_t* server, json_t* params) {
    /* Extract root URI */
    server->root_uri = json_get_string(params, "rootUri");

    /* Initialize Argo */
    server->argo_ctx = argo_init(NULL);
    if (!server->argo_ctx) {
        lsp_send_error(server->client_fd, "Failed to initialize Argo");
        return;
    }

    /* Send capabilities */
    json_t* result = json_object();
    json_t* capabilities = json_object();

    json_object_set(capabilities, "argoProvider", json_true());
    json_object_set(capabilities, "parallelDevelopment", json_true());
    json_object_set(capabilities, "mergeNegotiation", json_true());

    json_object_set(result, "capabilities", capabilities);
    lsp_send_response(server->client_fd, 1, result);

    server->initialized = 1;
}

static void handle_argo_command(lsp_server_t* server, const char* command, json_t* args) {
    if (strcmp(command, "argo/createSession") == 0) {
        const char* branch = json_get_string(args, "branch");
        argo_session_t* session = argo_session_create(server->argo_ctx, branch, "lsp");

        json_t* result = json_object();
        json_object_set(result, "sessionId", json_string(session->id));
        lsp_send_response(server->client_fd, 0, result);

    } else if (strcmp(command, "argo/negotiate") == 0) {
        const char* branch_a = json_get_string(args, "branchA");
        const char* branch_b = json_get_string(args, "branchB");

        argo_negotiation_t* neg = argo_merge_start(branch_a, branch_b);
        /* Send negotiation updates via LSP notifications */

    } else {
        lsp_send_error(server->client_fd, "Unknown Argo command");
    }
}
```

### Library Public API (include/argo.h)
```c
/* include/argo.h */
/* © 2025 Casey Koons All rights reserved */
#ifndef ARGO_H
#define ARGO_H

#ifdef __cplusplus
extern "C" {
#endif

/* Version information */
#define ARGO_VERSION_MAJOR 1
#define ARGO_VERSION_MINOR 0
#define ARGO_VERSION_PATCH 0

/* Core types */
typedef struct argo_context argo_context_t;
typedef struct argo_session argo_session_t;
typedef struct argo_message argo_message_t;
typedef struct argo_negotiation argo_negotiation_t;

/* Initialization */
argo_context_t* argo_init(const char* config_path);
void argo_cleanup(argo_context_t* ctx);
int argo_configure(argo_context_t* ctx, const char* key, const char* value);

/* Session management */
argo_session_t* argo_create_session(argo_context_t* ctx,
                                    const char* branch,
                                    const char* purpose);
int argo_destroy_session(argo_context_t* ctx, const char* session_id);
argo_session_t* argo_list_sessions(argo_context_t* ctx, int* count);

/* Messaging */
int argo_send_message(argo_context_t* ctx,
                      const char* from,
                      const char* to,
                      const char* type,
                      const char* payload);
argo_message_t* argo_receive_message(argo_context_t* ctx,
                                     const char* session_id,
                                     int timeout_ms);

/* Merge negotiation */
argo_negotiation_t* argo_start_negotiation(argo_context_t* ctx,
                                           const char* branch_a,
                                           const char* branch_b);
int argo_propose_resolution(argo_negotiation_t* neg,
                           const char* session_id,
                           const char* resolution);
int argo_apply_resolution(argo_negotiation_t* neg);

/* AI provider */
int argo_set_ai_provider(argo_context_t* ctx, const char* provider);
char* argo_ai_query(argo_context_t* ctx, const char* prompt);

#ifdef __cplusplus
}
#endif

#endif /* ARGO_H */
```

## Testing Strategy

### Provider Testing
- Test each provider with same prompts
- Verify response format consistency
- Test provider switching
- Error handling for API failures

### Library Testing
- Link test program with libargo
- Verify symbol exports
- Test in C and C++ programs
- Memory leak validation

### LSP Testing
- Test with VSCode client
- Message protocol compliance
- Command execution
- Configuration changes

## Success Criteria

1. **Functional**
   - All 3 providers working
   - Library linkable
   - LSP server connects to VSCode
   - Claude optimizations active

2. **Quality**
   - Clean API design
   - Provider abstraction complete
   - No memory leaks
   - < 7000 lines total code

3. **Performance**
   - Provider switching < 100ms
   - LSP response < 50ms
   - Library size < 500KB

## Notes

- Keep provider interface minimal
- LSP server should be stateless where possible
- Document configuration format
- Prepare for demo sprint

## Review Checklist

- [ ] Provider interface documented
- [ ] All providers tested
- [ ] Library builds on Linux/macOS
- [ ] LSP protocol compliant
- [ ] Claude features working
- [ ] Public API stable
- [ ] Integration tests passing
- [ ] Ready for packaging
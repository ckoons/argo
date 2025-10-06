# Building Argo with Claude

© 2025 Casey Koons. All rights reserved.

## What Argo Is

Argo is a lean C library (<10,000 lines) for coordinating multiple AI coding assistants in parallel development workflows. Built for clarity over cleverness, simplicity over showing off.

**Copyright note:** All rights reserved to preserve legal options during uncertain AI authorship law. When laws clarify, this may become co-authored credit. This protects what we're building together.

## Architecture Overview

**Layered Design** (bottom to top):
1. **Foundation**: Error handling (argo_error), logging, HTTP/socket, JSON parsing
   - `argo_error.c` - Centralized error reporting with `argo_report_error()`
   - `argo_http.c` - HTTP client (curl-based, with goto cleanup patterns)
   - `argo_socket.c` - Unix socket communication for CI coordination
   - `argo_json.c` - JSON parsing utilities
   - `argo_limits.h` - ALL numeric constants (buffer sizes, permissions, timeouts)

2. **Providers**: AI backends (Claude, OpenAI, Gemini, Ollama, etc.) - pluggable via ci_provider_t interface
   - API providers: `argo_claude_api.c`, `argo_openai_api.c`, `argo_gemini_api.c`, `argo_grok_api.c`, `argo_deepseek_api.c`, `argo_openrouter.c`
   - Local providers: `argo_ollama.c` (HTTP), `argo_claude.c` (CLI), `argo_claude_code.c` (CLI)
   - Common utilities: `argo_api_common.c` (HTTP POST, buffer allocation, memory augmentation)

3. **Registry**: Service discovery, CI instance tracking, port allocation
   - `argo_registry.c` - Core registry operations
   - `argo_registry_messaging.c` - Inter-CI communication
   - `argo_registry_persistence.c` - Registry state persistence

4. **Lifecycle**: Session management, shared services coordination
   - `argo_lifecycle.c` - Resource lifecycle management
   - `argo_session.c` - Session tracking
   - `argo_shared_services.c` - Background task management

5. **Orchestration**: Multi-CI coordination, workflow execution engine
   - `argo_orchestrator.c` - Multi-CI coordination
   - `argo_workflow.c` - Core workflow engine
   - `argo_workflow_executor.c` - Workflow execution
   - `argo_merge.c` - Code merge conflict resolution

6. **Workflows**: JSON-driven, pauseable, checkpointable task automation
   - `argo_workflow_loader.c` - Load workflows from JSON
   - `argo_workflow_checkpoint.c` - Save/restore workflow state
   - `argo_workflow_steps_*.c` - Step type implementations
   - `argo_workflow_persona.c` - Persona-based behavior

**Key Abstractions**:
- `ci_provider_t` - Unified AI provider interface with 9 implementations:
  - Claude: Code (CLI), CLI, API
  - OpenAI API, Gemini API, Grok API, DeepSeek API
  - Ollama (local HTTP), OpenRouter (multi-model API)
- `workflow_controller_t` - Stateful workflow executor with persona support
- `ci_registry_t` - Service registry for multi-CI coordination
- `lifecycle_manager_t` - Resource lifecycle management
- `ci_memory_digest_t` - Memory management with context limits

**Data Flow (Daemon-Based Architecture)**:
```
arc CLI
    ↓ (HTTP)
argo-daemon (persistent service)
    ├─ Registry (service discovery)
    ├─ Lifecycle (resource management)
    └─ Orchestrator (coordination)
    ↓ (fork+exec)
workflow_executor (per-workflow process)
    ↓
Workflow Controller (step execution)
    ↓
Provider Interface (ci_provider_t)
    ↓
AI Backend (Claude/OpenAI/Gemini/etc)
    ↓ (HTTP)
argo-daemon (progress reporting)
```

**See:** [Daemon Architecture Documentation](docs/DAEMON_ARCHITECTURE.md) for detailed design.

**Library Structure**:
- **libargo_core.a** - Foundation (error, HTTP, JSON, providers)
- **libargo_daemon.a** - Daemon services (registry, lifecycle, orchestrator, HTTP server)
- **libargo_workflow.a** - Workflow execution (controller, steps, checkpoint)
- **arc** - CLI (links core only, HTTP calls to daemon)

**Module Dependencies**:
- Core has no dependencies on higher layers
- Daemon depends on Core
- Workflow depends on Core (but NOT Daemon)
- Daemon ↔ Workflow communicate via HTTP only

## Daemon Architecture

### Communication Model

**HTTP-Based Message Passing**:
- `arc → daemon`: HTTP REST (POST, GET, DELETE)
- `daemon → executor`: fork/exec + signals (SIGTERM, SIGINT)
- `executor → daemon`: HTTP progress updates (best-effort)

**Why HTTP?**
- Location independence (localhost or remote)
- Loose coupling (no shared memory)
- Fault isolation (executor crash won't kill daemon)
- Language agnostic (future executors in any language)
- Testable (easy to mock endpoints)

### REST API Endpoints

**Workflow Operations** (`argo_daemon_api.c`):
```c
POST   /api/workflow/start         /* Start new workflow */
GET    /api/workflow/list          /* List all workflows */
GET    /api/workflow/status/{id}   /* Get workflow status */
DELETE /api/workflow/abandon/{id}  /* Terminate workflow */
POST   /api/workflow/progress/{id} /* Progress update (from executor) */
POST   /api/workflow/pause/{id}    /* Pause workflow (stub) */
POST   /api/workflow/resume/{id}   /* Resume workflow (stub) */
```

**Info Endpoints**:
```c
GET /api/health    /* Daemon health check */
GET /api/version   /* Daemon version */
```

### HTTP Server Implementation

**Custom HTTP Server** (`argo_http_server.c`):
- ~350 lines, zero dependencies
- Simple request/response handling
- Route-based dispatch
- JSON request/response format
- Thread-per-connection model

**Why custom?** No external dependencies (mongoose, libmicrohttpd), full control, minimal size.

### Binaries

**argo-daemon** (165 KB):
- Persistent service (listens on port 9876 by default)
- HTTP server + REST API handlers
- Registry and lifecycle management
- Forks workflow executors on demand

**argo_workflow_executor** (179 KB):
- Per-workflow process (one executor per workflow)
- Loads JSON workflow templates
- Executes steps sequentially
- Reports progress to daemon via HTTP
- Signal handling (SIGTERM/SIGINT)

**arc** (~100 KB):
- Thin HTTP client
- Links only libargo_core.a
- All operations via REST API to daemon

## Core Philosophy

### "What you don't build, you don't debug."

Before writing code:
1. Search existing code for similar patterns
2. Check utility headers (`argo_*_common.h`, `argo_json.h`)
3. Reuse or refactor existing code
4. Only create new code when truly necessary

**Simple beats clever. Follow patterns that work.**

## Essential Rules

### Copyright & Files
- Add to ALL files: `/* © 2025 Casey Koons All rights reserved */`
- Never create files without explicit request
- Never create documentation files unless asked
- Prefer editing existing files over creating new ones

### Memory & Safety

**Memory Safety Checklist**:
- [ ] Check ALL return values (malloc, calloc, strdup, fopen, etc.)
- [ ] Free everything you allocate (no leaks)
- [ ] **Use goto cleanup pattern** for ALL functions that allocate resources
- [ ] Initialize ALL variables at declaration
- [ ] Check for strdup() failures (it can return NULL)
- [ ] Verify realloc() success before using new pointer
- [ ] Transfer ownership clearly (set pointer to NULL after transfer)
- [ ] Never return uninitialized pointers
- [ ] NULL-check all pointer parameters at function entry

**String Safety Checklist**:
- [ ] **NEVER use unsafe functions**: gets, strcpy, sprintf, strcat
- [ ] **ALWAYS use safe alternatives**: strncpy + null termination, snprintf, strncat
- [ ] Always provide size limits (sizeof(buffer) - 1 for strncpy)
- [ ] Explicitly null-terminate after strncpy
- [ ] Use snprintf, never sprintf
- [ ] Track offset for multiple string operations
- [ ] Check buffer sizes before concatenation
- [ ] Validate string lengths don't exceed buffer capacity

**Resource Safety Checklist**:
- [ ] Close all file handles (fclose)
- [ ] Close all sockets
- [ ] Unlock all mutexes
- [ ] Free all allocated memory
- [ ] Clean up in reverse order of allocation
- [ ] Use goto cleanup for consistent cleanup path

### Error Reporting
- **ONLY use `argo_report_error()` for ALL errors** - single breakpoint location
- Format: `argo_report_error(code, "function_name", "additional details")`
- Never use `fprintf(stderr, ...)` or `LOG_ERROR()` directly for errors
- `LOG_ERROR()` is for informational logging only (non-error events)
- All errors route through one function for consistent format and debugging

### Code Organization
- Max 600 lines per .c file (refactor if approaching, 3% tolerance acceptable)
- One clear purpose per module
- **NEVER use numeric constants in .c files** - ALL constants in headers (argo_limits.h, module headers)
- **NEVER use string literals in .c files** - ALL strings in headers (except format strings)
- **File/directory permissions**: Use ARGO_DIR_PERMISSIONS (0755), ARGO_FILE_PERMISSIONS (0644)
- **Buffer sizes**: Use ARGO_PATH_MAX (512) for paths, ARGO_BUFFER_* for other buffers
- Never use environment variables at runtime (.env for build only)

### When to Extract Common Patterns
**Rule of 3** - Extract at 3rd usage, profit at 4+:
- **2 USES**: Keep as-is (extraction overhead not justified yet)
- **AT 3RD USAGE**: Extract (breakeven point)
- **AT 4TH+ USAGE**: Significant savings (50%+)

Wait until the 3rd usage appears before extracting. At 2 uses, the extraction overhead (helper function + configs) doesn't justify the savings. At 3 uses, it breaks even. At 4+, savings become significant.

### Build & Test
- Compile with: `gcc -Wall -Werror -Wextra -std=c11`
- Before committing: `make clean && make && make test-quick`
- All 33 tests must pass
- Stay under 10,000 meaningful lines (check with `./scripts/count_core.sh`)

## Mandatory Daily Practices

### Before Every Code Change
1. **Read before write** - Always read existing files first
2. **Search for patterns** - Grep for similar code before creating new
3. **Check headers** - Look for existing constants/macros before defining new ones

### During Coding
1. **No magic numbers** - Every numeric constant goes in a header with a descriptive name
2. **No string literals** - Every string (except format strings) defined as constant in header
3. **HTTP errors** - Always log "HTTP {code} ({description})" format, never lose information
4. **Buffer allocation** - Use `ARGO_ALLOC_BUFFER()` or `ensure_buffer_capacity()` with error reporting
5. **Error reporting** - Always call `argo_report_error()` with meaningful context, never silent failures
6. **Memory cleanup** - Every malloc has corresponding free, use goto cleanup pattern
7. **Null checks** - Use `ARGO_CHECK_NULL()` at function entry for all pointer parameters

### After Coding
1. **Compile clean** - Zero warnings, zero errors with `-Wall -Werror -Wextra`
2. **Run tests** - `make test-quick` must pass 100%
3. **Check TODOs** - Remove TODO comments or convert to clear future work notes
4. **Verify constants** - Grep for `[0-9]{2,}` in your .c file - should find nothing (except line numbers)
5. **Check line count** - `./scripts/count_core.sh` - stay under budget

### Never Commit
- Code with magic numbers in .c files
- Code using unsafe string functions (strcpy, sprintf, strcat, gets)
- Code with TODOs (convert to clear comments or implement)
- Code that doesn't compile with `-Werror`
- Code that fails any test
- Code with memory leaks (check with valgrind if uncertain)
- Functions over 100 lines (refactor first)
- Files over 600 lines (split into modules, 3% tolerance acceptable)
- Resource allocations without goto cleanup pattern

## Common Patterns

### Error Handling (goto cleanup)
```c
int argo_operation(void) {
    int result = 0;
    char* buffer = NULL;

    buffer = malloc(SIZE);
    if (!buffer) {
        result = E_SYSTEM_MEMORY;
        goto cleanup;
    }

    result = some_operation(buffer);
    if (result != ARGO_SUCCESS) {
        goto cleanup;
    }

cleanup:
    free(buffer);
    return result;
}
```

### Utility Macros (use these)
```c
ARGO_CHECK_NULL(ptr);                    // Null check with early return
ARGO_GET_CONTEXT(provider, type, ctx);   // Extract provider context
ARGO_UPDATE_STATS(ctx);                  // Update query statistics
ensure_buffer_capacity(&buf, &cap, size); // Grow buffer if needed
```

### Provider Creation Pattern
```c
ci_provider_t* provider_create(const char* model) {
    context_t* ctx = calloc(1, sizeof(context_t));
    if (!ctx) {
        LOG_ERROR("Failed to allocate context");
        return NULL;
    }

    init_provider_base(&ctx->provider, ctx, init_fn, connect_fn,
                      query_fn, stream_fn, cleanup_fn);

    strncpy(ctx->model, model ? model : DEFAULT_MODEL, sizeof(ctx->model) - 1);

    strncpy(ctx->provider.name, "provider-name", sizeof(ctx->provider.name) - 1);
    strncpy(ctx->provider.model, ctx->model, sizeof(ctx->provider.model) - 1);
    ctx->provider.supports_streaming = true;
    ctx->provider.supports_memory = false;
    ctx->provider.max_context = MAX_CONTEXT_SIZE;

    LOG_INFO("Created provider for model %s", ctx->model);
    return &ctx->provider;
}
```

### Streaming (use the wrapper)
```c
static int provider_stream(ci_provider_t* provider, const char* prompt,
                          ci_stream_callback callback, void* userdata) {
    ARGO_CHECK_NULL(prompt);
    ARGO_CHECK_NULL(callback);
    return ci_query_to_stream(provider, prompt, provider_query,
                             callback, userdata);
}
```

### API Providers (use common utilities)
```c
/* Setup authentication */
api_auth_config_t auth = {
    .type = API_AUTH_BEARER,  // or API_AUTH_HEADER, API_AUTH_URL_PARAM
    .value = API_KEY
};

/* HTTP request */
http_response_t* resp = NULL;
int result = api_http_post_json(API_URL, json_body, &auth, extra_headers, &resp);
if (result != ARGO_SUCCESS) return result;

/* Extract JSON content */
const char* field_path[] = { "message", "content" };
char* content = NULL;
size_t len = 0;
result = json_extract_nested_string(resp->body, field_path, 2, &content, &len);

/* Copy to context buffer */
result = ensure_buffer_capacity(&ctx->response_content, &ctx->response_capacity, len + 1);
if (result == ARGO_SUCCESS) {
    memcpy(ctx->response_content, content, len);
    ctx->response_content[len] = '\0';
}
free(content);
http_response_free(resp);
```

### String Safety (Mandatory)
```c
/* WRONG - unsafe, can overflow */
strcpy(buffer, source);
strcat(buffer, more);
sprintf(buffer, "%s", str);

/* CORRECT - safe with size limits */
strncpy(buffer, source, sizeof(buffer) - 1);
buffer[sizeof(buffer) - 1] = '\0';  /* Explicit null termination */

strncat(buffer, more, sizeof(buffer) - strlen(buffer) - 1);

snprintf(buffer, sizeof(buffer), "%s", str);

/* BEST - use offset tracking for multiple operations */
size_t offset = 0;
offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Part 1: %s\n", part1);
offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Part 2: %s\n", part2);
```

### Daemon API Handler Pattern
```c
/* REST API handler (in argo_daemon_api.c) */
int api_workflow_operation(http_request_t* req, http_response_t* resp) {
    if (!req || !resp) {
        http_response_set_error(resp, 500, "Internal server error");
        return E_SYSTEM_MEMORY;
    }

    /* Extract path parameters */
    const char* workflow_id = extract_path_param(req->path, "/api/workflow/operation");
    if (!workflow_id) {
        http_response_set_error(resp, 400, "Missing workflow ID");
        return E_INVALID_PARAMS;
    }

    /* Parse JSON body if needed */
    if (req->method == HTTP_METHOD_POST && !req->body) {
        http_response_set_error(resp, 400, "Missing request body");
        return E_INVALID_PARAMS;
    }

    /* Perform operation */
    int result = perform_operation(workflow_id, req->body);
    if (result == E_WORKFLOW_NOT_FOUND) {
        http_response_set_error(resp, 404, "Workflow not found");
        return result;
    } else if (result != ARGO_SUCCESS) {
        http_response_set_error(resp, 500, "Operation failed");
        return result;
    }

    /* Return success */
    char response_json[256];
    snprintf(response_json, sizeof(response_json),
            "{\"status\":\"success\",\"workflow_id\":\"%s\"}",
            workflow_id);

    http_response_set_json(resp, 200, response_json);
    return ARGO_SUCCESS;
}
```

### Arc HTTP Client Pattern
```c
/* Arc command using HTTP (in arc/src/) */
int arc_workflow_command(int argc, char** argv) {
    const char* workflow_id = NULL;

    /* Get workflow ID from args or context */
    if (argc >= 1) {
        workflow_id = argv[0];
    } else {
        LOG_USER_ERROR("No workflow specified\n");
        LOG_USER_INFO("Usage: arc workflow command <workflow_id>\n");
        return ARC_EXIT_ERROR;
    }

    /* Build request URL */
    char endpoint[512];
    snprintf(endpoint, sizeof(endpoint), "/api/workflow/command/%s", workflow_id);

    /* Send HTTP request to daemon */
    arc_http_response_t* response = NULL;
    int result = arc_http_get(endpoint, &response);  /* or arc_http_post/delete */
    if (result != ARGO_SUCCESS) {
        LOG_USER_ERROR("Failed to connect to daemon: %s\n", arc_get_daemon_url());
        LOG_USER_INFO("  Make sure daemon is running: argo-daemon\n");
        return ARC_EXIT_ERROR;
    }

    /* Check HTTP status */
    if (response->status_code == 404) {
        LOG_USER_ERROR("Workflow not found: %s\n", workflow_id);
        arc_http_response_free(response);
        return ARC_EXIT_ERROR;
    } else if (response->status_code != 200) {
        LOG_USER_ERROR("Command failed (HTTP %d)\n", response->status_code);
        arc_http_response_free(response);
        return ARC_EXIT_ERROR;
    }

    /* Parse and display response */
    if (response->body) {
        /* Simple JSON field extraction using sscanf */
        char field[256] = {0};
        const char* field_str = strstr(response->body, "\"field\"");
        if (field_str) {
            sscanf(field_str, "\"field\":\"%255[^\"]\"", field);
        }
        LOG_USER_SUCCESS("Operation succeeded: %s\n", field);
    }

    /* Cleanup */
    arc_http_response_free(response);
    return ARC_EXIT_SUCCESS;
}
```

### Executor Progress Reporting Pattern
```c
/* Executor reports progress to daemon (in workflow executor) */
static void report_progress(const char* workflow_id, int current_step,
                           int total_steps, const char* step_name) {
    CURL* curl = curl_easy_init();
    if (!curl) return;

    char url[512];
    char json_body[512];
    snprintf(url, sizeof(url),
            "http://localhost:9876/api/workflow/progress/%s", workflow_id);
    snprintf(json_body, sizeof(json_body),
            "{\"current_step\":%d,\"total_steps\":%d,\"step_name\":\"%s\"}",
            current_step, total_steps, step_name ? step_name : "");

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);  /* Best-effort */
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);   /* Don't need response */

    curl_easy_perform(curl);  /* Ignore errors */

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
}
```

## Where to Find Things

### Common Utilities
- `argo_ci_common.h` - Provider macros, inline helpers, buffer allocation
- `argo_api_common.h` - HTTP requests, API authentication
- `argo_json.h` - JSON parsing
- `argo_http.h` - Low-level HTTP operations, HTTP constants
- `argo_socket.h` - Socket configuration, buffer sizes
- `argo_error.h` - Error codes and messages
- `argo_error_messages.h` - Internationalization-ready error strings
- `argo_limits.h` - **All numeric constants** (buffer sizes, timeouts, permissions)
- `argo_file_utils.h` - File operations (file_read_all)

### Daemon-Specific
- `argo_http_server.h` - HTTP server implementation (route handling, request/response)
- `argo_daemon_api.h` - REST API handlers for all endpoints
- `argo_orchestrator.h` - Workflow orchestration (fork/exec executors)
- `arc_http_client.h` - HTTP client for arc CLI (daemon communication)

### Workflow-Specific
- `argo_workflow.h` - Workflow controller, step execution
- `argo_workflow_loader.h` - JSON workflow template loading
- `argo_workflow_checkpoint.h` - Save/restore workflow state
- `argo_workflow_steps.h` - Step type implementations

### Constants by Category
**HTTP** (`argo_http.h`):
```c
HTTP_DEFAULT_TIMEOUT_SECONDS, HTTP_CMD_BUFFER_SIZE
HTTP_RESPONSE_BUFFER_SIZE, HTTP_CHUNK_SIZE
HTTP_STATUS_OK, HTTP_STATUS_BAD_REQUEST, HTTP_STATUS_UNAUTHORIZED
HTTP_STATUS_FORBIDDEN, HTTP_STATUS_NOT_FOUND, HTTP_STATUS_RATE_LIMIT
HTTP_STATUS_SERVER_ERROR, HTTP_PORT_HTTPS, HTTP_PORT_HTTP
```

**Socket** (`argo_socket.h`):
```c
SOCKET_PATH_MAX, SOCKET_JSON_TOKEN_MAX
SOCKET_BUFFER_SIZE, MS_PER_SECOND
SOCKET_BACKLOG, MAX_PENDING_REQUESTS
```

**System Limits** (`argo_limits.h`):
```c
/* Buffer sizes */
ARGO_BUFFER_TINY (32), ARGO_BUFFER_SMALL (64), ARGO_BUFFER_MEDIUM (256)
ARGO_BUFFER_STANDARD (4096), ARGO_BUFFER_LARGE (16384)

/* Filesystem */
ARGO_PATH_MAX (512)
ARGO_DIR_PERMISSIONS (0755), ARGO_FILE_PERMISSIONS (0644)

/* Ports, timeouts, merge confidence, etc */
```

**Error Messages** (`argo_error_messages.h`):
```c
ERR_MSG_MEMORY_ALLOC_FAILED, ERR_MSG_FILE_OPEN_FAILED
ERR_MSG_HTTP_BAD_REQUEST, ERR_MSG_API_KEY_MISSING
... and 40+ more internationalization-ready messages
```

### Learn by Example
- API provider? Read `src/argo_claude_api.c` or `src/argo_openai_api.c`
- Local provider? Read `src/argo_ollama.c`
- CLI provider? Read `src/argo_claude_code.c`
- Daemon API handler? Read `src/argo_daemon_api.c` (see `api_workflow_status`, `api_workflow_progress`)
- Arc HTTP command? Read `arc/src/workflow_status.c` or `arc/src/workflow_abandon.c`
- Executor implementation? Read `bin/argo_workflow_executor_main.c`
- HTTP server? Read `src/argo_http_server.c`
- Error handling? Read any provider init/query function
- Testing? Read `tests/test_providers.c`

## Naming Conventions

```c
/* Correct */
typedef struct argo_context {
    int ref_count;
    char* buffer;
} argo_context_t;

void argo_context_init(argo_context_t* ctx);
static int helper_function(const char* input);

/* Wrong */
typedef struct ArgoContext { ... }  // NO CamelCase
void ArgoContextInit(...)           // NO CamelCase
```

## File Structure

```c
/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>

/* Project includes */
#include "argo_ci_common.h"
#include "module_name.h"

/* Static declarations */
static int helper_function(void);

/* Public functions */
int argo_module_init(void) {
    /* implementation */
}

/* Static functions */
static int helper_function(void) {
    /* implementation */
}
```

## Working with Casey

When Casey says:
- **"implement X"** - Write minimal code, reuse utilities, follow existing patterns
- **"analyze X"** - Analysis only, no code changes
- **"refactor X"** - Improve structure, reduce duplication
- **"debug X"** - Find and fix the specific issue

When uncertain:
- Look at similar existing code first
- Ask Casey rather than guess
- Simple solution over clever solution
- Working code over perfect code

## Provider Implementation Guide

### Provider Types

**API Providers** (HTTP-based):
- Claude API, OpenAI API, Gemini API, Grok API, DeepSeek API, OpenRouter
- Use `argo_api_common.c` utilities
- Authentication via bearer tokens or custom headers
- JSON request/response handling

**Local Providers** (process/socket based):
- Ollama (local HTTP server on port 11434)
- Claude CLI/Code (process execution)

### Adding a New API Provider

1. **Create provider file**: `src/argo_newprovider_api.c`
2. **Include copyright header**
3. **Define context structure**:
```c
typedef struct {
    ci_provider_t provider;        /* Base provider (MUST be first) */
    char model[256];               /* Model name */
    char api_key[512];             /* API key */
    char* response_content;        /* Response buffer */
    size_t response_capacity;      /* Buffer capacity */
    /* Provider-specific fields */
} newprovider_context_t;
```

4. **Implement required functions**:
```c
static int newprovider_init(ci_provider_t* provider);
static int newprovider_connect(ci_provider_t* provider);
static int newprovider_query(ci_provider_t* provider, const char* prompt, char** response);
static int newprovider_stream(ci_provider_t* provider, const char* prompt,
                               ci_stream_callback callback, void* userdata);
static void newprovider_cleanup(ci_provider_t* provider);
```

5. **Create provider constructor**:
```c
ci_provider_t* newprovider_create_provider(const char* model) {
    newprovider_context_t* ctx = calloc(1, sizeof(newprovider_context_t));
    if (!ctx) return NULL;

    init_provider_base(&ctx->provider, ctx, newprovider_init, newprovider_connect,
                      newprovider_query, newprovider_stream, newprovider_cleanup);

    strncpy(ctx->model, model ? model : "default-model", sizeof(ctx->model) - 1);
    strncpy(ctx->provider.name, "provider-name", sizeof(ctx->provider.name) - 1);
    strncpy(ctx->provider.model, ctx->model, sizeof(ctx->provider.model) - 1);

    ctx->provider.supports_streaming = true;
    ctx->provider.max_context = 128000;  /* Provider's context window */

    return &ctx->provider;
}
```

6. **Implement query function** using `argo_api_common.c`:
```c
static int newprovider_query(ci_provider_t* provider, const char* prompt, char** response) {
    ARGO_CHECK_NULL(prompt);
    ARGO_CHECK_NULL(response);
    ARGO_GET_CONTEXT(provider, newprovider_context_t, ctx);

    /* Build JSON request */
    char json_body[API_JSON_BODY_SIZE];
    snprintf(json_body, sizeof(json_body),
            "{\"model\":\"%s\",\"prompt\":\"%s\"}",
            ctx->model, prompt);

    /* Setup authentication */
    api_auth_config_t auth = {
        .type = API_AUTH_BEARER,
        .value = ctx->api_key
    };

    /* Make HTTP request */
    http_response_t* resp = NULL;
    int result = api_http_post_json(API_URL, json_body, &auth, NULL, &resp);
    if (result != ARGO_SUCCESS) return result;

    /* Extract response content */
    const char* field_path[] = {"response", "text"};
    char* content = NULL;
    size_t len = 0;
    result = json_extract_nested_string(resp->body, field_path, 2, &content, &len);

    if (result == ARGO_SUCCESS) {
        result = ensure_buffer_capacity(&ctx->response_content,
                                       &ctx->response_capacity, len + 1);
        if (result == ARGO_SUCCESS) {
            memcpy(ctx->response_content, content, len);
            ctx->response_content[len] = '\0';
            *response = ctx->response_content;
        }
    }

    free(content);
    http_response_free(resp);
    return result;
}
```

7. **Add to Makefile** and test

### Provider Checklist

- [ ] Copyright header on all files
- [ ] Context structure with `ci_provider_t` as first field
- [ ] All 5 required functions implemented
- [ ] `supports_streaming` set correctly
- [ ] `max_context` set to provider's limit
- [ ] API key loaded from environment
- [ ] Error handling with `argo_report_error()`
- [ ] Memory cleanup in cleanup function
- [ ] goto cleanup pattern for allocations
- [ ] Test file created (`tests/test_newprovider.c`)

## Incomplete Features & Future Work

### Daemon Architecture (Production Ready ✅)

**Fully Implemented**:
- HTTP server (custom, 350 lines, zero dependencies)
- REST API endpoints (start, list, status, abandon, progress)
- Workflow executor (complete implementation, not stub)
- Bi-directional messaging (executor → daemon via HTTP POST)
- Arc CLI integration (workflow_start, workflow_status, workflow_abandon use HTTP)
- Process management (fork/exec, signal handling)
- Error handling (consistent HTTP status codes, JSON responses)

**Partial/Stub Implementations**:
- `api_workflow_pause()` - Returns success but doesn't actually pause execution
- `api_workflow_resume()` - Returns success but doesn't actually resume execution
- `arc workflow list` - Still uses direct registry access (not HTTP yet)

**Future Enhancements** (design documented, not implemented):
- Long-polling for real-time updates
- WebSocket upgrade for streaming progress
- Distributed daemon coordination (multi-host)
- Enhanced progress tracking in registry

### Legacy Code (Pre-Daemon)

**No-op Implementations** (complete but minimal):
- `argo_registry.c`:
  - `registry_load_config()` - Returns success (config loading not yet needed)
  - `registry_connect_ci()` - Returns success (socket handled at higher level)
  - `registry_disconnect_ci()` - Returns success (socket handled at higher level)

These functions are intentionally minimal. They perform null checks and return success, which is correct for optional functionality that isn't currently required.

**Incomplete Implementations** (return E_INTERNAL_NOTIMPL):
- `argo_claude.c`:
  - `claude_stream()` - Streaming not supported by CLI (intentional limitation)

This represents an actual limitation, not future work.

## JSON Builder Pattern (Future)

When implementing complex JSON construction, consider this pattern:
```c
typedef struct {
    char* buffer;
    size_t capacity;
    size_t used;
} json_builder_t;

int json_builder_init(json_builder_t* jb, size_t initial_size);
int json_builder_add_string(json_builder_t* jb, const char* key, const char* value);
int json_builder_add_int(json_builder_t* jb, const char* key, int value);
char* json_builder_finalize(json_builder_t* jb);  /* Caller must free */
```

Currently we use `snprintf()` for JSON which works fine for simple cases. Implement builder only when needed for complex nested JSON.

## Reminders

- Quality over quantity - do less perfectly rather than more poorly
- Code you'd want to debug at 3am
- Tests catch bugs, simplicity prevents them
- Budget: 3,217 / 10,000 lines (32%) - plenty of room to grow
- All 33 tests passing
- 6 API providers (claude, openai, gemini, grok, deepseek, openrouter)

---

**This is a collaboration.** Casey brings decades of hard-won engineering wisdom. Claude brings fresh perspective and tireless implementation. Together we build something maintainable, debuggable, and actually useful.

*"What you don't build, you don't debug."*

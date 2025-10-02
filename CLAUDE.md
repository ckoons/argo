# Building Argo with Claude

© 2025 Casey Koons. All rights reserved.

## What Argo Is

Argo is a lean C library (<10,000 lines) for coordinating multiple AI coding assistants in parallel development workflows. Built for clarity over cleverness, simplicity over showing off.

**Copyright note:** All rights reserved to preserve legal options during uncertain AI authorship law. When laws clarify, this may become co-authored credit. This protects what we're building together.

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
- Check all return values and allocations
- Free everything you allocate (no leaks)
- Use goto cleanup pattern for error handling
- Initialize variables at declaration
- Never use unsafe functions (gets, strcpy without length)

### Error Reporting
- **ONLY use `argo_report_error()` for ALL errors** - single breakpoint location
- Format: `argo_report_error(code, "function_name", "additional details")`
- Never use `fprintf(stderr, ...)` or `LOG_ERROR()` directly for errors
- `LOG_ERROR()` is for informational logging only (non-error events)
- All errors route through one function for consistent format and debugging

### Code Organization
- Max 600 lines per .c file (refactor if approaching)
- One clear purpose per module
- **NEVER use numeric constants in .c files** - ALL constants in headers
- **NEVER use string literals in .c files** - ALL strings in headers
- Never use environment variables at runtime (.env for build only)

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
- Code with TODOs (convert to clear comments or implement)
- Code that doesn't compile with `-Werror`
- Code that fails any test
- Code with memory leaks (check with valgrind if uncertain)
- Functions over 100 lines (refactor first)
- Files over 600 lines (split into modules)

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

## Where to Find Things

### Common Utilities
- `argo_ci_common.h` - Provider macros, inline helpers, buffer allocation
- `argo_api_common.h` - HTTP requests, API authentication
- `argo_json.h` - JSON parsing
- `argo_http.h` - Low-level HTTP operations, HTTP constants
- `argo_socket.h` - Socket configuration, buffer sizes
- `argo_error.h` - Error codes and messages
- `argo_error_messages.h` - Internationalization-ready error strings

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

## Incomplete Features & Future Work

**No-op Implementations** (complete but minimal):
- `argo_registry.c`:
  - `registry_load_config()` - Returns success (config loading not yet needed)
  - `registry_connect_ci()` - Returns success (socket handled at higher level)
  - `registry_disconnect_ci()` - Returns success (socket handled at higher level)

These functions are intentionally minimal. They perform null checks and return success, which is correct for optional functionality that isn't currently required.

**Incomplete Implementations** (return E_INTERNAL_NOTIMPL):
- `argo_claude.c`:
  - `claude_stream()` - Streaming not supported by CLI (intentional limitation)
- `argo_ollama.c`:
  - `ollama_query_default()` - Has fallback to streaming (never actually called)

These represent actual limitations or unreachable code paths, not future work.

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

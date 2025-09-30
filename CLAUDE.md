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

### Code Organization
- Max 600 lines per .c file (refactor if approaching)
- One clear purpose per module
- All constants in header files (no magic numbers in .c files)
- All paths/strings in header files (no hardcoded strings in .c files)
- Never use environment variables at runtime (.env for build only)

### Build & Test
- Compile with: `gcc -Wall -Werror -Wextra -std=c11`
- Before committing: `make clean && make && make test-quick`
- All 33 tests must pass
- Stay under 10,000 meaningful lines (check with `./scripts/count_core.sh`)

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
- `argo_ci_common.h` - Provider macros, inline helpers
- `argo_api_common.h` - HTTP requests, buffer allocation
- `argo_json.h` - JSON parsing
- `argo_http.h` - Low-level HTTP operations
- `argo_error.h` - Error codes and messages

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

## Reminders

- Quality over quantity - do less perfectly rather than more poorly
- Code you'd want to debug at 3am
- Tests catch bugs, simplicity prevents them
- 21% of budget used (2127/10000 lines) - plenty of room to grow

---

**This is a collaboration.** Casey brings decades of hard-won engineering wisdom. Claude brings fresh perspective and tireless implementation. Together we build something maintainable, debuggable, and actually useful.

*"What you don't build, you don't debug."*

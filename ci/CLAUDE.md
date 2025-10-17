# Building CI Tool with Claude

© 2025 Casey Koons. All rights reserved.

## What CI Is

CI (Companion Intelligence tool) is a lightweight command-line query tool for the Argo platform. It provides a simple, direct interface for workflows to query AI providers without the full complexity of Arc CLI or workflow orchestration.

**Purpose**: Enable bash workflows to query AI providers with a single command
**Architecture**: Thin HTTP client that communicates with argo-daemon
**Budget**: <500 lines (minimal, focused tool)

## Inherits Core Standards

CI follows all standards from `/CLAUDE.md`:
- Memory management (check returns, free everything, goto cleanup pattern)
- Error reporting (use `argo_report_error()` exclusively)
- No magic numbers (all constants in ci_constants.h)
- File size limits (max 600 lines per file)
- HTTP communication (all operations via argo-daemon REST API)

## CI Design Philosophy

### Simplicity First
- **Single purpose**: Query AI providers, nothing more
- **Zero configuration**: Works out of the box with daemon defaults
- **No state**: Stateless tool, daemon manages everything
- **Fast startup**: Thin client, under 50KB binary

### Command-Line Interface
```bash
# Simple query (command line argument)
ci "What is the capital of France?"

# Or via stdin
echo "What is 2+2?" | ci

# With provider override
ci --provider=claude_api --model=claude-opus-4 "Explain quantum computing"

# Help
ci --help
```

## Architecture

### Thin Client Model
```
ci (command-line tool)
  ↓ (HTTP POST)
argo-daemon (background service)
  ├─ Configuration (provider, model from ~/.argo/config)
  ├─ Provider selection (defaults if not specified)
  └─ Query execution
  ↓ (HTTP response)
ci → stdout (AI response)
```

### No Direct Provider Access
- CI NEVER talks to AI providers directly
- ALL queries route through argo-daemon `/api/ci/query` endpoint
- Daemon handles provider selection, authentication, timeouts
- CI is just an HTTP client + stdin/stdout handler

## Code Organization

```
ci/
├── src/
│   ├── ci_main.c           # Entry point, argument parsing
│   ├── ci_http_client.c    # HTTP communication with daemon
│   └── ci_stdin_handler.c  # Read from stdin if no args
├── include/
│   ├── ci_constants.h      # ALL constants (buffer sizes, timeouts)
│   └── ci_http_client.h    # HTTP client interface
└── tests/
    ├── test_ci_integration.sh  # Integration tests (requires daemon)
    └── test_ci_http_client.c   # HTTP client unit tests
```

## Implementation Patterns

### Main Function Pattern
```c
int main(int argc, char** argv) {
    char query[CI_QUERY_MAX] = {0};
    char* provider = NULL;
    char* model = NULL;

    /* Parse arguments (getopt) */
    int c;
    while ((c = getopt(argc, argv, "hp:m:")) != -1) {
        switch (c) {
            case 'p': provider = optarg; break;
            case 'm': model = optarg; break;
            case 'h': print_usage(); return CI_EXIT_SUCCESS;
        }
    }

    /* Get query from args or stdin */
    if (optind < argc) {
        /* Command line query */
        strncpy(query, argv[optind], sizeof(query) - 1);
    } else {
        /* Read from stdin */
        if (!read_stdin(query, sizeof(query))) {
            fprintf(stderr, "Error: No query provided\n");
            return CI_EXIT_ERROR;
        }
    }

    /* Execute query via daemon */
    char* response = NULL;
    int result = ci_query(query, provider, model, &response);
    if (result != ARGO_SUCCESS) {
        fprintf(stderr, "Query failed\n");
        return CI_EXIT_ERROR;
    }

    /* Output response */
    printf("%s\n", response);
    free(response);

    return CI_EXIT_SUCCESS;
}
```

### HTTP Client Pattern
```c
/* POST query to daemon */
int ci_query(const char* query, const char* provider,
             const char* model, char** response) {
    CURL* curl = NULL;
    char* json_body = NULL;
    size_t body_size = CI_REQUEST_BUFFER_SIZE;
    http_response_t* http_resp = NULL;
    int result = ARGO_SUCCESS;

    /* Allocate request buffer */
    json_body = calloc(1, body_size);
    if (!json_body) {
        result = E_SYSTEM_MEMORY;
        goto cleanup;
    }

    /* Build JSON request */
    size_t offset = 0;
    offset += snprintf(json_body + offset, body_size - offset,
                      "{\"query\":\"%s\"", query);

    if (provider) {
        offset += snprintf(json_body + offset, body_size - offset,
                          ",\"provider\":\"%s\"", provider);
    }

    if (model) {
        offset += snprintf(json_body + offset, body_size - offset,
                          ",\"model\":\"%s\"", model);
    }

    offset += snprintf(json_body + offset, body_size - offset, "}");

    /* Initialize curl */
    curl = curl_easy_init();
    if (!curl) {
        result = E_SYSTEM_CURL;
        goto cleanup;
    }

    /* Send request to daemon */
    const char* daemon_url = ci_get_daemon_url();
    char endpoint[512];
    snprintf(endpoint, sizeof(endpoint), "%s/api/ci/query", daemon_url);

    /* ... curl setup and execution ... */

    /* Extract response from JSON */
    /* ... json parsing ... */

cleanup:
    free(json_body);
    if (curl) curl_easy_cleanup(curl);
    if (http_resp) http_response_free(http_resp);
    return result;
}
```

### Daemon Auto-Start Pattern
```c
/* Ensure daemon is running before query */
int ci_ensure_daemon_running(void) {
    /* Try HTTP health check */
    CURL* curl = curl_easy_init();
    if (!curl) return E_SYSTEM_CURL;

    const char* daemon_url = ci_get_daemon_url();
    char health_endpoint[512];
    snprintf(health_endpoint, sizeof(health_endpoint),
             "%s/api/health", daemon_url);

    curl_easy_setopt(curl, CURLOPT_URL, health_endpoint);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res == CURLE_OK) {
        return ARGO_SUCCESS;  /* Daemon running */
    }

    /* Daemon not running - start it */
    return ci_start_daemon();
}

int ci_start_daemon(void) {
    pid_t pid = fork();
    if (pid == 0) {
        /* Child - exec daemon */
        char* args[] = {"argo-daemon", "--port", "9876", NULL};
        execvp("argo-daemon", args);
        _exit(1);  /* exec failed */
    } else if (pid > 0) {
        /* Parent - wait briefly for daemon startup */
        sleep(1);
        return ARGO_SUCCESS;
    }
    return E_SYSTEM_PROCESS;
}
```

## Constants (ci_constants.h)

```c
/* © 2025 Casey Koons All rights reserved */
#ifndef CI_CONSTANTS_H
#define CI_CONSTANTS_H

/* Buffer sizes */
#define CI_QUERY_MAX 8192           /* Max query length */
#define CI_RESPONSE_MAX 32768       /* Max response length */
#define CI_READ_CHUNK_SIZE 4096     /* Stdin read chunk */
#define CI_REQUEST_BUFFER_SIZE 16384 /* JSON request buffer */

/* HTTP timeouts */
#define CI_QUERY_TIMEOUT_SECONDS 120  /* 2 minutes for AI query */

/* Exit codes */
#define CI_EXIT_SUCCESS 0
#define CI_EXIT_ERROR 1

/* Default daemon URL */
#define CI_DEFAULT_DAEMON_URL "http://localhost:9876"

#endif /* CI_CONSTANTS_H */
```

## Configuration

CI uses daemon-side configuration from `~/.argo/config`:

```ini
# CI tool defaults (managed by daemon)
CI_DEFAULT_PROVIDER=claude_code
CI_DEFAULT_MODEL=claude-sonnet-4-5
```

**Configuration Precedence**:
1. Command-line flags (`--provider`, `--model`) - highest priority
2. Daemon configuration (`~/.argo/config`)
3. Built-in defaults - lowest priority

## Testing

### Integration Tests (requires daemon)
```bash
cd ci/tests
./test_ci_integration.sh

# Tests:
# - ci without daemon (should auto-start)
# - ci with command line query
# - ci with stdin query
# - ci with provider override
# - ci with model override
# - ci error handling
```

### Unit Tests
```bash
make test-ci-unit

# Tests HTTP client functions:
# - URL construction
# - JSON request building
# - Timeout values
# - Response parsing
```

## Common Use Cases

### In Bash Workflows
```bash
#!/bin/bash
# workflow_example.sh

# Query AI for code review
REVIEW=$(ci "Review this code: $(cat script.sh)")
echo "AI Review: $REVIEW"

# Multi-line query via heredoc
ANALYSIS=$(ci <<EOF
Analyze the following data:
- CPU: 80%
- Memory: 90%
- Disk: 75%
Suggest optimizations.
EOF
)
echo "$ANALYSIS"

# With specific provider
RESULT=$(ci --provider=claude_api --model=claude-opus-4 \
         "Generate a test plan for user authentication")
```

### Quick Queries
```bash
# Quick one-liner
ci "What time is it in Tokyo?"

# Pipe processing
cat error.log | ci "Summarize these errors"

# Chain with other tools
git diff | ci "Explain these changes" | tee review.txt
```

## Reminders

- **Keep it simple** - CI is intentionally minimal
- **No state** - Each invocation is independent
- **Daemon dependency** - CI requires argo-daemon running
- **Auto-start behavior** - CI starts daemon if not running
- **HTTP only** - Never bypass daemon to talk to providers
- **Fast startup** - Sub-100ms for simple queries (after daemon running)
- **Thin client** - Under 500 lines total
- **No configuration files** - Uses daemon config only

## Comparison with Arc

**Arc** (terminal-facing CLI):
- User-facing commands (workflow start, list, status)
- Interactive workflow management
- Progress tracking
- Full REST API client

**CI** (workflow query tool):
- Single-purpose AI query tool
- Non-interactive (command → response)
- No workflow management
- Minimal REST API usage (only /api/ci/query)

Both are thin HTTP clients to argo-daemon, but serve different purposes:
- **Arc**: Users interact with workflows
- **CI**: Workflows query AI providers

---

*CI makes AI accessible from bash workflows. Simple, fast, focused.*

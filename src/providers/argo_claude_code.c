/* © 2025 Casey Koons All rights reserved */

/* Claude Code Provider - In-Process with Streaming and Memory
 * Matches Tekton's proven implementation:
 * - In-process execution via subprocess with streaming output
 * - Memory digest integration (sundown/sunrise)
 * - Real-time streaming to stdout
 */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>

/* Project includes */
#include "argo_ci.h"
#include "argo_ci_defaults.h"
#include "argo_ci_common.h"
#include "argo_error.h"
#include "argo_error_messages.h"
#include "argo_log.h"
#include "argo_output.h"
#include "argo_limits.h"
#include "argo_memory.h"

/* Claude Code context structure */
typedef struct claude_code_context {
    /* Model configuration */
    char model[CLAUDE_CODE_MODEL_SIZE];

    /* Response buffer */
    char* response_content;
    size_t response_capacity;

    /* Memory digest for sundown/sunrise */
    ci_memory_digest_t* memory_digest;

    /* Statistics */
    uint64_t total_queries;
    time_t last_query;

    /* Provider interface (MUST be first for casting) */
    ci_provider_t provider;
} claude_code_context_t;

/* Forward declarations */
static int claude_code_init(ci_provider_t* provider);
static int claude_code_connect(ci_provider_t* provider);
static int claude_code_query(ci_provider_t* provider, const char* prompt,
                             ci_response_callback callback, void* userdata);
static int claude_code_stream(ci_provider_t* provider, const char* prompt,
                              ci_stream_callback callback, void* userdata);
static void claude_code_cleanup(ci_provider_t* provider);
static int claude_code_execute_with_streaming(claude_code_context_t* ctx,
                                               const char* prompt,
                                               const char* augmented_prompt);

/* Provider creation */
ci_provider_t* claude_code_create_provider(const char* model) {
    claude_code_context_t* ctx = calloc(1, sizeof(claude_code_context_t));
    if (!ctx) {
        argo_report_error(E_SYSTEM_MEMORY, "claude_code_create_provider", "allocation failed");
        return NULL;
    }

    /* Initialize base provider */
    init_provider_base(&ctx->provider, ctx, claude_code_init, claude_code_connect,
                      claude_code_query, claude_code_stream, claude_code_cleanup);

    /* Set model */
    strncpy(ctx->model, model ? model : "claude-sonnet-4", sizeof(ctx->model) - 1);

    /* Configure provider metadata */
    strncpy(ctx->provider.name, "claude_code", sizeof(ctx->provider.name) - 1);
    strncpy(ctx->provider.model, ctx->model, sizeof(ctx->provider.model) - 1);
    ctx->provider.supports_streaming = true;   /* Streaming visible to user */
    ctx->provider.supports_memory = true;      /* Memory digest integrated */
    ctx->provider.max_context = CLAUDE_CONTEXT_WINDOW;

    /* Allocate response buffer */
    ctx->response_capacity = CLAUDE_CODE_RESPONSE_BUFFER_SIZE;
    ctx->response_content = malloc(ctx->response_capacity);
    if (!ctx->response_content) {
        argo_report_error(E_SYSTEM_MEMORY, "claude_code_create_provider", "buffer allocation failed");
        free(ctx);
        return NULL;
    }

    /* Create memory digest (50% of context window) */
    ctx->memory_digest = memory_digest_create(ctx->provider.max_context);
    if (!ctx->memory_digest) {
        LOG_WARN("Failed to create memory digest, continuing without memory");
    }

    LOG_INFO("Created Claude Code provider (in-process with streaming and memory)");
    return &ctx->provider;
}

/* Initialize provider */
static int claude_code_init(ci_provider_t* provider) {
    ARGO_CHECK_NULL(provider);
    ARGO_GET_CONTEXT(provider, claude_code_context_t, ctx);

    ctx->total_queries = 0;
    ctx->last_query = 0;

    LOG_DEBUG("Claude Code provider initialized");
    return ARGO_SUCCESS;
}

/* Connect to provider */
static int claude_code_connect(ci_provider_t* provider) {
    ARGO_CHECK_NULL(provider);

    /* Check if 'claude' command exists */
    if (access("/usr/local/bin/claude", X_OK) != 0 &&
        system("which claude > /dev/null 2>&1") != 0) {
        argo_report_error(E_CI_NO_PROVIDER, "claude_code_connect",
                         "claude command not found in PATH");
        return E_CI_NO_PROVIDER;
    }

    LOG_DEBUG("Claude Code provider connected");
    return ARGO_SUCCESS;
}

/* Execute claude -p with streaming output and memory augmentation */
static int claude_code_query(ci_provider_t* provider, const char* prompt,
                             ci_response_callback callback, void* userdata) {
    ARGO_CHECK_NULL(prompt);
    ARGO_CHECK_NULL(callback);
    ARGO_GET_CONTEXT(provider, claude_code_context_t, ctx);

    /* Build augmented prompt with memory context */
    char* augmented_prompt = NULL;
    size_t augmented_size = strlen(prompt) + 1;  /* Start with prompt size */

    /* Add sunrise brief if available */
    if (ctx->memory_digest && ctx->memory_digest->sunrise_brief) {
        size_t sunrise_len = strlen(ctx->memory_digest->sunrise_brief);
        augmented_size += sunrise_len + MEMORY_AUGMENT_MARKER_OVERHEAD;
    }

    /* Allocate augmented prompt buffer */
    augmented_prompt = malloc(augmented_size);
    if (!augmented_prompt) {
        argo_report_error(E_SYSTEM_MEMORY, "claude_code_query", "failed to allocate augmented prompt");
        return E_SYSTEM_MEMORY;
    }

    /* Build prompt with context */
    size_t offset = 0;

    /* Add sunrise context if exists */
    if (ctx->memory_digest && ctx->memory_digest->sunrise_brief) {
        offset += snprintf(augmented_prompt + offset, augmented_size - offset,
                          "[Previous session context]\n%s\n\n",
                          ctx->memory_digest->sunrise_brief);
    }

    /* Add actual prompt */
    offset += snprintf(augmented_prompt + offset, augmented_size - offset,
                      "%s", prompt);

    /* Execute with streaming */
    int result = claude_code_execute_with_streaming(ctx, prompt, augmented_prompt);

    free(augmented_prompt);

    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Build response structure */
    ci_response_t response;
    build_ci_response(&response, true, ARGO_SUCCESS,
                     ctx->response_content, "claude-code-streaming");

    /* Update statistics */
    ctx->total_queries++;
    ctx->last_query = time(NULL);
    ARGO_UPDATE_STATS(ctx);

    /* Invoke callback */
    callback(&response, userdata);

    LOG_DEBUG("Claude Code query successful");
    return ARGO_SUCCESS;
}

/* Execute claude with streaming output visible to user */
static int claude_code_execute_with_streaming(claude_code_context_t* ctx,
                                               const char* prompt,
                                               const char* augmented_prompt) {
    (void)prompt;  /* Unused - augmented_prompt contains prompt + context */
    int result = ARGO_SUCCESS;
    int stdin_pipe[2] = {-1, -1};
    int stdout_pipe[2] = {-1, -1};
    pid_t pid = -1;

    /* Create pipes for stdin and stdout */
    if (pipe(stdin_pipe) < 0) {
        argo_report_error(E_SYSTEM_PROCESS, "claude_code_execute", "stdin pipe creation failed: %s", strerror(errno));
        return E_SYSTEM_PROCESS;
    }

    if (pipe(stdout_pipe) < 0) {
        argo_report_error(E_SYSTEM_PROCESS, "claude_code_execute", "stdout pipe creation failed: %s", strerror(errno));
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        return E_SYSTEM_PROCESS;
    }

    /* Fork subprocess */
    pid = fork();
    if (pid < 0) {
        argo_report_error(E_SYSTEM_FORK, "claude_code_execute", "fork failed: %s", strerror(errno));
        result = E_SYSTEM_FORK;
        goto cleanup_pipes;
    }

    if (pid == 0) {
        /* Child process: exec 'claude -p' (reads prompt from stdin) */
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);

        /* Keep stderr visible for debugging */

        /* Execute: claude -p (prompt comes from stdin) */
        execlp("claude", "claude", "-p", NULL);

        /* If exec fails */
        fprintf(stderr, "Failed to exec claude: %s\n", strerror(errno));
        exit(127);
    }

    /* Parent process: write prompt to stdin, then read stdout */
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    /* Write prompt to stdin */
    size_t prompt_len = strlen(augmented_prompt);
    LOG_DEBUG("Writing %zu bytes to Claude stdin", prompt_len);

    ssize_t written = write(stdin_pipe[1], augmented_prompt, prompt_len);
    if (written < 0 || (size_t)written != prompt_len) {
        argo_report_error(E_SYSTEM_PROCESS, "claude_code_execute", "failed to write prompt (wrote %zd of %zu): %s",
                         written, prompt_len, strerror(errno));
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        result = E_SYSTEM_PROCESS;
        goto cleanup_pipes;
    }

    LOG_DEBUG("Successfully wrote prompt to stdin, closing pipe");

    /* Close stdin to signal EOF to Claude */
    close(stdin_pipe[1]);

    /* Read response with streaming to stdout */
    size_t total_read = 0;
    ssize_t bytes_read;
    char read_buf[CLAUDE_CODE_READ_CHUNK_SIZE];

    LOG_USER_INFO("\n[Claude streaming...]\n");
    fflush(stdout);

    while ((bytes_read = read(stdout_pipe[0], read_buf, sizeof(read_buf))) > 0) {
        /* Write to stdout for user visibility */
        fwrite(read_buf, 1, bytes_read, stdout);
        fflush(stdout);

        /* Copy to response buffer */
        size_t space_left = ctx->response_capacity - total_read - 1;
        size_t to_copy = (bytes_read < (ssize_t)space_left) ? bytes_read : space_left;

        if (to_copy > 0) {
            memcpy(ctx->response_content + total_read, read_buf, to_copy);
            total_read += to_copy;
        }

        /* Check buffer overflow */
        if (total_read >= ctx->response_capacity - 1) {
            LOG_WARN("Response buffer full, output truncated");
            break;
        }
    }

    /* Null-terminate response */
    ctx->response_content[total_read] = '\0';

    LOG_USER_INFO("\n[End of Claude response]\n");
    fflush(stdout);

    /* Close pipe */
    close(stdout_pipe[0]);
    stdout_pipe[0] = -1;

    /* Wait for child */
    int status;
    if (waitpid(pid, &status, 0) < 0) {
        argo_report_error(E_SYSTEM_PROCESS, "claude_code_execute", "waitpid failed: %s", strerror(errno));
        result = E_SYSTEM_PROCESS;
        goto cleanup;
    }

    /* Check exit status */
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        argo_report_error(E_CI_CONFUSED, "claude_code_execute", "claude exited with code %d", exit_code);
        result = E_CI_CONFUSED;
        goto cleanup;
    }

    LOG_DEBUG("Claude streaming complete (%zu bytes)", total_read);
    return ARGO_SUCCESS;

cleanup_pipes:
    if (pid > 0) {
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
    }
    if (stdin_pipe[0] >= 0) close(stdin_pipe[0]);
    if (stdin_pipe[1] >= 0) close(stdin_pipe[1]);
    if (stdout_pipe[0] >= 0) close(stdout_pipe[0]);
    if (stdout_pipe[1] >= 0) close(stdout_pipe[1]);

cleanup:
    return result;
}

/* Streaming interface (same as query for Claude Code) */
static int claude_code_stream(ci_provider_t* provider, const char* prompt,
                              ci_stream_callback callback, void* userdata) {
    /* For Claude Code, streaming is built into query */
    (void)callback;
    (void)userdata;

    /* Just use the query interface which already streams */
    return claude_code_query(provider, prompt,
                            (ci_response_callback)callback, userdata);
}

/* Cleanup provider */
static void claude_code_cleanup(ci_provider_t* provider) {
    if (!provider) return;

    claude_code_context_t* ctx = (claude_code_context_t*)provider->context;
    if (!ctx) return;

    /* Save memory digest if exists */
    if (ctx->memory_digest) {
        /* TODO: Auto-save memory digest to session file */
        memory_digest_destroy(ctx->memory_digest);
    }

    free(ctx->response_content);
    free(ctx);

    LOG_DEBUG("Claude Code provider cleaned up");
}

/* Public API for memory management */

/* Set sunrise brief for next query */
int claude_code_set_sunrise(ci_provider_t* provider, const char* brief) {
    ARGO_CHECK_NULL(provider);
    ARGO_CHECK_NULL(brief);
    ARGO_GET_CONTEXT(provider, claude_code_context_t, ctx);

    if (!ctx->memory_digest) {
        LOG_WARN("No memory digest available");
        return E_INTERNAL_NOTIMPL;
    }

    return memory_set_sunrise_brief(ctx->memory_digest, brief);
}

/* Set sunset notes after query */
int claude_code_set_sunset(ci_provider_t* provider, const char* notes) {
    ARGO_CHECK_NULL(provider);
    ARGO_CHECK_NULL(notes);
    ARGO_GET_CONTEXT(provider, claude_code_context_t, ctx);

    if (!ctx->memory_digest) {
        LOG_WARN("No memory digest available");
        return E_INTERNAL_NOTIMPL;
    }

    return memory_set_sunset_notes(ctx->memory_digest, notes);
}

/* Get memory digest for inspection */
ci_memory_digest_t* claude_code_get_memory(ci_provider_t* provider) {
    if (!provider) return NULL;

    claude_code_context_t* ctx = (claude_code_context_t*)provider->context;
    if (!ctx) return NULL;

    return ctx->memory_digest;
}

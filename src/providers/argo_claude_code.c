/* © 2025 Casey Koons All rights reserved */

/* Claude Code Provider - Subprocess Implementation
 * Communicates with 'claude --print' via stdin/stdout pipes
 * Based on Tekton's proven implementation
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
#include "argo_limits.h"

/* Buffer sizes */
#define CLAUDE_CODE_RESPONSE_BUFFER_SIZE (1024 * 1024)  /* 1MB for response */
#define CLAUDE_CODE_MODEL_SIZE 128

/* Claude Code context structure */
typedef struct claude_code_context {
    /* Model configuration */
    char model[CLAUDE_CODE_MODEL_SIZE];

    /* Response buffer */
    char* response_content;
    size_t response_capacity;

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

    /* Set model (currently unused - Claude Code uses default) */
    strncpy(ctx->model, model ? model : "claude-sonnet-4", sizeof(ctx->model) - 1);

    /* Configure provider metadata */
    strncpy(ctx->provider.name, "claude_code", sizeof(ctx->provider.name) - 1);
    strncpy(ctx->provider.model, ctx->model, sizeof(ctx->provider.model) - 1);
    ctx->provider.supports_streaming = false;  /* No streaming in --print mode */
    ctx->provider.supports_memory = false;
    ctx->provider.max_context = 200000;  /* Claude's context window */

    /* Allocate response buffer */
    ctx->response_capacity = CLAUDE_CODE_RESPONSE_BUFFER_SIZE;
    ctx->response_content = malloc(ctx->response_capacity);
    if (!ctx->response_content) {
        argo_report_error(E_SYSTEM_MEMORY, "claude_code_create_provider", "buffer allocation failed");
        free(ctx);
        return NULL;
    }

    LOG_INFO("Created Claude Code provider (subprocess mode)");
    return &ctx->provider;
}

/* Initialize provider */
static int claude_code_init(ci_provider_t* provider) {
    ARGO_CHECK_NULL(provider);
    ARGO_GET_CONTEXT(provider, claude_code_context_t, ctx);

    ctx->total_queries = 0;
    ctx->last_query = 0;

    LOG_DEBUG("Claude Code provider initialized (subprocess mode)");
    return ARGO_SUCCESS;
}

/* Connect to provider (no-op for subprocess) */
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

/* Execute claude --print via subprocess */
static int claude_code_query(ci_provider_t* provider, const char* prompt,
                             ci_response_callback callback, void* userdata) {
    ARGO_CHECK_NULL(prompt);
    ARGO_CHECK_NULL(callback);
    ARGO_GET_CONTEXT(provider, claude_code_context_t, ctx);

    int result = ARGO_SUCCESS;
    int stdin_pipe[2] = {-1, -1};
    int stdout_pipe[2] = {-1, -1};
    pid_t pid = -1;

    /* Create pipes for stdin/stdout */
    if (pipe(stdin_pipe) < 0) {
        argo_report_error(E_SYSTEM_PROCESS, "claude_code_query", "stdin pipe failed: %s", strerror(errno));
        return E_SYSTEM_PROCESS;
    }

    if (pipe(stdout_pipe) < 0) {
        argo_report_error(E_SYSTEM_PROCESS, "claude_code_query", "stdout pipe failed: %s", strerror(errno));
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        return E_SYSTEM_PROCESS;
    }

    /* Fork subprocess */
    pid = fork();
    if (pid < 0) {
        argo_report_error(E_SYSTEM_FORK, "claude_code_query", "fork failed: %s", strerror(errno));
        result = E_SYSTEM_FORK;
        goto cleanup_pipes;
    }

    if (pid == 0) {
        /* Child process: exec 'claude --print' */

        /* Redirect stdin from pipe */
        dup2(stdin_pipe[0], STDIN_FILENO);
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);

        /* Redirect stdout to pipe */
        dup2(stdout_pipe[1], STDOUT_FILENO);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);

        /* Redirect stderr to /dev/null (suppress Claude's internal logging) */
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }

        /* Execute claude --print */
        execlp("claude", "claude", "--print", NULL);

        /* If exec fails */
        fprintf(stderr, "Failed to exec claude: %s\n", strerror(errno));
        exit(127);
    }

    /* Parent process: communicate with child */

    /* Close unused ends of pipes */
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    /* Write prompt to child's stdin */
    size_t prompt_len = strlen(prompt);
    ssize_t written = write(stdin_pipe[1], prompt, prompt_len);
    if (written < 0 || (size_t)written != prompt_len) {
        argo_report_error(E_SYSTEM_PROCESS, "claude_code_query", "write to stdin failed: %s", strerror(errno));
        result = E_SYSTEM_PROCESS;
        goto cleanup_process;
    }

    /* Close stdin to signal EOF */
    close(stdin_pipe[1]);
    stdin_pipe[1] = -1;

    /* Read response from child's stdout */
    size_t total_read = 0;
    ssize_t bytes_read;

    while ((bytes_read = read(stdout_pipe[0],
                              ctx->response_content + total_read,
                              ctx->response_capacity - total_read - 1)) > 0) {
        total_read += bytes_read;

        /* Check if buffer is full */
        if (total_read >= ctx->response_capacity - 1) {
            argo_report_error(E_INPUT_TOO_LARGE, "claude_code_query", "response too large");
            result = E_INPUT_TOO_LARGE;
            goto cleanup_process;
        }
    }

    if (bytes_read < 0) {
        argo_report_error(E_SYSTEM_PROCESS, "claude_code_query", "read from stdout failed: %s", strerror(errno));
        result = E_SYSTEM_PROCESS;
        goto cleanup_process;
    }

    /* Null-terminate response */
    ctx->response_content[total_read] = '\0';

    /* Close stdout pipe */
    close(stdout_pipe[0]);
    stdout_pipe[0] = -1;

    /* Wait for child to exit */
    int status;
    if (waitpid(pid, &status, 0) < 0) {
        argo_report_error(E_SYSTEM_PROCESS, "claude_code_query", "waitpid failed: %s", strerror(errno));
        result = E_SYSTEM_PROCESS;
        goto cleanup;
    }

    /* Check exit status */
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        argo_report_error(E_CI_CONFUSED, "claude_code_query", "claude exited with code %d", exit_code);
        result = E_CI_CONFUSED;
        goto cleanup;
    }

    /* Build response structure */
    ci_response_t response;
    build_ci_response(&response, true, ARGO_SUCCESS,
                     ctx->response_content, "claude-code-subprocess");

    /* Update statistics */
    ctx->total_queries++;
    ctx->last_query = time(NULL);
    ARGO_UPDATE_STATS(ctx);

    /* Invoke callback */
    callback(&response, userdata);

    LOG_DEBUG("Claude Code query successful (%zu bytes)", total_read);
    return ARGO_SUCCESS;

cleanup_process:
    /* Kill child if still running */
    if (pid > 0) {
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
    }

cleanup_pipes:
    if (stdin_pipe[0] >= 0) close(stdin_pipe[0]);
    if (stdin_pipe[1] >= 0) close(stdin_pipe[1]);
    if (stdout_pipe[0] >= 0) close(stdout_pipe[0]);
    if (stdout_pipe[1] >= 0) close(stdout_pipe[1]);

cleanup:
    return result;
}

/* Streaming not supported in --print mode */
static int claude_code_stream(ci_provider_t* provider, const char* prompt,
                              ci_stream_callback callback, void* userdata) {
    (void)provider;
    (void)prompt;
    (void)callback;
    (void)userdata;

    argo_report_error(E_INTERNAL_NOTIMPL, "claude_code_stream", "streaming not supported in --print mode");
    return E_INTERNAL_NOTIMPL;
}

/* Cleanup provider */
static void claude_code_cleanup(ci_provider_t* provider) {
    if (!provider) return;

    claude_code_context_t* ctx = (claude_code_context_t*)provider->context;
    if (!ctx) return;

    free(ctx->response_content);
    free(ctx);

    LOG_DEBUG("Claude Code provider cleaned up");
}

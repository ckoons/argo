/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

/* Project includes */
#include "argo_claude_process.h"
#include "argo_error.h"
#include "argo_error_messages.h"
#include "argo_log.h"
#include "argo_limits.h"

/* Spawn Claude subprocess */
int spawn_claude_process(claude_context_t* ctx) {
    /* Create pipes */
    if (pipe(ctx->stdin_pipe) < 0 ||
        pipe(ctx->stdout_pipe) < 0 ||
        pipe(ctx->stderr_pipe) < 0) {
        argo_report_error(E_SYSTEM_FORK, "spawn_claude_process", ERR_FMT_SYSCALL_ERROR, ERR_MSG_PIPE_FAILED, strerror(errno));
        return E_SYSTEM_FORK;
    }

    /* Fork process */
    ctx->claude_pid = fork();
    if (ctx->claude_pid < 0) {
        argo_report_error(E_SYSTEM_FORK, "spawn_claude_process", ERR_FMT_SYSCALL_ERROR, ERR_MSG_FORK_FAILED, strerror(errno));
        return E_SYSTEM_FORK;
    }

    if (ctx->claude_pid == 0) {
        /* Child process - become Claude */

        /* Redirect stdin/stdout/stderr */
        dup2(ctx->stdin_pipe[0], STDIN_FILENO);
        dup2(ctx->stdout_pipe[1], STDOUT_FILENO);
        dup2(ctx->stderr_pipe[1], STDERR_FILENO);

        /* Close unused pipe ends */
        close(ctx->stdin_pipe[1]);
        close(ctx->stdout_pipe[0]);
        close(ctx->stderr_pipe[0]);

        /* Execute Claude */
        execlp("claude", "claude", NULL);

        /* If we get here, exec failed */
        /* GUIDELINE_APPROVED: Child process error before exit, stderr redirected to pipe */
        fprintf(stderr, CLAUDE_EXEC_FAILED_MSG "%s\n", strerror(errno));
        exit(1);
    }

    /* Parent process - close unused pipe ends */
    close(ctx->stdin_pipe[0]);
    close(ctx->stdout_pipe[1]);
    close(ctx->stderr_pipe[1]);

    /* Make pipes non-blocking */
    fcntl(ctx->stdout_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(ctx->stderr_pipe[0], F_SETFL, O_NONBLOCK);

    LOG_INFO("Spawned Claude process with PID %d", ctx->claude_pid);
    return ARGO_SUCCESS;
}

/* Kill Claude process */
int kill_claude_process(claude_context_t* ctx) {
    if (ctx->claude_pid <= 0) {
        return ARGO_SUCCESS;
    }

    /* Send SIGTERM */
    kill(ctx->claude_pid, SIGTERM);

    /* Wait for process to exit */
    int status;
    waitpid(ctx->claude_pid, &status, 0);

    /* Close pipes */
    if (ctx->stdin_pipe[1] >= 0) close(ctx->stdin_pipe[1]);
    if (ctx->stdout_pipe[0] >= 0) close(ctx->stdout_pipe[0]);
    if (ctx->stderr_pipe[0] >= 0) close(ctx->stderr_pipe[0]);

    ctx->claude_pid = -1;
    LOG_DEBUG("Claude process terminated");
    return ARGO_SUCCESS;
}

/* Write input to Claude */
int write_to_claude(claude_context_t* ctx, const char* input) {
    size_t len = strlen(input);
    ssize_t written = write(ctx->stdin_pipe[1], input, len);
    if (written != (ssize_t)len) {
        return E_SYSTEM_SOCKET;
    }
    write(ctx->stdin_pipe[1], "\n", 1);
    return ARGO_SUCCESS;
}

/* Read response from Claude */
int read_from_claude(claude_context_t* ctx, char** output, int timeout_ms) {
    /* Poll for response */
    struct pollfd pfd = {
        .fd = ctx->stdout_pipe[0],
        .events = POLLIN
    };

    /* Simple implementation - real version would accumulate response */
    if (poll(&pfd, 1, timeout_ms) <= 0) {
        return E_CI_TIMEOUT;
    }

    ctx->response_size = 0;
    char buffer[ARGO_BUFFER_STANDARD];
    ssize_t bytes;

    while ((bytes = read(ctx->stdout_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
        /* Grow response buffer if needed */
        if (ctx->response_size + bytes >= ctx->response_capacity) {
            ctx->response_capacity *= 2;
            ctx->response_buffer = realloc(ctx->response_buffer, ctx->response_capacity);
        }
        memcpy(ctx->response_buffer + ctx->response_size, buffer, bytes);
        ctx->response_size += bytes;
    }

    ctx->response_buffer[ctx->response_size] = '\0';
    *output = ctx->response_buffer;
    return ARGO_SUCCESS;
}

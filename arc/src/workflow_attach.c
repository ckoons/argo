/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <errno.h>
#include "arc_commands.h"
#include "arc_context.h"
#include "arc_error.h"
#include "arc_constants.h"
#include "arc_http_client.h"
#include "argo_error.h"
#include "argo_output.h"

#define INPUT_BUFFER_SIZE 4096

/* Thread synchronization */
typedef struct {
    const char* workflow_id;
    pthread_t input_thread;
    volatile bool should_stop;
    off_t* final_cursor;
} attach_context_t;

/* Get cursor environment variable name for workflow */
static void get_cursor_env_name(const char* workflow_id, char* env_name, size_t size) {
    snprintf(env_name, size, "ARGO_CURSOR_%s", workflow_id);
}

/* Get cursor position for this terminal */
static off_t get_cursor_position(const char* workflow_id) {
    char env_name[ARC_ENV_NAME_MAX];
    get_cursor_env_name(workflow_id, env_name, sizeof(env_name));

    const char* cursor_str = getenv(env_name);
    if (!cursor_str) {
        return 0;  /* Start from beginning */
    }

    char* endptr = NULL;
    long pos = strtol(cursor_str, &endptr, 10);
    if (endptr == cursor_str || pos < 0) {
        return 0;  /* Invalid cursor, start from beginning */
    }

    return (off_t)pos;
}

/* Set cursor position for this terminal */
static void set_cursor_position(const char* workflow_id, off_t position) {
    char env_name[ARC_ENV_NAME_MAX];
    char position_str[32];

    get_cursor_env_name(workflow_id, env_name, sizeof(env_name));
    snprintf(position_str, sizeof(position_str), "%ld", (long)position);

    setenv(env_name, position_str, 1);
}

/* Get log file path for workflow */
static int get_log_path(const char* workflow_id, char* path, size_t path_size) {
    const char* home = getenv("HOME");
    if (!home) {
        home = ".";
    }

    snprintf(path, path_size, "%s/.argo/logs/%s.log", home, workflow_id);
    return ARGO_SUCCESS;
}

/* Send user input to daemon via HTTP */
static int send_input_to_daemon(const char* workflow_id, const char* input) {
    char endpoint[512];
    char json_body[INPUT_BUFFER_SIZE + 100];

    snprintf(endpoint, sizeof(endpoint), "/api/workflow/input?workflow_name=%s", workflow_id);
    snprintf(json_body, sizeof(json_body), "{\"input\":\"%s\"}", input);

    arc_http_response_t* response = NULL;
    int result = arc_http_post(endpoint, json_body, &response);
    if (result != ARGO_SUCCESS || !response) {
        LOG_USER_ERROR("Failed to connect to daemon for input\n");
        return -1;
    }

    if (response->status_code != 200) {
        LOG_USER_ERROR("Daemon rejected input (HTTP %d)\n", response->status_code);
        if (response->body) {
            LOG_USER_ERROR("  Response: %s\n", response->body);
        }
        arc_http_response_free(response);
        return -1;
    }

    arc_http_response_free(response);
    return 0;
}

/* Input thread - reads stdin and sends to daemon via HTTP */
static void* input_thread_func(void* arg) {
    attach_context_t* ctx = (attach_context_t*)arg;
    char buffer[INPUT_BUFFER_SIZE];

    while (!ctx->should_stop) {
        /* Read line from stdin */
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            break;  /* EOF or error */
        }

        /* Remove trailing newline if present */
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            buffer[len-1] = '\0';
            len--;
        }

        /* Check for detach command (Ctrl+D results in empty line) */
        if (len == 0) {
            ctx->should_stop = true;
            break;
        }

        /* Send input via HTTP */
        if (send_input_to_daemon(ctx->workflow_id, buffer) < 0) {
            if (!ctx->should_stop) {
                LOG_USER_ERROR("Failed to send input to daemon\n");
            }
            break;
        }
    }

    return NULL;
}

/* Read and display log from cursor position, following until EOF */
static int display_log_backlog(const char* log_path, off_t start_pos, off_t* end_pos) {
    int fd = open(log_path, O_RDONLY);
    if (fd < 0) {
        if (errno == ENOENT) {
            LOG_USER_INFO("No log file yet (workflow may be starting)\n");
            *end_pos = 0;
            return ARGO_SUCCESS;
        }
        LOG_USER_ERROR("Failed to open log file: %s\n", strerror(errno));
        return E_SYSTEM_FILE;
    }

    /* Seek to cursor position */
    if (lseek(fd, start_pos, SEEK_SET) < 0) {
        close(fd);
        LOG_USER_ERROR("Failed to seek in log file: %s\n", strerror(errno));
        return E_SYSTEM_FILE;
    }

    /* Read and display from cursor to EOF */
    char buffer[ARC_READ_CHUNK_SIZE];
    ssize_t bytes_read;
    off_t current_pos = start_pos;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        /* Write directly to stdout */
        if (write(STDOUT_FILENO, buffer, bytes_read) != bytes_read) {
            close(fd);
            return E_SYSTEM_FILE;
        }
        current_pos += bytes_read;
    }

    close(fd);
    *end_pos = current_pos;

    return ARGO_SUCCESS;
}

/* Follow log file with simultaneous input thread */
static int follow_log_stream(const char* log_path, off_t start_pos, off_t* final_pos, const char* workflow_id) {
    int result;
    off_t current_pos = start_pos;

    /* First, display backlog from cursor to current EOF */
    result = display_log_backlog(log_path, current_pos, &current_pos);
    if (result != ARGO_SUCCESS) {
        *final_pos = current_pos;
        return result;
    }

    /* Setup attach context for threads */
    attach_context_t ctx = {
        .workflow_id = workflow_id,
        .should_stop = false,
        .final_cursor = final_pos
    };

    /* Start input thread for HTTP-based user input */
    if (pthread_create(&ctx.input_thread, NULL, input_thread_func, &ctx) != 0) {
        LOG_USER_INFO("Failed to create input thread - output only mode\n");
        /* Continue without input thread */
    }

    /* Now follow like 'tail -f' until input thread signals stop */
    int fd = open(log_path, O_RDONLY);
    if (fd < 0) {
        *final_pos = current_pos;
        return ARGO_SUCCESS;  /* No file yet, that's ok */
    }

    /* Seek to current position */
    lseek(fd, current_pos, SEEK_SET);

    LOG_USER_INFO("\n[Streaming output - Press Ctrl+D on empty line to detach]\n");

    char buffer[ARC_READ_CHUNK_SIZE];

    /* Loop until input thread signals stop (via ctx.should_stop) */
    while (!ctx.should_stop) {
        /* Read new data from log */
        ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            /* Write to stdout */
            write(STDOUT_FILENO, buffer, bytes_read);
            current_pos += bytes_read;
        } else {
            /* No new data, sleep briefly */
            usleep(100000);  /* 100ms */
        }
    }

    /* Wait for input thread to finish */
    pthread_join(ctx.input_thread, NULL);

    close(fd);
    *final_pos = current_pos;

    return ARGO_SUCCESS;
}

/* arc workflow attach command handler */
int arc_workflow_attach(int argc, char** argv) {
    const char* workflow_id = NULL;

    /* Get workflow ID from args or environment */
    if (argc >= 1) {
        workflow_id = argv[0];
    } else {
        /* Try to get from ARGO_ACTIVE_WORKFLOW */
        workflow_id = getenv("ARGO_ACTIVE_WORKFLOW");
        if (!workflow_id) {
            LOG_USER_ERROR("No workflow specified and no active workflow set\n");
            LOG_USER_INFO("Usage: arc attach <workflow_id>\n");
            LOG_USER_INFO("   or: arc switch <workflow_id> (to set active workflow)\n");
            return ARC_EXIT_ERROR;
        }
    }

    /* Get log path */
    char log_path[512];
    get_log_path(workflow_id, log_path, sizeof(log_path));

    /* Check if log file exists (workflow must have been started) */
    struct stat st;
    if (stat(log_path, &st) < 0) {
        LOG_USER_ERROR("Workflow log not found: %s\n", workflow_id);
        LOG_USER_INFO("  Log path: %s\n", log_path);
        LOG_USER_INFO("  Workflow may not exist or hasn't started yet\n");
        LOG_USER_INFO("Try: arc workflow list\n");
        return ARC_EXIT_ERROR;
    }

    int result;

    /* Get cursor position for this terminal */
    off_t cursor = get_cursor_position(workflow_id);

    LOG_USER_INFO("Attaching to workflow: %s\n", workflow_id);
    if (cursor > 0) {
        LOG_USER_INFO("(Resuming from position %ld)\n\n", (long)cursor);
    } else {
        LOG_USER_INFO("(Starting from beginning)\n\n");
    }

    /* Follow log stream until user presses Enter */
    off_t final_pos = cursor;
    result = follow_log_stream(log_path, cursor, &final_pos, workflow_id);

    /* Update cursor position ONLY on clean detach */
    if (result == ARGO_SUCCESS) {
        set_cursor_position(workflow_id, final_pos);
        LOG_USER_INFO("\n[Detached from workflow: %s]\n", workflow_id);
    }

    return (result == ARGO_SUCCESS) ? ARC_EXIT_SUCCESS : ARC_EXIT_ERROR;
}

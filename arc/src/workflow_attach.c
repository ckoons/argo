/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include "arc_commands.h"
#include "arc_context.h"
#include "arc_error.h"
#include "arc_constants.h"
#include "argo_workflow_registry.h"
#include "argo_init.h"
#include "argo_error.h"
#include "argo_output.h"

#define WORKFLOW_REGISTRY_PATH ".argo/workflows/registry/active_workflow_registry.json"

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

    return (off_t)atol(cursor_str);
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

/* Send user input to workflow via socket */
static int send_input_to_workflow(const char* workflow_id, const char* input) {
    const char* home = getenv("HOME");
    if (!home) home = ".";

    /* Build socket path */
    char socket_path[512];
    snprintf(socket_path, sizeof(socket_path),
            "%s/.argo/sockets/%s.sock", home, workflow_id);

    /* Connect to socket */
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        LOG_USER_ERROR("Failed to create socket: %s\n", strerror(errno));
        return E_SYSTEM_SOCKET;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_USER_ERROR("Failed to connect to workflow socket: %s\n", strerror(errno));
        close(sock);
        return E_SYSTEM_SOCKET;
    }

    /* Send input with newline */
    size_t input_len = strlen(input);
    ssize_t written = write(sock, input, input_len);
    if (written < 0 || (size_t)written != input_len) {
        LOG_USER_ERROR("Failed to send input: %s\n", strerror(errno));
        close(sock);
        return E_SYSTEM_SOCKET;
    }

    /* Send newline */
    write(sock, "\n", 1);

    close(sock);
    return ARGO_SUCCESS;
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

/* Follow log file until user presses Enter (streaming mode) */
static int follow_log_stream(const char* log_path, off_t start_pos, off_t* final_pos, const char* workflow_id) {
    int result;
    off_t current_pos = start_pos;

    /* First, display backlog from cursor to current EOF */
    result = display_log_backlog(log_path, current_pos, &current_pos);
    if (result != ARGO_SUCCESS) {
        *final_pos = current_pos;
        return result;
    }

    /* Now follow like 'tail -f' until Enter pressed */
    int fd = open(log_path, O_RDONLY);
    if (fd < 0) {
        *final_pos = current_pos;
        return ARGO_SUCCESS;  /* No file yet, that's ok */
    }

    /* Set stdin to non-blocking for Enter detection */
    int stdin_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, stdin_flags | O_NONBLOCK);

    /* Seek to current position */
    lseek(fd, current_pos, SEEK_SET);

    LOG_USER_INFO("\n[Streaming output - Press Ctrl+D to detach]\n");

    char buffer[ARC_READ_CHUNK_SIZE];
    char input_char;
    bool should_exit = false;
    char line_buffer[1024] = {0};
    int line_pos = 0;

    while (!should_exit) {
        /* Check for Ctrl+D (EOF) */
        ssize_t bytes = read(STDIN_FILENO, &input_char, 1);
        if (bytes > 0) {
            if (input_char == 4) {  /* Ctrl+D is ASCII 4 (EOT) */
                should_exit = true;
                break;
            }
        } else if (bytes == 0) {
            /* EOF from stdin (Ctrl+D) */
            should_exit = true;
            break;
        }

        /* Read new data from log */
        ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            /* Write to stdout */
            write(STDOUT_FILENO, buffer, bytes_read);
            current_pos += bytes_read;

            /* Check for [WAITING_FOR_INPUT] marker line-by-line */
            for (ssize_t i = 0; i < bytes_read; i++) {
                if (buffer[i] == '\n') {
                    line_buffer[line_pos] = '\0';

                    /* Check if this line contains waiting marker */
                    if (strstr(line_buffer, "[WAITING_FOR_INPUT")) {
                        /* Extract prompt if provided */
                        char* prompt_start = strchr(line_buffer, ':');
                        char* prompt_end = strchr(line_buffer, ']');

                        /* Restore stdin to blocking mode temporarily */
                        fcntl(STDIN_FILENO, F_SETFL, stdin_flags);

                        /* Show prompt */
                        if (prompt_start && prompt_end && prompt_start < prompt_end) {
                            prompt_start++; /* Skip ':' */
                            *prompt_end = '\0';
                            printf("%s ", prompt_start);
                        } else {
                            printf("You: ");
                        }
                        fflush(stdout);

                        /* Read user input */
                        char user_input[4096];
                        if (fgets(user_input, sizeof(user_input), stdin)) {
                            /* Remove trailing newline */
                            size_t len = strlen(user_input);
                            if (len > 0 && user_input[len - 1] == '\n') {
                                user_input[len - 1] = '\0';
                            }

                            /* Send to workflow via socket */
                            send_input_to_workflow(workflow_id, user_input);
                        }

                        /* Restore non-blocking mode */
                        fcntl(STDIN_FILENO, F_SETFL, stdin_flags | O_NONBLOCK);
                    }

                    line_pos = 0;
                } else if (line_pos < (int)sizeof(line_buffer) - 1) {
                    line_buffer[line_pos++] = buffer[i];
                }
            }
        } else {
            /* No new data, sleep briefly */
            usleep(100000);  /* 100ms */
        }
    }

    /* Restore stdin flags */
    fcntl(STDIN_FILENO, F_SETFL, stdin_flags);

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

    /* Initialize argo */
    int result = argo_init();
    if (result != ARGO_SUCCESS) {
        LOG_USER_ERROR("Failed to initialize argo\n");
        return ARC_EXIT_ERROR;
    }

    /* Load workflow registry */
    workflow_registry_t* registry = workflow_registry_create(WORKFLOW_REGISTRY_PATH);
    if (!registry) {
        LOG_USER_ERROR("Failed to create workflow registry\n");
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    result = workflow_registry_load(registry);
    if (result != ARGO_SUCCESS) {
        LOG_USER_ERROR("Failed to load workflow registry\n");
        workflow_registry_destroy(registry);
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    /* Verify workflow exists */
    workflow_instance_t* wf = workflow_registry_get_workflow(registry, workflow_id);
    if (!wf) {
        LOG_USER_ERROR("Workflow not found: %s\n", workflow_id);
        LOG_USER_INFO("Try: arc list\n");
        workflow_registry_destroy(registry);
        argo_exit();
        return ARC_EXIT_ERROR;
    }

    /* Get log path */
    char log_path[512];
    get_log_path(workflow_id, log_path, sizeof(log_path));

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

    /* Cleanup */
    workflow_registry_destroy(registry);
    argo_exit();

    return (result == ARGO_SUCCESS) ? ARC_EXIT_SUCCESS : ARC_EXIT_ERROR;
}

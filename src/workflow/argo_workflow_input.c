/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

/* Project includes */
#include "argo_workflow_input.h"
#include "argo_error.h"
#include "argo_log.h"
#include "argo_limits.h"

/* Create input socket for workflow */
workflow_input_socket_t* workflow_input_create(const char* workflow_id) {
    if (!workflow_id) {
        argo_report_error(E_INPUT_NULL, "workflow_input_create", "workflow_id is NULL");
        return NULL;
    }

    workflow_input_socket_t* sock = calloc(1, sizeof(workflow_input_socket_t));
    if (!sock) {
        argo_report_error(E_SYSTEM_MEMORY, "workflow_input_create", "allocation failed");
        return NULL;
    }

    /* Copy workflow ID */
    strncpy(sock->workflow_id, workflow_id, sizeof(sock->workflow_id) - 1);

    /* Build socket path: ~/.argo/sockets/<workflow_id>.sock */
    const char* home = getenv("HOME");
    if (!home) home = ".";

    snprintf(sock->socket_path, sizeof(sock->socket_path),
            "%s/.argo/sockets/%s.sock", home, workflow_id);

    /* Create sockets directory if needed */
    char sockets_dir[ARGO_PATH_MAX];
    snprintf(sockets_dir, sizeof(sockets_dir), "%s/.argo/sockets", home);
    mkdir(sockets_dir, ARGO_DIR_PERMISSIONS);

    /* Remove old socket file if exists */
    unlink(sock->socket_path);

    /* Create Unix domain socket */
    sock->socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock->socket_fd < 0) {
        argo_report_error(E_SYSTEM_SOCKET, "workflow_input_create",
                         "socket() failed: %s", strerror(errno));
        free(sock);
        return NULL;
    }

    /* Bind to socket path */
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock->socket_path, sizeof(addr.sun_path) - 1);

    if (bind(sock->socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        argo_report_error(E_SYSTEM_SOCKET, "workflow_input_create",
                         "bind() failed: %s", strerror(errno));
        close(sock->socket_fd);
        free(sock);
        return NULL;
    }

    /* Listen for connections */
    if (listen(sock->socket_fd, 1) < 0) {
        argo_report_error(E_SYSTEM_SOCKET, "workflow_input_create",
                         "listen() failed: %s", strerror(errno));
        close(sock->socket_fd);
        unlink(sock->socket_path);
        free(sock);
        return NULL;
    }

    LOG_DEBUG("Created input socket: %s", sock->socket_path);
    return sock;
}

/* Wait for and read one line of input from socket */
int workflow_input_read_line(workflow_input_socket_t* socket,
                              char* buffer, size_t buffer_size) {
    if (!socket || !buffer || buffer_size == 0) {
        argo_report_error(E_INPUT_NULL, "workflow_input_read_line", "invalid parameters");
        return -1;
    }

    /* Accept connection from arc client */
    int client_fd = accept(socket->socket_fd, NULL, NULL);
    if (client_fd < 0) {
        argo_report_error(E_SYSTEM_SOCKET, "workflow_input_read_line",
                         "accept() failed: %s", strerror(errno));
        return -1;
    }

    /* Read until newline or buffer full */
    size_t total_read = 0;
    while (total_read < buffer_size - 1) {
        char c;
        ssize_t n = read(client_fd, &c, 1);

        if (n < 0) {
            argo_report_error(E_SYSTEM_SOCKET, "workflow_input_read_line",
                             "read() failed: %s", strerror(errno));
            close(client_fd);
            return -1;
        }

        if (n == 0) {
            /* EOF */
            break;
        }

        if (c == '\n') {
            /* End of line */
            break;
        }

        buffer[total_read++] = c;
    }

    buffer[total_read] = '\0';
    close(client_fd);

    LOG_DEBUG("Read input: %zu bytes", total_read);
    return (int)total_read;
}

/* Destroy input socket and cleanup */
void workflow_input_destroy(workflow_input_socket_t* socket) {
    if (!socket) return;

    if (socket->socket_fd >= 0) {
        close(socket->socket_fd);
    }

    unlink(socket->socket_path);
    free(socket);

    LOG_DEBUG("Destroyed input socket");
}

/* Log that workflow is waiting for input */
void workflow_input_log_waiting(const char* prompt) {
    /* Special marker that arc attach can detect */
    printf("\n[WAITING_FOR_INPUT");
    if (prompt) {
        printf(":%s", prompt);
    }
    printf("]\n");
    fflush(stdout);
}

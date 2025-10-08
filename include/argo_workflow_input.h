/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_WORKFLOW_INPUT_H
#define ARGO_WORKFLOW_INPUT_H

#include <stddef.h>

/* Input socket infrastructure for interactive workflows
 *
 * Workflows can create an input socket to receive user input from arc CLI.
 * This enables interactive workflows while maintaining daemon architecture.
 */

/* Maximum input line size */
#define WORKFLOW_INPUT_BUFFER_SIZE 4096

/* Input socket context */
typedef struct workflow_input_socket {
    int socket_fd;           /* Socket file descriptor */
    char socket_path[512];   /* Path to socket file */
    char workflow_id[256];   /* Workflow ID for this socket */
} workflow_input_socket_t;

/* Create input socket for workflow
 *
 * Creates Unix socket at ~/.argo/sockets/<workflow_id>.sock
 * Logs marker to indicate workflow is waiting for input
 *
 * Parameters:
 *   workflow_id - ID of the workflow
 *
 * Returns:
 *   Allocated socket context on success, NULL on failure
 */
workflow_input_socket_t* workflow_input_create(const char* workflow_id);

/* Wait for and read one line of input from socket
 *
 * Blocks until arc client sends input via socket.
 * Returns when complete line received (terminated by newline).
 *
 * Parameters:
 *   socket - Socket context
 *   buffer - Buffer to store input
 *   buffer_size - Size of buffer
 *
 * Returns:
 *   Number of bytes read (excluding newline) on success
 *   0 on EOF (client disconnected)
 *   -1 on error
 */
int workflow_input_read_line(workflow_input_socket_t* socket,
                              char* buffer, size_t buffer_size);

/* Destroy input socket and cleanup
 *
 * Closes socket, removes socket file
 *
 * Parameters:
 *   socket - Socket context to destroy
 */
void workflow_input_destroy(workflow_input_socket_t* socket);

/* Log that workflow is waiting for input
 *
 * Writes special marker to stdout so arc attach knows to prompt user
 *
 * Parameters:
 *   prompt - Optional prompt to display (NULL for default)
 */
void workflow_input_log_waiting(const char* prompt);

#endif /* ARGO_WORKFLOW_INPUT_H */

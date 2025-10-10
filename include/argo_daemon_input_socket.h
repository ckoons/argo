/* © 2025 Casey Koons All rights reserved */
/* Daemon Input Socket Handler - TCP connections for user input */

#ifndef ARGO_DAEMON_INPUT_SOCKET_H
#define ARGO_DAEMON_INPUT_SOCKET_H

#include <stdint.h>
#include "argo_daemon.h"

/* Initialize input socket subsystem */
int input_socket_init(argo_daemon_t* daemon);

/* Handle a JSON input socket connection (called from HTTP server) */
void input_socket_handle_connection(int client_fd);

/* Shutdown input socket subsystem and close all connections */
void input_socket_shutdown(void);

#endif /* ARGO_DAEMON_INPUT_SOCKET_H */

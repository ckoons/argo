/* © 2025 Casey Koons All rights reserved */
#ifndef ARGO_DAEMON_API_H
#define ARGO_DAEMON_API_H

#include "argo_http_server.h"
#include "argo_daemon.h"

/* TODO: Unix pivot - workflow API handlers removed, will be replaced with bash-based workflows */
/* Stub declarations for API handlers that were deleted with workflow engine */

/* Global daemon context (defined in argo_daemon_api_routes.c) */
extern argo_daemon_t* g_api_daemon;

/* Register API routes */
int argo_daemon_register_api_routes(argo_daemon_t* daemon);

#endif /* ARGO_DAEMON_API_H */

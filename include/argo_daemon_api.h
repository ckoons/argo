/* © 2025 Casey Koons All rights reserved */
#ifndef ARGO_DAEMON_API_H
#define ARGO_DAEMON_API_H

#include "argo_http_server.h"
#include "argo_daemon.h"

/* Global daemon context (defined in argo_daemon_api_routes.c) */
extern argo_daemon_t* g_api_daemon;

/* Workflow API handlers (bash-based) */
int api_workflow_start(http_request_t* req, http_response_t* resp);
int api_workflow_list(http_request_t* req, http_response_t* resp);
int api_workflow_status(http_request_t* req, http_response_t* resp);
int api_workflow_abandon(http_request_t* req, http_response_t* resp);
int api_workflow_pause(http_request_t* req, http_response_t* resp);
int api_workflow_resume(http_request_t* req, http_response_t* resp);
int api_workflow_input(http_request_t* req, http_response_t* resp);

/* Register API routes */
int argo_daemon_register_api_routes(argo_daemon_t* daemon);

#endif /* ARGO_DAEMON_API_H */

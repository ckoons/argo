/* © 2025 Casey Koons All rights reserved */
#ifndef ARGO_DAEMON_API_H
#define ARGO_DAEMON_API_H

#include "argo_http_server.h"
#include "argo_daemon.h"

/* Workflow API handlers */
int api_workflow_start(http_request_t* req, http_response_t* resp);
int api_workflow_list(http_request_t* req, http_response_t* resp);
int api_workflow_status(http_request_t* req, http_response_t* resp);
int api_workflow_pause(http_request_t* req, http_response_t* resp);
int api_workflow_resume(http_request_t* req, http_response_t* resp);
int api_workflow_abandon(http_request_t* req, http_response_t* resp);

/* Executor progress reporting */
int api_workflow_progress(http_request_t* req, http_response_t* resp);

/* Registry API handlers */
int api_registry_list_ci(http_request_t* req, http_response_t* resp);

/* Global daemon context for API handlers */
extern argo_daemon_t* g_api_daemon;

/* Initialize API routes */
int argo_daemon_register_api_routes(argo_daemon_t* daemon);

#endif /* ARGO_DAEMON_API_H */

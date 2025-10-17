/* © 2025 Casey Koons All rights reserved */
/* Daemon API route registration */

/* System includes */
#include <stdio.h>
#include <stdlib.h>

/* Project includes */
#include "argo_daemon_api.h"
#include "argo_daemon.h"
#include "argo_http_server.h"
#include "argo_daemon_ci_api.h"
#include "argo_error.h"
#include "argo_log.h"

/* Global daemon context for API handlers */
argo_daemon_t* g_api_daemon = NULL;

/* GET /api/registry/ci - List CIs */
int api_registry_list_ci(http_request_t* req, http_response_t* resp) {
    (void)req;  /* Unused */

    if (!resp || !g_api_daemon || !g_api_daemon->registry) {
        http_response_set_error(resp, HTTP_STATUS_SERVER_ERROR, DAEMON_ERR_INTERNAL_SERVER);
        return E_SYSTEM_MEMORY;
    }

    /* For now, return empty list */
    http_response_set_json(resp, HTTP_STATUS_OK, "{\"cis\":[]}");
    return ARGO_SUCCESS;
}

/* Register all API routes with daemon */
int argo_daemon_register_api_routes(argo_daemon_t* daemon) {
    if (!daemon) return E_INVALID_PARAMS;

    g_api_daemon = daemon;

    /* Workflow routes (bash-based) */
    http_server_add_route(daemon->http_server, HTTP_METHOD_POST,
                         "/api/workflow/start", api_workflow_start);
    http_server_add_route(daemon->http_server, HTTP_METHOD_GET,
                         "/api/workflow/list", api_workflow_list);
    http_server_add_route(daemon->http_server, HTTP_METHOD_GET,
                         "/api/workflow/status", api_workflow_status);
    http_server_add_route(daemon->http_server, HTTP_METHOD_DELETE,
                         "/api/workflow/abandon", api_workflow_abandon);
    http_server_add_route(daemon->http_server, HTTP_METHOD_POST,
                         "/api/workflow/pause", api_workflow_pause);
    http_server_add_route(daemon->http_server, HTTP_METHOD_POST,
                         "/api/workflow/resume", api_workflow_resume);
    http_server_add_route(daemon->http_server, HTTP_METHOD_POST,
                         "/api/workflow/input", api_workflow_input);

    /* Registry routes */
    http_server_add_route(daemon->http_server, HTTP_METHOD_GET,
                         "/api/registry/ci", api_registry_list_ci);

    /* CI query routes */
    http_server_add_route(daemon->http_server, HTTP_METHOD_POST,
                         "/api/ci/query", api_ci_query);

    LOG_INFO("API routes registered (workflow + CI API ready)");
    return ARGO_SUCCESS;
}

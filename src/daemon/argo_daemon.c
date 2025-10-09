/* © 2025 Casey Koons All rights reserved */
/* Argo Daemon - Central orchestration service */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Project includes */
#include "argo_daemon.h"
#include "argo_daemon_api.h"
#include "argo_error.h"
#include "argo_http_server.h"
#include "argo_registry.h"
#include "argo_lifecycle.h"
#include "argo_limits.h"

/* Create daemon */
argo_daemon_t* argo_daemon_create(uint16_t port) {
    argo_daemon_t* daemon = calloc(1, sizeof(argo_daemon_t));
    if (!daemon) {
        argo_report_error(E_SYSTEM_MEMORY, "argo_daemon_create", "allocation failed");
        return NULL;
    }

    daemon->port = port;
    daemon->should_shutdown = false;

    /* Initialize workflow registry mutex */
    if (pthread_mutex_init(&daemon->workflow_registry_lock, NULL) != 0) {
        argo_report_error(E_SYSTEM_THREAD, "argo_daemon_create", "mutex init failed");
        free(daemon);
        return NULL;
    }

    /* Create and load workflow registry - SINGLE INSTANCE */
    daemon->workflow_registry = workflow_registry_create(".argo/workflows/registry/active_workflow_registry.json");
    if (!daemon->workflow_registry) {
        argo_report_error(E_SYSTEM_MEMORY, "argo_daemon_create", "workflow registry creation failed");
        pthread_mutex_destroy(&daemon->workflow_registry_lock);
        free(daemon);
        return NULL;
    }

    /* Load registry from disk */
    int result = workflow_registry_load(daemon->workflow_registry);
    if (result != ARGO_SUCCESS && result != E_SYSTEM_FILE) {
        argo_report_error(result, "argo_daemon_create", "failed to load workflow registry");
        /* Continue anyway - empty registry is OK */
    }

    /* Cleanup dead workflows on startup */
    workflow_registry_cleanup_dead_workflows(daemon->workflow_registry);

    /* Create HTTP server */
    daemon->http_server = http_server_create(port);
    if (!daemon->http_server) {
        argo_report_error(E_SYSTEM_MEMORY, "argo_daemon_create", "HTTP server creation failed");
        workflow_registry_destroy(daemon->workflow_registry);
        pthread_mutex_destroy(&daemon->workflow_registry_lock);
        free(daemon);
        return NULL;
    }

    /* Create registry */
    daemon->registry = registry_create();
    if (!daemon->registry) {
        argo_report_error(E_SYSTEM_MEMORY, "argo_daemon_create", "registry creation failed");
        http_server_destroy(daemon->http_server);
        free(daemon);
        return NULL;
    }

    /* Create lifecycle manager */
    daemon->lifecycle = lifecycle_manager_create(daemon->registry);
    if (!daemon->lifecycle) {
        argo_report_error(E_SYSTEM_MEMORY, "argo_daemon_create", "lifecycle manager creation failed");
        registry_destroy(daemon->registry);
        http_server_destroy(daemon->http_server);
        free(daemon);
        return NULL;
    }

    return daemon;
}

/* Destroy daemon */
void argo_daemon_destroy(argo_daemon_t* daemon) {
    if (!daemon) return;

    if (daemon->lifecycle) {
        lifecycle_manager_destroy(daemon->lifecycle);
    }

    if (daemon->registry) {
        registry_destroy(daemon->registry);
    }

    if (daemon->http_server) {
        http_server_destroy(daemon->http_server);
    }

    /* Save and destroy workflow registry */
    if (daemon->workflow_registry) {
        workflow_registry_destroy(daemon->workflow_registry);
        pthread_mutex_destroy(&daemon->workflow_registry_lock);
    }

    free(daemon);
}

/* Health check handler */
int daemon_handle_health(http_request_t* req, http_response_t* resp) {
    (void)req;  /* Unused */

    const char* health_json =
        "{\"status\":\"ok\",\"service\":\"argo-daemon\",\"version\":\"0.1.0\"}";

    http_response_set_json(resp, HTTP_STATUS_OK, health_json);
    return ARGO_SUCCESS;
}

/* Version handler */
int daemon_handle_version(http_request_t* req, http_response_t* resp) {
    (void)req;  /* Unused */

    const char* version_json =
        "{\"version\":\"0.1.0\",\"api_version\":\"1\"}";

    http_response_set_json(resp, HTTP_STATUS_OK, version_json);
    return ARGO_SUCCESS;
}

/* Shutdown handler - triggers graceful shutdown */
int daemon_handle_shutdown(http_request_t* req, http_response_t* resp) {
    (void)req;  /* Unused */

    /* Get daemon from global context */
    extern argo_daemon_t* g_api_daemon;

    if (g_api_daemon) {
        g_api_daemon->should_shutdown = true;
    }

    const char* shutdown_json = "{\"status\":\"shutting down\"}";
    http_response_set_json(resp, HTTP_STATUS_OK, shutdown_json);

    return ARGO_SUCCESS;
}

/* Start daemon */
int argo_daemon_start(argo_daemon_t* daemon) {
    if (!daemon) return E_INVALID_PARAMS;

    /* Register basic routes */
    http_server_add_route(daemon->http_server, HTTP_METHOD_GET,
                         "/api/health", daemon_handle_health);

    http_server_add_route(daemon->http_server, HTTP_METHOD_GET,
                         "/api/version", daemon_handle_version);

    http_server_add_route(daemon->http_server, HTTP_METHOD_POST,
                         "/api/shutdown", daemon_handle_shutdown);

    /* Register API routes */
    argo_daemon_register_api_routes(daemon);

    printf("Argo Daemon starting on port %d\n", daemon->port);

    /* Start HTTP server (blocking) */
    return http_server_start(daemon->http_server);
}

/* Stop daemon */
void argo_daemon_stop(argo_daemon_t* daemon) {
    if (!daemon) return;

    printf("Stopping Argo Daemon\n");
    http_server_stop(daemon->http_server);
}

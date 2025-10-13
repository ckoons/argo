/* © 2025 Casey Koons All rights reserved */
/* Argo Daemon - Central orchestration service */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Project includes */
#include "argo_daemon.h"
#include "argo_daemon_api.h"
#include "argo_daemon_tasks.h"
#include "argo_daemon_workflow.h"
#include "argo_error.h"
#include "argo_http_server.h"
#include "argo_registry.h"
#include "argo_lifecycle.h"
#include "argo_workflow_registry.h"
#include "argo_shared_services.h"
#include "argo_limits.h"
#include "argo_log.h"

/* System includes for signal handling */
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

/* Global daemon pointer for signal handler */
static argo_daemon_t* g_daemon_for_sigchld = NULL;

/* SIGCHLD handler - POSIX async-signal-safe - ONLY reap zombies */
static void sigchld_handler(int sig) {
    (void)sig;  /* Unused */

    /* ONLY call async-signal-safe functions */
    int status;
    /* Reap all terminated children without blocking */
    /* This prevents zombie processes - actual workflow handling done in background task */
    while (waitpid(-1, &status, WNOHANG) > 0) {
        /* Just reap - workflow completion handled by workflow_completion_task() */
    }
}

/* Create daemon */
argo_daemon_t* argo_daemon_create(uint16_t port) {
    argo_daemon_t* daemon = calloc(1, sizeof(argo_daemon_t));
    if (!daemon) {
        argo_report_error(E_SYSTEM_MEMORY, "argo_daemon_create", "allocation failed");
        return NULL;
    }

    daemon->port = port;
    daemon->should_shutdown = false;

    /* Create workflow registry (Phase 3) */
    daemon->workflow_registry = workflow_registry_create();
    if (!daemon->workflow_registry) {
        argo_report_error(E_SYSTEM_MEMORY, "argo_daemon_create", "workflow registry creation failed");
        free(daemon);
        return NULL;
    }

    /* Create HTTP server */
    daemon->http_server = http_server_create(port);
    if (!daemon->http_server) {
        argo_report_error(E_SYSTEM_MEMORY, "argo_daemon_create", "HTTP server creation failed");
        workflow_registry_destroy(daemon->workflow_registry);
        free(daemon);
        return NULL;
    }

    /* Create registry */
    daemon->registry = registry_create();
    if (!daemon->registry) {
        argo_report_error(E_SYSTEM_MEMORY, "argo_daemon_create", "registry creation failed");
        http_server_destroy(daemon->http_server);
        workflow_registry_destroy(daemon->workflow_registry);
        free(daemon);
        return NULL;
    }

    /* Create lifecycle manager */
    daemon->lifecycle = lifecycle_manager_create(daemon->registry);
    if (!daemon->lifecycle) {
        argo_report_error(E_SYSTEM_MEMORY, "argo_daemon_create", "lifecycle manager creation failed");
        registry_destroy(daemon->registry);
        http_server_destroy(daemon->http_server);
        workflow_registry_destroy(daemon->workflow_registry);
        free(daemon);
        return NULL;
    }

    /* Create shared services */
    daemon->shared_services = shared_services_create();
    if (!daemon->shared_services) {
        argo_report_error(E_SYSTEM_MEMORY, "argo_daemon_create", "shared services creation failed");
        lifecycle_manager_destroy(daemon->lifecycle);
        registry_destroy(daemon->registry);
        http_server_destroy(daemon->http_server);
        workflow_registry_destroy(daemon->workflow_registry);
        free(daemon);
        return NULL;
    }

    LOG_INFO("Daemon created with workflow registry and shared services");
    return daemon;
}

/* Destroy daemon */
void argo_daemon_destroy(argo_daemon_t* daemon) {
    if (!daemon) return;

    /* Stop shared services first */
    if (daemon->shared_services) {
        shared_services_stop(daemon->shared_services);
        shared_services_destroy(daemon->shared_services);
    }

    if (daemon->lifecycle) {
        lifecycle_manager_destroy(daemon->lifecycle);
    }

    if (daemon->registry) {
        registry_destroy(daemon->registry);
    }

    if (daemon->http_server) {
        http_server_destroy(daemon->http_server);
    }

    if (daemon->workflow_registry) {
        workflow_registry_destroy(daemon->workflow_registry);
    }

    free(daemon);
    LOG_INFO("Daemon destroyed");
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

    /* Get daemon from global context (defined in argo_daemon_api_routes.c) */
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

    /* Set global daemon for SIGCHLD handler */
    g_daemon_for_sigchld = daemon;

    /* Install SIGCHLD handler for workflow process reaping */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;  /* Restart interrupted syscalls, ignore SIGSTOP */
    if (sigaction(SIGCHLD, &sa, NULL) < 0) {
        argo_report_error(E_SYSTEM_PROCESS, "argo_daemon_start", "failed to install SIGCHLD handler");
        return E_SYSTEM_PROCESS;
    }

    /* Register basic routes */
    http_server_add_route(daemon->http_server, HTTP_METHOD_GET,
                         "/api/health", daemon_handle_health);

    http_server_add_route(daemon->http_server, HTTP_METHOD_GET,
                         "/api/version", daemon_handle_version);

    http_server_add_route(daemon->http_server, HTTP_METHOD_POST,
                         "/api/shutdown", daemon_handle_shutdown);

    /* Register API routes */
    argo_daemon_register_api_routes(daemon);

    /* Start shared services and register background tasks */
    if (daemon->shared_services) {
        /* Register workflow timeout monitoring task */
        shared_services_register_task(daemon->shared_services,
                                     workflow_timeout_task,
                                     daemon,
                                     WORKFLOW_TIMEOUT_CHECK_INTERVAL_SECONDS);

        /* Register workflow completion detection task */
        shared_services_register_task(daemon->shared_services,
                                     workflow_completion_task,
                                     daemon,
                                     WORKFLOW_COMPLETION_CHECK_INTERVAL_SECONDS);

        /* Register log rotation task */
        shared_services_register_task(daemon->shared_services,
                                     log_rotation_task,
                                     daemon,
                                     LOG_ROTATION_CHECK_INTERVAL_SECONDS);

        /* Start shared services thread */
        int svc_result = shared_services_start(daemon->shared_services);
        if (svc_result != ARGO_SUCCESS) {
            argo_report_error(svc_result, "argo_daemon_start", "failed to start shared services");
            return svc_result;
        }

        LOG_INFO("Shared services started (timeout, completion, log rotation)");
    }

    LOG_INFO("Argo Daemon starting on port %d", daemon->port);

    /* Start HTTP server (blocking) */
    return http_server_start(daemon->http_server);
}

/* Stop daemon */
void argo_daemon_stop(argo_daemon_t* daemon) {
    if (!daemon) return;

    LOG_INFO("Stopping Argo Daemon");
    http_server_stop(daemon->http_server);
}

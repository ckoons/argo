/* © 2025 Casey Koons All rights reserved */
#ifndef ARGO_DAEMON_H
#define ARGO_DAEMON_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "argo_http_server.h"
#include "argo_registry.h"
#include "argo_lifecycle.h"
#include "argo_workflow_registry.h"

/* Daemon structure */
typedef struct {
    http_server_t* http_server;
    ci_registry_t* registry;
    lifecycle_manager_t* lifecycle;
    workflow_registry_t* workflow_registry;  /* Single shared registry */
    pthread_mutex_t workflow_registry_lock;  /* Protect registry access */
    uint16_t port;
    bool should_shutdown;  /* Graceful shutdown flag */
} argo_daemon_t;

/* Daemon lifecycle */
argo_daemon_t* argo_daemon_create(uint16_t port);
void argo_daemon_destroy(argo_daemon_t* daemon);

/* Daemon operations */
int argo_daemon_start(argo_daemon_t* daemon);
void argo_daemon_stop(argo_daemon_t* daemon);

/* API route handlers */
int daemon_handle_health(http_request_t* req, http_response_t* resp);
int daemon_handle_version(http_request_t* req, http_response_t* resp);
int daemon_handle_shutdown(http_request_t* req, http_response_t* resp);

#endif /* ARGO_DAEMON_H */

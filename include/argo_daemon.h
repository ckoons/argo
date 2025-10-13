/* © 2025 Casey Koons All rights reserved */
#ifndef ARGO_DAEMON_H
#define ARGO_DAEMON_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "argo_http_server.h"
#include "argo_registry.h"
#include "argo_lifecycle.h"
#include "argo_daemon_exit_queue.h"

/* Forward declarations */
typedef struct workflow_registry workflow_registry_t;
typedef struct shared_services shared_services_t;

/* Daemon structure */
typedef struct argo_daemon_struct {
    http_server_t* http_server;
    ci_registry_t* registry;
    lifecycle_manager_t* lifecycle;
    workflow_registry_t* workflow_registry;  /* Bash workflow tracking (Phase 3) */
    shared_services_t* shared_services;      /* Background tasks (timeout, log rotation) */
    exit_code_queue_t* exit_queue;           /* Signal-safe exit code queue (SIGCHLD → completion task) */
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

/* Workflow execution (bash-based) */
int daemon_execute_bash_workflow(argo_daemon_t* daemon,
                                 const char* script_path,
                                 char** args,
                                 int arg_count,
                                 char** env_keys,
                                 char** env_values,
                                 int env_count,
                                 const char* workflow_id);

#endif /* ARGO_DAEMON_H */

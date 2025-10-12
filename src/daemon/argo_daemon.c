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
#include "argo_workflow_registry.h"
#include "argo_limits.h"
#include "argo_log.h"

/* System includes for fork/exec */
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>

/* Global daemon pointer for signal handler */
static argo_daemon_t* g_daemon_for_sigchld = NULL;

/* SIGCHLD handler - reap completed workflow processes */
static void sigchld_handler(int sig) {
    (void)sig;  /* Unused */

    if (!g_daemon_for_sigchld || !g_daemon_for_sigchld->workflow_registry) {
        return;
    }

    /* Reap all terminated children without blocking */
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        /* Find workflow by PID */
        workflow_entry_t* entries = NULL;
        int count = 0;
        int result = workflow_registry_list(g_daemon_for_sigchld->workflow_registry,
                                           &entries, &count);
        if (result != ARGO_SUCCESS || !entries) {
            continue;
        }

        /* Find matching workflow */
        for (int i = 0; i < count; i++) {
            if (entries[i].executor_pid == pid) {
                /* Update workflow state based on exit status */
                workflow_state_t new_state;
                int exit_code = 0;

                if (WIFEXITED(status)) {
                    exit_code = WEXITSTATUS(status);
                    new_state = (exit_code == 0) ? WORKFLOW_STATE_COMPLETED : WORKFLOW_STATE_FAILED;
                } else if (WIFSIGNALED(status)) {
                    new_state = WORKFLOW_STATE_ABANDONED;
                    exit_code = WTERMSIG(status);
                } else {
                    new_state = WORKFLOW_STATE_FAILED;
                }

                /* Update workflow registry */
                workflow_registry_update_state(g_daemon_for_sigchld->workflow_registry,
                                              entries[i].workflow_id, new_state);

                /* Update exit code and end time */
                const workflow_entry_t* entry = workflow_registry_find(
                    g_daemon_for_sigchld->workflow_registry, entries[i].workflow_id);
                if (entry) {
                    workflow_entry_t* mutable_entry = (workflow_entry_t*)entry;
                    mutable_entry->exit_code = exit_code;
                    mutable_entry->end_time = time(NULL);
                }

                break;
            }
        }

        free(entries);
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

    LOG_INFO("Daemon created with workflow registry");
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

/* Execute bash workflow script */
int daemon_execute_bash_workflow(argo_daemon_t* daemon,
                                 const char* script_path,
                                 char** args,
                                 int arg_count,
                                 const char* workflow_id) {
    if (!daemon || !script_path || !workflow_id) {
        return E_INPUT_NULL;
    }

    /* Create workflow entry */
    workflow_entry_t entry = {0};
    strncpy(entry.workflow_id, workflow_id, sizeof(entry.workflow_id) - 1);
    strncpy(entry.workflow_name, script_path, sizeof(entry.workflow_name) - 1);
    entry.state = WORKFLOW_STATE_PENDING;
    entry.start_time = time(NULL);
    entry.end_time = 0;
    entry.exit_code = 0;
    entry.current_step = 0;
    entry.total_steps = 1;  /* Bash scripts don't have steps */

    /* Add to registry before forking */
    int result = workflow_registry_add(daemon->workflow_registry, &entry);
    if (result != ARGO_SUCCESS) {
        argo_report_error(result, "daemon_execute_bash_workflow",
                         "Failed to add workflow to registry");
        return result;
    }

    /* Fork process */
    pid_t pid = fork();
    if (pid < 0) {
        /* Fork failed */
        argo_report_error(E_SYSTEM_FORK, "daemon_execute_bash_workflow", "fork failed");
        workflow_registry_update_state(daemon->workflow_registry, workflow_id,
                                      WORKFLOW_STATE_FAILED);
        return E_SYSTEM_FORK;
    }

    if (pid == 0) {
        /* Child process - execute bash script */

        /* Create log directory if needed */
        const char* home = getenv("HOME");
        if (!home) home = ".";

        char log_dir[ARGO_PATH_MAX];
        snprintf(log_dir, sizeof(log_dir), "%s/.argo/logs", home);
        mkdir(log_dir, 0755);

        /* Redirect stdout/stderr to log file */
        char log_path[ARGO_PATH_MAX];
        snprintf(log_path, sizeof(log_path), "%s/%s.log", log_dir, workflow_id);

        int log_fd = open(log_path, O_CREAT | O_WRONLY | O_APPEND, 0644);
        if (log_fd >= 0) {
            dup2(log_fd, STDOUT_FILENO);
            dup2(log_fd, STDERR_FILENO);
            close(log_fd);
        }

        /* Build argv for bash execution */
        char** exec_args = malloc(sizeof(char*) * (arg_count + 3));
        if (!exec_args) {
            fprintf(stderr, "Failed to allocate exec args\n");
            exit(E_SYSTEM_MEMORY);
        }

        exec_args[0] = "/bin/bash";
        exec_args[1] = (char*)script_path;
        for (int i = 0; i < arg_count; i++) {
            exec_args[i + 2] = args[i];
        }
        exec_args[arg_count + 2] = NULL;

        /* Execute bash script */
        execv("/bin/bash", exec_args);

        /* If execv returns, it failed */
        fprintf(stderr, "Failed to execute script: %s\n", script_path);
        exit(E_SYSTEM_PROCESS);
    }

    /* Parent process - update registry with PID */
    workflow_registry_update_state(daemon->workflow_registry, workflow_id,
                                  WORKFLOW_STATE_RUNNING);

    /* Store PID in registry entry */
    /* Note: workflow_entry_t has executor_pid field */
    const workflow_entry_t* wf_entry = workflow_registry_find(daemon->workflow_registry, workflow_id);
    if (wf_entry) {
        /* Need to cast away const to update - this is a limitation of current API */
        workflow_entry_t* mutable_entry = (workflow_entry_t*)wf_entry;
        mutable_entry->executor_pid = pid;
    }

    LOG_INFO("Started bash workflow: %s (PID: %d)", workflow_id, pid);
    return ARGO_SUCCESS;
}

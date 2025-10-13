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
#include "argo_shared_services.h"
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
#include <dirent.h>

/* Global daemon pointer for signal handler */
static argo_daemon_t* g_daemon_for_sigchld = NULL;

/* Workflow timeout monitoring task */
static void workflow_timeout_task(void* context) {
    argo_daemon_t* daemon = (argo_daemon_t*)context;
    if (!daemon || !daemon->workflow_registry) {
        return;
    }

    time_t now = time(NULL);
    workflow_entry_t* entries = NULL;
    int count = 0;

    int result = workflow_registry_list(daemon->workflow_registry, &entries, &count);
    if (result != ARGO_SUCCESS || !entries) {
        return;
    }

    for (int i = 0; i < count; i++) {
        workflow_entry_t* entry = &entries[i];

        /* Only check running workflows with timeout set */
        if (entry->state != WORKFLOW_STATE_RUNNING || entry->timeout_seconds == 0) {
            continue;
        }

        /* Check if workflow has exceeded timeout */
        time_t elapsed = now - entry->start_time;
        if (elapsed > entry->timeout_seconds) {
            LOG_WARN("Workflow %s exceeded timeout (%d seconds), terminating",
                    entry->workflow_id, entry->timeout_seconds);

            /* Kill the workflow process */
            if (entry->executor_pid > 0) {
                kill(entry->executor_pid, SIGTERM);
            }

            /* Update state to abandoned */
            workflow_registry_update_state(daemon->workflow_registry,
                                          entry->workflow_id,
                                          WORKFLOW_STATE_ABANDONED);
        }
    }

    free(entries);
}

/* Log rotation task */
static void log_rotation_task(void* context) {
    argo_daemon_t* daemon = (argo_daemon_t*)context;
    (void)daemon;  /* Currently unused, may be needed for stats */

    const char* home = getenv("HOME");
    if (!home) home = ".";

    char log_dir[ARGO_PATH_MAX];
    snprintf(log_dir, sizeof(log_dir), "%s/.argo/logs", home);

    /* Open log directory */
    DIR* dir = opendir(log_dir);
    if (!dir) {
        return;  /* No logs directory, nothing to rotate */
    }

    time_t now = time(NULL);
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        /* Skip non-log files */
        if (strstr(entry->d_name, ".log") == NULL) {
            continue;
        }

        char log_path[ARGO_PATH_MAX];
        snprintf(log_path, sizeof(log_path), "%s/%s", log_dir, entry->d_name);

        struct stat st;
        if (stat(log_path, &st) != 0) {
            continue;
        }

        /* Check if log needs rotation (by age or size) */
        bool needs_rotation = false;
        time_t age = now - st.st_mtime;

        if (age > LOG_MAX_AGE_SECONDS) {
            LOG_DEBUG("Log %s exceeds max age (%ld days), rotating",
                     entry->d_name, age / SECONDS_PER_DAY);
            needs_rotation = true;
        } else if (st.st_size > LOG_MAX_SIZE_BYTES) {
            LOG_DEBUG("Log %s exceeds max size (%ld MB), rotating",
                     entry->d_name, st.st_size / BYTES_PER_MEGABYTE);
            needs_rotation = true;
        }

        if (!needs_rotation) {
            continue;
        }

        /* Rotate log files: file.log -> file.log.1 -> file.log.2 -> ... */
        /* First, remove oldest if it exists */
        char oldest_path[ARGO_PATH_MAX];
        snprintf(oldest_path, sizeof(oldest_path), "%s.%d",
                log_path, LOG_ROTATION_KEEP_COUNT);
        unlink(oldest_path);  /* Ignore errors */

        /* Rotate existing backups */
        for (int i = LOG_ROTATION_KEEP_COUNT - 1; i >= 1; i--) {
            char old_path[ARGO_PATH_MAX];
            char new_path[ARGO_PATH_MAX];

            if (i == 1) {
                snprintf(old_path, sizeof(old_path), "%s", log_path);
            } else {
                snprintf(old_path, sizeof(old_path), "%s.%d", log_path, i - 1);
            }
            snprintf(new_path, sizeof(new_path), "%s.%d", log_path, i);

            rename(old_path, new_path);  /* Ignore errors */
        }

        /* Create new empty log file */
        FILE* new_log = fopen(log_path, "w");
        if (new_log) {
            fclose(new_log);
        }
    }

    closedir(dir);
}

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
                bool should_retry = false;

                if (WIFEXITED(status)) {
                    exit_code = WEXITSTATUS(status);
                    new_state = (exit_code == 0) ? WORKFLOW_STATE_COMPLETED : WORKFLOW_STATE_FAILED;

                    /* Check if we should retry on failure */
                    if (exit_code != 0 && entries[i].retry_count < entries[i].max_retries) {
                        should_retry = true;
                    }
                } else if (WIFSIGNALED(status)) {
                    new_state = WORKFLOW_STATE_ABANDONED;
                    exit_code = WTERMSIG(status);
                } else {
                    new_state = WORKFLOW_STATE_FAILED;
                }

                /* Get mutable entry for updates */
                const workflow_entry_t* entry = workflow_registry_find(
                    g_daemon_for_sigchld->workflow_registry, entries[i].workflow_id);
                if (!entry) {
                    break;
                }
                workflow_entry_t* mutable_entry = (workflow_entry_t*)entry;

                if (should_retry) {
                    /* Retry the workflow */
                    mutable_entry->retry_count++;
                    mutable_entry->last_retry_time = time(NULL);

                    /* Calculate exponential backoff delay */
                    int delay = RETRY_DELAY_BASE_SECONDS * (1 << (mutable_entry->retry_count - 1));

                    LOG_INFO("Workflow %s failed (exit %d), retry %d/%d in %d seconds",
                            entries[i].workflow_id, exit_code,
                            mutable_entry->retry_count, mutable_entry->max_retries, delay);

                    /* Sleep before retry */
                    sleep(delay);

                    /* Re-execute workflow by forking again */
                    pid_t retry_pid = fork();
                    if (retry_pid == 0) {
                        /* Child process - re-execute the script */
                        const char* home = getenv("HOME");
                        if (!home) home = ".";

                        char log_dir[ARGO_PATH_MAX];
                        snprintf(log_dir, sizeof(log_dir), "%s/.argo/logs", home);
                        mkdir(log_dir, ARGO_DIR_PERMISSIONS);

                        /* Redirect to same log file (append mode) */
                        char log_path[ARGO_PATH_MAX];
                        snprintf(log_path, sizeof(log_path), "%s/%s.log", log_dir,
                                entries[i].workflow_id);

                        int log_fd = open(log_path, O_CREAT | O_WRONLY | O_APPEND, ARGO_FILE_PERMISSIONS);
                        if (log_fd >= 0) {
                            /* Log retry attempt */
                            dprintf(log_fd, "\n=== RETRY ATTEMPT %d/%d ===\n\n",
                                   mutable_entry->retry_count, mutable_entry->max_retries);
                            dup2(log_fd, STDOUT_FILENO);
                            dup2(log_fd, STDERR_FILENO);
                            close(log_fd);
                        }

                        /* Execute script (args/env preserved in original fork) */
                        char* exec_args[] = {"/bin/bash", (char*)entries[i].workflow_name, NULL};
                        execv("/bin/bash", exec_args);

                        /* If execv returns, it failed */
                        fprintf(stderr, "Failed to execute retry: %s\n", entries[i].workflow_name);
                        exit(E_SYSTEM_PROCESS);
                    } else if (retry_pid > 0) {
                        /* Parent - update PID and state */
                        mutable_entry->executor_pid = retry_pid;
                        workflow_registry_update_state(g_daemon_for_sigchld->workflow_registry,
                                                      entries[i].workflow_id,
                                                      WORKFLOW_STATE_RUNNING);
                    }
                } else {
                    /* No retry - finalize workflow */
                    workflow_registry_update_state(g_daemon_for_sigchld->workflow_registry,
                                                  entries[i].workflow_id, new_state);
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

        LOG_INFO("Shared services started (timeout monitoring, log rotation)");
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

/* Execute bash workflow script */
int daemon_execute_bash_workflow(argo_daemon_t* daemon,
                                 const char* script_path,
                                 char** args,
                                 int arg_count,
                                 char** env_keys,
                                 char** env_values,
                                 int env_count,
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
    entry.timeout_seconds = DEFAULT_WORKFLOW_TIMEOUT_SECONDS;
    entry.retry_count = 0;
    entry.max_retries = DEFAULT_MAX_RETRY_ATTEMPTS;
    entry.last_retry_time = 0;

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

        /* Apply environment variables if provided */
        for (int i = 0; i < env_count; i++) {
            if (env_keys[i] && env_values[i]) {
                setenv(env_keys[i], env_values[i], 1);  /* Overwrite if exists */
            }
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

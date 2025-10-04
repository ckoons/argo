/* © 2025 Casey Koons All rights reserved */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "argo_orchestrator_api.h"
#include "argo_error.h"
#include "argo_log.h"

/* Log file path pattern */
#define LOG_PATH_PATTERN "%s/.argo/logs/%s.log"
#define LOG_PATH_MAX 512

/* Workflow executor binary path (relative to argo installation) */
#define EXECUTOR_BINARY "./argo_workflow_executor"

/* Get log file path for workflow */
static int get_log_path(const char* workflow_id, char* path, size_t path_size) {
    const char* home = getenv("HOME");
    if (!home) {
        return E_SYSTEM_FILE;
    }

    snprintf(path, path_size, LOG_PATH_PATTERN, home, workflow_id);
    return ARGO_SUCCESS;
}

/* Create log directory if needed */
static int ensure_log_directory(void) {
    const char* home = getenv("HOME");
    if (!home) {
        return E_SYSTEM_FILE;
    }

    char log_dir[LOG_PATH_MAX];
    snprintf(log_dir, sizeof(log_dir), "%s/.argo/logs", home);

    /* Create directory (ignore error if exists) */
    if (mkdir(log_dir, 0755) != 0 && errno != EEXIST) {
        return E_SYSTEM_FILE;
    }

    return ARGO_SUCCESS;
}

/* Start workflow execution in background */
int workflow_exec_start(const char* workflow_id,
                       const char* template_path,
                       const char* branch,
                       workflow_registry_t* registry) {
    if (!workflow_id || !template_path || !registry) {
        return E_INVALID_PARAMS;
    }

    /* Ensure log directory exists */
    int result = ensure_log_directory();
    if (result != ARGO_SUCCESS) {
        LOG_ERROR("Failed to create log directory");
        return result;
    }

    /* Get log file path */
    char log_path[LOG_PATH_MAX];
    result = get_log_path(workflow_id, log_path, sizeof(log_path));
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Fork process */
    pid_t pid = fork();
    if (pid < 0) {
        LOG_ERROR("Failed to fork process: %s", strerror(errno));
        return E_SYSTEM_THREAD;
    }

    if (pid == 0) {
        /* Child process */

        /* Redirect stdout and stderr to log file */
        int log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (log_fd < 0) {
            fprintf(stderr, "Failed to open log file: %s\n", log_path);
            exit(1);
        }

        dup2(log_fd, STDOUT_FILENO);
        dup2(log_fd, STDERR_FILENO);
        close(log_fd);

        /* Close stdin */
        close(STDIN_FILENO);

        /* TODO: Execute workflow executor */
        /* For now, just log that we would execute */
        fprintf(stdout, "Workflow %s started (template=%s, branch=%s)\n",
                workflow_id, template_path, branch);
        fprintf(stdout, "Process would execute workflow here\n");
        fprintf(stdout, "Workflow executor integration pending\n");

        /* Sleep to simulate workflow execution */
        sleep(5);

        fprintf(stdout, "Workflow %s completed\n", workflow_id);
        exit(0);
    }

    /* Parent process */
    LOG_INFO("Started workflow %s with PID %d", workflow_id, pid);

    /* Update registry with PID */
    workflow_instance_t* wf = workflow_registry_get_workflow(registry, workflow_id);
    if (wf) {
        wf->pid = pid;
        workflow_registry_schedule_save(registry);
    }

    return ARGO_SUCCESS;
}

/* Check if process is alive */
bool workflow_exec_is_process_alive(pid_t pid) {
    if (pid <= 0) {
        return false;
    }

    /* Send signal 0 to check if process exists */
    if (kill(pid, 0) == 0) {
        return true;
    }

    /* Process doesn't exist or we don't have permission */
    return false;
}

/* Pause workflow at next checkpoint */
int workflow_exec_pause(const char* workflow_id,
                       workflow_registry_t* registry) {
    if (!workflow_id || !registry) {
        return E_INVALID_PARAMS;
    }

    workflow_instance_t* wf = workflow_registry_get_workflow(registry, workflow_id);
    if (!wf) {
        return E_INPUT_INVALID;
    }

    if (wf->pid <= 0) {
        LOG_WARN("Workflow %s has no associated process", workflow_id);
        return ARGO_SUCCESS;
    }

    /* Check if process is alive */
    if (!workflow_exec_is_process_alive(wf->pid)) {
        LOG_WARN("Workflow %s process (PID %d) is no longer running", workflow_id, wf->pid);
        wf->pid = 0;
        wf->status = WORKFLOW_STATUS_COMPLETED;
        workflow_registry_schedule_save(registry);
        return ARGO_SUCCESS;
    }

    /* Send SIGUSR1 to pause */
    if (kill(wf->pid, SIGUSR1) != 0) {
        LOG_ERROR("Failed to send pause signal to PID %d: %s", wf->pid, strerror(errno));
        return E_SYSTEM_THREAD;
    }

    LOG_INFO("Sent pause signal to workflow %s (PID %d)", workflow_id, wf->pid);
    return ARGO_SUCCESS;
}

/* Resume workflow from checkpoint */
int workflow_exec_resume(const char* workflow_id,
                        workflow_registry_t* registry) {
    if (!workflow_id || !registry) {
        return E_INVALID_PARAMS;
    }

    workflow_instance_t* wf = workflow_registry_get_workflow(registry, workflow_id);
    if (!wf) {
        return E_INPUT_INVALID;
    }

    if (wf->pid <= 0) {
        LOG_WARN("Workflow %s has no associated process", workflow_id);
        return ARGO_SUCCESS;
    }

    /* Check if process is alive */
    if (!workflow_exec_is_process_alive(wf->pid)) {
        LOG_WARN("Workflow %s process (PID %d) is no longer running", workflow_id, wf->pid);
        wf->pid = 0;
        wf->status = WORKFLOW_STATUS_COMPLETED;
        workflow_registry_schedule_save(registry);
        return ARGO_SUCCESS;
    }

    /* Send SIGUSR2 to resume */
    if (kill(wf->pid, SIGUSR2) != 0) {
        LOG_ERROR("Failed to send resume signal to PID %d: %s", wf->pid, strerror(errno));
        return E_SYSTEM_THREAD;
    }

    LOG_INFO("Sent resume signal to workflow %s (PID %d)", workflow_id, wf->pid);
    return ARGO_SUCCESS;
}

/* Abandon workflow and cleanup */
int workflow_exec_abandon(const char* workflow_id,
                         workflow_registry_t* registry) {
    if (!workflow_id || !registry) {
        return E_INVALID_PARAMS;
    }

    workflow_instance_t* wf = workflow_registry_get_workflow(registry, workflow_id);
    if (!wf) {
        return E_INPUT_INVALID;
    }

    /* Kill process if running */
    if (wf->pid > 0) {
        if (workflow_exec_is_process_alive(wf->pid)) {
            LOG_INFO("Terminating workflow %s process (PID %d)", workflow_id, wf->pid);

            /* Try graceful termination first (SIGTERM) */
            if (kill(wf->pid, SIGTERM) == 0) {
                /* Wait briefly for graceful shutdown */
                sleep(1);

                /* Check if still alive */
                if (workflow_exec_is_process_alive(wf->pid)) {
                    /* Force kill */
                    LOG_WARN("Forcefully killing workflow %s (PID %d)", workflow_id, wf->pid);
                    kill(wf->pid, SIGKILL);
                }
            }

            /* Reap zombie process */
            waitpid(wf->pid, NULL, WNOHANG);
        }
    }

    return ARGO_SUCCESS;
}

/* Get workflow execution state */
int workflow_exec_get_state(const char* workflow_id,
                           workflow_registry_t* registry,
                           workflow_execution_state_t* state) {
    if (!workflow_id || !registry || !state) {
        return E_INVALID_PARAMS;
    }

    workflow_instance_t* wf = workflow_registry_get_workflow(registry, workflow_id);
    if (!wf) {
        return E_INPUT_INVALID;
    }

    /* Initialize state */
    memset(state, 0, sizeof(workflow_execution_state_t));
    state->pid = wf->pid;
    state->workflow_start_time = wf->created_at;
    state->is_paused = (wf->status == WORKFLOW_STATUS_SUSPENDED);

    /* Check if process is alive */
    if (wf->pid > 0) {
        if (!workflow_exec_is_process_alive(wf->pid)) {
            /* Process died */
            wf->pid = 0;
            wf->status = WORKFLOW_STATUS_COMPLETED;
            workflow_registry_schedule_save(registry);
            state->pid = 0;
        }
    }

    /* TODO: Query actual execution state from process */
    /* For now, return basic info */
    strcpy(state->current_step, "Unknown (executor integration pending)");
    state->step_number = 0;
    state->total_steps = 0;
    strcpy(state->last_checkpoint, "Not available");

    return ARGO_SUCCESS;
}

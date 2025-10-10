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
#include "argo_workflow_executor.h"
#include "argo_error.h"
#include "argo_log.h"
#include "argo_output.h"
#include "argo_limits.h"

/* Log file path pattern */
#define LOG_PATH_PATTERN "%s/.argo/logs/%s.log"

/* Find executor in common locations */
static const char* find_executor_binary(void) {
    static char executor_path[ARGO_PATH_MAX];
    static char home_local_path[ARGO_PATH_MAX];

    /* Build HOME/.local/bin path */
    const char* home = getenv("HOME");
    if (home) {
        snprintf(home_local_path, sizeof(home_local_path),
                "%s/.local/bin/argo_workflow_executor", home);
    }

    const char* locations[] = {
        home ? home_local_path : NULL,          /* ~/.local/bin */
        "./bin/argo_workflow_executor",          /* Local build */
        "/usr/local/bin/argo_workflow_executor", /* System install */
        NULL
    };

    for (int i = 0; locations[i]; i++) {
        /* Check if file exists and is executable */
        if (access(locations[i], X_OK) == 0) {
            strncpy(executor_path, locations[i], sizeof(executor_path) - 1);
            executor_path[sizeof(executor_path) - 1] = '\0';
            return executor_path;
        }
    }

    /* Fallback - just use the name and let execvp search PATH */
    return "argo_workflow_executor";
}

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

    char log_dir[ARGO_PATH_MAX];
    snprintf(log_dir, sizeof(log_dir), "%s/.argo/logs", home);

    /* Create directory (ignore error if exists) */
    if (mkdir(log_dir, ARGO_DIR_PERMISSIONS) != 0 && errno != EEXIST) {
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
    char log_path[ARGO_PATH_MAX];
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
        int log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, ARGO_FILE_PERMISSIONS);
        if (log_fd < 0) {
            FORK_ERROR("Failed to open log file: %s\n", log_path);
            exit(1);
        }

        dup2(log_fd, STDOUT_FILENO);
        dup2(log_fd, STDERR_FILENO);
        close(log_fd);

        /* Close stdin */
        close(STDIN_FILENO);

        /* Execute workflow executor binary */
        const char* executor_bin = find_executor_binary();

        char* args[5];
        args[0] = (char*)executor_bin;
        args[1] = (char*)workflow_id;
        args[2] = (char*)template_path;
        args[3] = (char*)branch;
        args[4] = NULL;

        /* Use execvp to search PATH if needed (for the fallback case) */
        if (strchr(executor_bin, '/')) {
            /* Has a path - use execv */
            execv(executor_bin, args);
        } else {
            /* No path - use execvp to search PATH */
            execvp(executor_bin, args);
        }

        /* If execv returns, it failed */
        FORK_ERROR("Failed to execute workflow executor: %s (path: %s)\n", strerror(errno), executor_bin);
        exit(1);
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

    /* Try to read checkpoint file for execution state */
    char checkpoint_path[ARGO_PATH_MAX];
    const char* home = getenv("HOME");
    if (home) {
        snprintf(checkpoint_path, sizeof(checkpoint_path),
                "%s/.argo/workflows/checkpoints/%s.json", home, workflow_id);

        FILE* cp_file = fopen(checkpoint_path, "r");
        if (cp_file) {
            char buffer[ARGO_PATH_MAX];
            size_t bytes = fread(buffer, 1, sizeof(buffer) - 1, cp_file);
            fclose(cp_file);
            buffer[bytes] = '\0';

            /* Extract state from checkpoint */
            const char* step_str = strstr(buffer, JSON_CURRENT_STEP_FIELD);
            const char* total_str = strstr(buffer, JSON_TOTAL_STEPS_FIELD);

            if (step_str && total_str) {
                sscanf(step_str + JSON_CURRENT_STEP_OFFSET, "%d", &state->step_number);
                sscanf(total_str + JSON_TOTAL_STEPS_OFFSET, "%d", &state->total_steps);
                snprintf(state->current_step, sizeof(state->current_step),
                        "Step %d/%d", state->step_number + 1, state->total_steps);
                snprintf(state->last_checkpoint, sizeof(state->last_checkpoint),
                        "%s", checkpoint_path);
            } else {
                snprintf(state->current_step, sizeof(state->current_step), "Running");
                state->step_number = 0;
                state->total_steps = 0;
                snprintf(state->last_checkpoint, sizeof(state->last_checkpoint), "No checkpoint available");
            }
        } else {
            snprintf(state->current_step, sizeof(state->current_step), "Running (no checkpoint)");
            state->step_number = 0;
            state->total_steps = 0;
            snprintf(state->last_checkpoint, sizeof(state->last_checkpoint), "No checkpoint file");
        }
    }

    return ARGO_SUCCESS;
}

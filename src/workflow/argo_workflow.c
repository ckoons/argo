/* © 2025 Casey Koons All rights reserved */
/* Workflow execution engine - execute bash scripts */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

/* Project includes */
#include "argo_workflow.h"
#include "argo_error.h"
#include "argo_log.h"
#include "argo_limits.h"

/* Wait for child process with timeout enforcement */
process_wait_result_t process_wait_with_timeout(pid_t pid, int timeout_seconds, int timeout_exit_code) {
    process_wait_result_t result = {
        .exit_code = 0,
        .timed_out = false,
        .wait_failed = false
    };

    time_t start_time = time(NULL);
    int status = 0;

    while (1) {
        pid_t wait_result = waitpid(pid, &status, WNOHANG);

        if (wait_result == pid) {
            /* Child exited normally */
            if (WIFEXITED(status)) {
                result.exit_code = WEXITSTATUS(status);
            } else {
                /* Abnormal termination (signal, etc.) */
                result.exit_code = 1;
            }
            break;
        } else if (wait_result < 0) {
            /* Wait failed */
            if (errno == EINTR) {
                continue;  /* Interrupted by signal, retry */
            }
            /* Real error */
            argo_report_error(E_SYSTEM_PROCESS, "process_wait_with_timeout",
                             "waitpid failed: %s", strerror(errno));
            result.exit_code = -1;
            result.wait_failed = true;
            break;
        }

        /* Check timeout */
        if (timeout_seconds > 0) {
            time_t elapsed = time(NULL) - start_time;
            if (elapsed >= timeout_seconds) {
                /* Timeout reached - terminate process */
                LOG_WARN("Process timed out after %d seconds, killing PID %d",
                        timeout_seconds, (int)pid);

                /* Try graceful termination first */
                kill(pid, SIGTERM);
                sleep(1);

                /* Force kill if still running */
                kill(pid, SIGKILL);

                /* Wait for process to die */
                waitpid(pid, &status, 0);

                result.exit_code = timeout_exit_code;
                result.timed_out = true;
                break;
            }
        }

        /* Sleep briefly before next check */
        usleep(WORKFLOW_POLL_INTERVAL_USEC);
    }

    return result;
}

/* Execute workflow script */
int workflow_execute(workflow_t* workflow, const char* log_path) {
    if (!workflow) {
        return E_INPUT_NULL;
    }

    if (workflow->script_path[0] == '\0') {
        argo_report_error(E_INPUT_FORMAT, "workflow_execute",
                         "No script path specified");
        return E_INPUT_FORMAT;
    }

    int result = ARGO_SUCCESS;
    int log_fd = -1;
    pid_t script_pid = 0;

    /* Open log file if provided */
    if (log_path) {
        log_fd = open(log_path, O_CREAT | O_WRONLY | O_APPEND, ARGO_FILE_PERMISSIONS);
        if (log_fd < 0) {
            LOG_ERROR("Failed to open log file: %s", log_path);
        } else {
            /* Write workflow header to log */
            dprintf(log_fd, "=== Workflow: %s (ID: %s) ===\n",
                   workflow->workflow_name, workflow->workflow_id);
            dprintf(log_fd, "Description: %s\n", workflow->description);
            dprintf(log_fd, "Script: %s\n", workflow->script_path);
            dprintf(log_fd, "Started: %ld\n\n", (long)time(NULL));
        }
    }

    /* Update workflow state */
    workflow->state = WORKFLOW_STATE_RUNNING;
    workflow->start_time = time(NULL);

    LOG_INFO("Starting workflow '%s' (ID: %s, script: %s)",
            workflow->workflow_name, workflow->workflow_id, workflow->script_path);

    /* Fork process to execute script */
    script_pid = fork();
    if (script_pid < 0) {
        argo_report_error(E_SYSTEM_FORK, "workflow_execute", "fork failed");
        result = E_SYSTEM_FORK;
        goto cleanup;
    }

    if (script_pid == 0) {
        /* Child process */

        /* Redirect stdout/stderr to log file if provided */
        if (log_fd >= 0) {
            dup2(log_fd, STDOUT_FILENO);
            dup2(log_fd, STDERR_FILENO);
            close(log_fd);
        }

        /* Change working directory if specified */
        if (workflow->working_dir[0] != '\0') {
            if (chdir(workflow->working_dir) != 0) {
                /* GUIDELINE_APPROVED: Child process error before _exit */
                fprintf(stderr, "Failed to change directory to: %s\n",
                       workflow->working_dir);
                _exit(1);
            }
        }

        /* Execute script via bash */
        char* exec_args[] = {"/bin/bash", workflow->script_path, NULL};
        execv("/bin/bash", exec_args);

        /* If execv returns, it failed */
        /* GUIDELINE_APPROVED: Child process error before _exit */
        fprintf(stderr, "Failed to execute script: %s\n", workflow->script_path);
        _exit(E_SYSTEM_PROCESS);
    }

    /* Parent process - wait for child with timeout */
    process_wait_result_t wait_result = process_wait_with_timeout(
        script_pid,
        workflow->timeout_seconds,
        WORKFLOW_TIMEOUT_EXIT_CODE
    );

    /* Update workflow with wait results */
    workflow->exit_code = wait_result.exit_code;
    workflow->end_time = time(NULL);

    /* Handle wait failures */
    if (wait_result.wait_failed) {
        workflow->state = WORKFLOW_STATE_FAILED;
        result = E_SYSTEM_PROCESS;
        goto cleanup;
    }

    /* Update final state based on result */
    if (wait_result.timed_out) {
        workflow->state = WORKFLOW_STATE_FAILED;
        result = E_TIMEOUT;
    } else if (workflow->exit_code == 0) {
        workflow->state = WORKFLOW_STATE_COMPLETED;
        result = ARGO_SUCCESS;
    } else {
        workflow->state = WORKFLOW_STATE_FAILED;
        result = E_WORKFLOW_FAILED;
    }

    /* Write completion status to log */
    if (log_fd >= 0) {
        dprintf(log_fd, "\n=== Workflow %s ===\n",
               workflow->state == WORKFLOW_STATE_COMPLETED ? "COMPLETED" : "FAILED");
        dprintf(log_fd, "Exit code: %d\n", workflow->exit_code);
        dprintf(log_fd, "Duration: %ld seconds\n",
               (long)(workflow->end_time - workflow->start_time));
        dprintf(log_fd, "Ended: %ld\n", (long)workflow->end_time);
    }

    if (workflow->state == WORKFLOW_STATE_COMPLETED) {
        LOG_INFO("Workflow '%s' completed successfully (%ld seconds)",
                workflow->workflow_name,
                (long)(workflow->end_time - workflow->start_time));
    } else {
        LOG_ERROR("Workflow '%s' failed (exit code: %d, duration: %ld seconds)",
                 workflow->workflow_name, workflow->exit_code,
                 (long)(workflow->end_time - workflow->start_time));
    }

cleanup:
    if (log_fd >= 0) {
        close(log_fd);
    }

    return result;
}

/* Convert workflow state to string */
const char* workflow_state_to_string(workflow_state_t state) {
    switch (state) {
        case WORKFLOW_STATE_PENDING: return "pending";
        case WORKFLOW_STATE_RUNNING: return "running";
        case WORKFLOW_STATE_PAUSED: return "paused";
        case WORKFLOW_STATE_COMPLETED: return "completed";
        case WORKFLOW_STATE_FAILED: return "failed";
        case WORKFLOW_STATE_ABANDONED: return "abandoned";
        default: return "unknown";
    }
}

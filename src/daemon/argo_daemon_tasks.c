/* © 2025 Casey Koons All rights reserved */
/* Daemon background tasks - workflow monitoring, log rotation, completion detection */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

/* Project includes */
#include "argo_daemon_tasks.h"
#include "argo_daemon.h"
#include "argo_daemon_exit_queue.h"
#include "argo_workflow_registry.h"
#include "argo_limits.h"
#include "argo_log.h"
#include "argo_error.h"

/* Workflow timeout monitoring task */
void workflow_timeout_task(void* context) {
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

            /* Set abandon flag - completion task will remove it */
            const workflow_entry_t* found = workflow_registry_find(
                daemon->workflow_registry, entry->workflow_id);
            if (found) {
                workflow_entry_t* mutable = (workflow_entry_t*)found;
                mutable->abandon_requested = true;
            }
        }
    }

    free(entries);
}

/* Log rotation task */
void log_rotation_task(void* context) {
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

/* Helper: Setup log file for workflow retry */
static int setup_retry_log(const char* workflow_id, int retry_count, int max_retries) {
    const char* home = getenv("HOME");
    if (!home) home = ".";

    char log_dir[ARGO_PATH_MAX];
    snprintf(log_dir, sizeof(log_dir), "%s/.argo/logs", home);
    mkdir(log_dir, ARGO_DIR_PERMISSIONS);

    char log_path[ARGO_PATH_MAX];
    snprintf(log_path, sizeof(log_path), "%s/%s.log", log_dir, workflow_id);

    int log_fd = open(log_path, O_CREAT | O_WRONLY | O_APPEND, ARGO_FILE_PERMISSIONS);
    if (log_fd >= 0) {
        dprintf(log_fd, "\n=== RETRY ATTEMPT %d/%d ===\n\n", retry_count, max_retries);
        dup2(log_fd, STDOUT_FILENO);
        dup2(log_fd, STDERR_FILENO);
        close(log_fd);
    }

    return log_fd >= 0 ? 0 : -1;
}

/* Helper: Retry workflow execution */
static pid_t retry_workflow_execution(workflow_entry_t* entry, workflow_entry_t* mutable_entry) {
    /* Calculate exponential backoff delay */
    int delay = RETRY_DELAY_BASE_SECONDS * (1 << (mutable_entry->retry_count - 1));

    LOG_INFO("Workflow %s failed, retry %d/%d in %d seconds",
            entry->workflow_id,
            mutable_entry->retry_count, mutable_entry->max_retries, delay);

    sleep(delay);

    /* Re-execute workflow by forking */
    pid_t retry_pid = fork();
    if (retry_pid == 0) {
        /* Child process - setup logging and execute */
        setup_retry_log(entry->workflow_id, mutable_entry->retry_count,
                       mutable_entry->max_retries);

        /* Execute script */
        char* exec_args[] = {"/bin/bash", (char*)entry->workflow_name, NULL};
        execv("/bin/bash", exec_args);

        /* If execv returns, it failed */
        fprintf(stderr, "Failed to execute retry: %s\n", entry->workflow_name);
        _exit(E_SYSTEM_PROCESS);
    }

    return retry_pid;
}

/* Helper: Handle workflow process failure */
static void handle_workflow_failure(argo_daemon_t* daemon, workflow_entry_t* entry) {
    /* Get mutable entry for updates */
    const workflow_entry_t* found_entry = workflow_registry_find(
        daemon->workflow_registry, entry->workflow_id);
    if (!found_entry) {
        return;
    }
    workflow_entry_t* mutable_entry = (workflow_entry_t*)found_entry;

    /* Check if retry is allowed */
    bool should_retry = (mutable_entry->retry_count < mutable_entry->max_retries);

    if (should_retry) {
        /* Retry the workflow */
        mutable_entry->retry_count++;
        mutable_entry->last_retry_time = time(NULL);

        pid_t retry_pid = retry_workflow_execution(entry, mutable_entry);

        if (retry_pid > 0) {
            /* Parent - update PID and state */
            mutable_entry->executor_pid = retry_pid;
            workflow_registry_update_state(daemon->workflow_registry,
                                          entry->workflow_id,
                                          WORKFLOW_STATE_RUNNING);
        }
    } else {
        /* No retry - remove workflow from registry */
        LOG_INFO("Workflow %s failed after %d attempts", entry->workflow_id,
                mutable_entry->retry_count);
        workflow_registry_remove(daemon->workflow_registry, entry->workflow_id);
    }
}

/* Workflow completion detection and retry task */
void workflow_completion_task(void* context) {
    argo_daemon_t* daemon = (argo_daemon_t*)context;
    if (!daemon || !daemon->workflow_registry || !daemon->exit_queue) {
        return;
    }

    /* Check for dropped exit codes (queue overflow) */
    int dropped = exit_queue_get_dropped(daemon->exit_queue);
    if (dropped > 0) {
        LOG_WARN("Exit code queue dropped %d entries (queue full)", dropped);
    }

    /* Drain exit code queue from SIGCHLD handler */
    exit_code_entry_t exit_entry;
    while (exit_queue_pop(daemon->exit_queue, &exit_entry)) {
        /* Find workflow by PID */
        workflow_entry_t* entries = NULL;
        int count = 0;

        int result = workflow_registry_list(daemon->workflow_registry, &entries, &count);
        if (result != ARGO_SUCCESS || !entries) {
            continue;
        }

        /* Match PID to workflow */
        bool found = false;
        for (int i = 0; i < count; i++) {
            workflow_entry_t* entry = &entries[i];

            if (entry->executor_pid == exit_entry.pid && entry->state == WORKFLOW_STATE_RUNNING) {
                found = true;

                /* Get mutable entry for updates */
                const workflow_entry_t* found_entry = workflow_registry_find(
                    daemon->workflow_registry, entry->workflow_id);
                if (!found_entry) {
                    break;
                }
                workflow_entry_t* mutable_entry = (workflow_entry_t*)found_entry;
                mutable_entry->exit_code = exit_entry.exit_code;

                /* Check if abandon was requested */
                if (mutable_entry->abandon_requested) {
                    /* User requested abandon - remove from registry */
                    LOG_INFO("Workflow %s abandoned by user request (exit code %d)",
                            entry->workflow_id, exit_entry.exit_code);
                    workflow_registry_remove(daemon->workflow_registry, entry->workflow_id);
                } else if (exit_entry.exit_code == 0) {
                    /* Success - remove from registry */
                    LOG_INFO("Workflow %s completed successfully (exit code 0)", entry->workflow_id);
                    workflow_registry_remove(daemon->workflow_registry, entry->workflow_id);
                } else {
                    /* Failure - handle retry logic */
                    LOG_INFO("Workflow %s failed (exit code %d)", entry->workflow_id, exit_entry.exit_code);
                    handle_workflow_failure(daemon, entry);
                }

                break;
            }
        }

        if (!found) {
            LOG_DEBUG("Exit code for PID %d not matched to any workflow (already cleaned up?)", exit_entry.pid);
        }

        free(entries);
    }
}

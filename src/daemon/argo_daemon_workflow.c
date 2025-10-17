/* © 2025 Casey Koons All rights reserved */
/* Workflow execution - bash script execution with security validation */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>

/* Project includes */
#include "argo_daemon_workflow.h"
#include "argo_daemon.h"
#include "argo_workflow_registry.h"
#include "argo_limits.h"
#include "argo_log.h"
#include "argo_error.h"

/* Validate workflow script path - prevent directory traversal and command injection */
static bool validate_script_path(const char* path) {
    if (!path || strlen(path) == 0) {
        return false;
    }

    /* Check for dangerous patterns */
    if (strstr(path, "..") != NULL) {
        LOG_ERROR("Script path contains '..' (directory traversal): %s", path);
        return false;
    }

    if (path[0] == '|' || path[0] == '>' || path[0] == '<' || path[0] == '&') {
        LOG_ERROR("Script path starts with shell metacharacter: %s", path);
        return false;
    }

    /* Check for shell metacharacters that could enable command injection */
    const char* dangerous_chars = ";|&$`<>(){}[]!";
    for (const char* p = dangerous_chars; *p; p++) {
        if (strchr(path, *p) != NULL) {
            LOG_ERROR("Script path contains dangerous character '%c': %s", *p, path);
            return false;
        }
    }

    /* Verify file exists and is regular file */
    struct stat st;
    if (stat(path, &st) != 0) {
        LOG_ERROR("Script path does not exist: %s", path);
        return false;
    }

    if (!S_ISREG(st.st_mode)) {
        LOG_ERROR("Script path is not a regular file: %s", path);
        return false;
    }

    return true;
}

/* Sanitize environment variable name - prevent LD_PRELOAD and other dangerous vars */
static bool is_safe_env_var(const char* key) {
    if (!key || strlen(key) == 0) {
        return false;
    }

    /* Block dangerous environment variables */
    const char* blocked_vars[] = {
        "LD_PRELOAD", "LD_LIBRARY_PATH", "DYLD_INSERT_LIBRARIES",
        "DYLD_LIBRARY_PATH", "PATH", "IFS", "BASH_ENV", "ENV",
        "SHELLOPTS", "PS4", NULL
    };

    for (int i = 0; blocked_vars[i] != NULL; i++) {
        if (strcmp(key, blocked_vars[i]) == 0) {
            LOG_WARN("Blocked dangerous environment variable: %s", key);
            return false;
        }
    }

    /* Ensure alphanumeric + underscore only */
    for (const char* p = key; *p; p++) {
        if (!isalnum(*p) && *p != '_') {
            LOG_ERROR("Environment variable name contains invalid character: %s", key);
            return false;
        }
    }

    return true;
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

    /* Validate script path for security */
    if (!validate_script_path(script_path)) {
        argo_report_error(E_INVALID_PARAMS, "daemon_execute_bash_workflow",
                         "Script path failed security validation");
        return E_INVALID_PARAMS;
    }

    /* Validate workflow_id */
    if (strlen(workflow_id) > WORKFLOW_ID_MAX_LENGTH || strlen(workflow_id) == 0) {
        argo_report_error(E_INVALID_PARAMS, "daemon_execute_bash_workflow",
                         "Invalid workflow_id length");
        return E_INVALID_PARAMS;
    }

    /* Validate environment variables */
    for (int i = 0; i < env_count; i++) {
        if (!is_safe_env_var(env_keys[i])) {
            argo_report_error(E_INVALID_PARAMS, "daemon_execute_bash_workflow",
                             "Dangerous environment variable blocked");
            return E_INVALID_PARAMS;
        }
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

    /* Create pipe for stdin (parent writes, child reads) */
    int pipe_fds[2];
    if (pipe(pipe_fds) < 0) {
        argo_report_error(E_SYSTEM_PROCESS, "daemon_execute_bash_workflow", "pipe creation failed");
        workflow_registry_remove(daemon->workflow_registry, workflow_id);
        return E_SYSTEM_PROCESS;
    }

    /* Fork process */
    pid_t pid = fork();
    if (pid < 0) {
        /* Fork failed */
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        argo_report_error(E_SYSTEM_FORK, "daemon_execute_bash_workflow", "fork failed");
        workflow_registry_update_state(daemon->workflow_registry, workflow_id,
                                      WORKFLOW_STATE_FAILED);
        return E_SYSTEM_FORK;
    }

    if (pid == 0) {
        /* Child process - execute bash script */

        /* Setup stdin pipe (close write end, redirect read end to stdin) */
        close(pipe_fds[1]);  /* Close write end */
        dup2(pipe_fds[0], STDIN_FILENO);
        close(pipe_fds[0]);

        /* Create log directory if needed */
        const char* home = getenv("HOME");
        if (!home) home = ".";

        char log_dir[ARGO_PATH_MAX];
        snprintf(log_dir, sizeof(log_dir), "%s/.argo/logs", home);
        mkdir(log_dir, ARGO_DIR_PERMISSIONS);

        /* Redirect stdout/stderr to log file */
        char log_path[ARGO_PATH_MAX];
        snprintf(log_path, sizeof(log_path), "%s/%s.log", log_dir, workflow_id);

        int log_fd = open(log_path, O_CREAT | O_WRONLY | O_APPEND, ARGO_FILE_PERMISSIONS);
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
            /* GUIDELINE_APPROVED: Child process error before _exit */
            fprintf(stderr, "Failed to allocate exec args\n");
            _exit(E_SYSTEM_MEMORY);
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
        /* GUIDELINE_APPROVED: Child process error before _exit */
        fprintf(stderr, "Failed to execute script: %s\n", script_path);
        _exit(E_SYSTEM_PROCESS);
    }

    /* Parent process - close read end, keep write end for input */
    close(pipe_fds[0]);

    /* Update registry with PID and stdin pipe */
    workflow_registry_update_state(daemon->workflow_registry, workflow_id,
                                  WORKFLOW_STATE_RUNNING);

    /* Store PID and stdin pipe in registry entry */
    const workflow_entry_t* wf_entry = workflow_registry_find(daemon->workflow_registry, workflow_id);
    if (wf_entry) {
        /* Need to cast away const to update - this is a limitation of current API */
        workflow_entry_t* mutable_entry = (workflow_entry_t*)wf_entry;
        mutable_entry->executor_pid = pid;
        mutable_entry->stdin_pipe = pipe_fds[1];  /* Store write end for input */
    }

    LOG_INFO("Started bash workflow: %s (PID: %d, stdin_pipe: %d)", workflow_id, pid, pipe_fds[1]);
    return ARGO_SUCCESS;
}

/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_WORKFLOW_H
#define ARGO_WORKFLOW_H

#include <stdbool.h>
#include <time.h>
#include <sys/types.h>
#include "argo_limits.h"

/*
 * Simplified Workflow System
 *
 * Workflows are bash scripts with JSON configuration.
 * JSON provides metadata (name, description, timeout).
 * Bash scripts contain all workflow logic (retry, conditional, etc).
 * Daemon manages lifecycle: launch, monitor PID, apply timeout, track state.
 */

/* Workflow execution state (tracked by daemon) */
#ifndef WORKFLOW_STATE_T_DEFINED
#define WORKFLOW_STATE_T_DEFINED
typedef enum {
    WORKFLOW_STATE_PENDING = 0,    /* Queued, not started */
    WORKFLOW_STATE_RUNNING,        /* Currently executing */
    WORKFLOW_STATE_PAUSED,         /* Paused (SIGSTOP) */
    WORKFLOW_STATE_COMPLETED,      /* Finished successfully */
    WORKFLOW_STATE_FAILED,         /* Finished with error */
    WORKFLOW_STATE_ABANDONED       /* User cancelled */
} workflow_state_t;
#endif

/* Simplified workflow structure */
typedef struct {
    /* Identity */
    char workflow_id[ARGO_BUFFER_NAME];         /* Unique instance ID */
    char workflow_name[ARGO_BUFFER_NAME];       /* Human-readable name */
    char description[ARGO_BUFFER_LARGE];        /* Description */

    /* Script to execute */
    char script_path[ARGO_PATH_MAX];            /* Path to bash script */
    char working_dir[ARGO_PATH_MAX];            /* Working directory (optional) */

    /* Configuration */
    int timeout_seconds;                         /* Max execution time */

    /* Runtime state (managed by executor) */
    workflow_state_t state;                      /* Current state */
    pid_t executor_pid;                          /* Executor process PID */
    time_t start_time;                           /* Start timestamp */
    time_t end_time;                             /* End timestamp */
    int exit_code;                               /* Exit code from script */

    /* Checkpointing */
    bool checkpoint_enabled;                     /* Enable checkpoint/resume */
} workflow_t;

/**
 * Create new workflow instance
 *
 * @param workflow_id Unique identifier for this instance
 * @param workflow_name Human-readable name
 * @return New workflow instance or NULL on error
 */
workflow_t* workflow_create(const char* workflow_id, const char* workflow_name);

/**
 * Load workflow from JSON file
 *
 * JSON format:
 * {
 *   "workflow_name": "my_pipeline",
 *   "description": "Build and test",
 *   "script": "workflows/scripts/build_test.sh",
 *   "working_dir": "/path/to/project",
 *   "timeout_seconds": 3600
 * }
 *
 * @param json_path Path to JSON file
 * @param workflow_id Unique ID for this instance
 * @return Loaded workflow or NULL on error
 */
workflow_t* workflow_load_from_file(const char* json_path, const char* workflow_id);

/**
 * Load workflow from JSON string
 *
 * @param json_content JSON string
 * @param workflow_id Unique ID for this instance
 * @return Loaded workflow or NULL on error
 */
workflow_t* workflow_load_from_string(const char* json_content, const char* workflow_id);

/**
 * Execute workflow script
 *
 * Forks process, executes bash script, waits with timeout.
 * Updates workflow state and captures exit code.
 *
 * @param workflow Workflow to execute
 * @param log_path Path to log file (NULL for no logging)
 * @return ARGO_SUCCESS or error code
 */
int workflow_execute(workflow_t* workflow, const char* log_path);

/**
 * Destroy workflow and free resources
 *
 * @param workflow Workflow to destroy
 */
void workflow_destroy(workflow_t* workflow);

/**
 * Convert workflow state to string
 *
 * @param state Workflow state
 * @return String representation
 */
const char* workflow_state_to_string(workflow_state_t state);

#endif /* ARGO_WORKFLOW_H */

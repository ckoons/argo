/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_ORCHESTRATOR_API_H
#define ARGO_ORCHESTRATOR_API_H

#include <sys/types.h>
#include "argo_workflow_registry.h"

/* Workflow execution state */
typedef struct {
    char current_step[128];
    int step_number;
    int total_steps;
    char last_checkpoint[128];
    time_t step_start_time;
    time_t workflow_start_time;
    bool is_paused;
    char last_error[512];
    pid_t pid;
} workflow_execution_state_t;

/* Start workflow execution in background
 *
 * Spawns workflow executor process, redirects logs, and tracks PID.
 *
 * Parameters:
 *   workflow_id - Workflow identifier (template_instance)
 *   template_path - Path to workflow template JSON
 *   branch - Git branch to use
 *   registry - Workflow registry to update with PID
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   Error code on failure
 */
int workflow_exec_start(const char* workflow_id,
                       const char* template_path,
                       const char* branch,
                       workflow_registry_t* registry);

/* Pause workflow at next checkpoint
 *
 * Sends signal to workflow process to pause at next checkpoint.
 *
 * Parameters:
 *   workflow_id - Workflow identifier
 *   registry - Workflow registry
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   Error code on failure
 */
int workflow_exec_pause(const char* workflow_id,
                       workflow_registry_t* registry);

/* Resume workflow from last checkpoint
 *
 * Sends signal to workflow process to resume execution.
 *
 * Parameters:
 *   workflow_id - Workflow identifier
 *   registry - Workflow registry
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   Error code on failure
 */
int workflow_exec_resume(const char* workflow_id,
                        workflow_registry_t* registry);

/* Abandon workflow and cleanup
 *
 * Kills workflow process and removes from registry.
 *
 * Parameters:
 *   workflow_id - Workflow identifier
 *   registry - Workflow registry
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   Error code on failure
 */
int workflow_exec_abandon(const char* workflow_id,
                         workflow_registry_t* registry);

/* Get workflow execution state
 *
 * Queries workflow process for current execution state.
 *
 * Parameters:
 *   workflow_id - Workflow identifier
 *   registry - Workflow registry
 *   state - Output parameter for execution state
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   Error code on failure
 */
int workflow_exec_get_state(const char* workflow_id,
                           workflow_registry_t* registry,
                           workflow_execution_state_t* state);

/* Check if workflow process is alive
 *
 * Parameters:
 *   pid - Process ID
 *
 * Returns:
 *   true if process is running
 *   false if process has died
 */
bool workflow_exec_is_process_alive(pid_t pid);

#endif /* ARGO_ORCHESTRATOR_API_H */

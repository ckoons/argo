/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_WORKFLOW_REGISTRY_H
#define ARGO_WORKFLOW_REGISTRY_H

#include <sys/types.h>
#include <time.h>
#include <stdbool.h>

/* Workflow registry
 *
 * Tracks workflow execution state with JSON persistence.
 * Separate from CI registry (which tracks companion instances).
 *
 * Features:
 * - In-memory tracking of active/completed workflows
 * - JSON persistence to ~/.argo/workflow_registry.json
 * - Query by ID or list all
 * - Prune old completed workflows
 * - Survives daemon restarts
 *
 * Use Cases:
 * - Daemon tracks all spawned workflows
 * - Arc CLI queries workflow status
 * - Audit trail of past executions
 * - Cleanup of old workflow history
 *
 * THREAD SAFETY:
 * - NOT thread-safe - caller must synchronize
 * - Current usage: All access from daemon main thread or shared services thread
 * - Workflow completion task holds lock during workflow_registry_list() call
 * - No concurrent modification risk in current architecture
 * - NOTE: Workflow entries returned by find() are mutable by design (cast away const)
 *         This is safe because only daemon thread modifies them
 */

/* Workflow states */
/* NOTE: workflow_state_t is also defined in argo_workflow.h */
/* We use a guard to prevent duplicate definition when both headers are included */
#ifndef WORKFLOW_STATE_T_DEFINED
#define WORKFLOW_STATE_T_DEFINED
typedef enum {
    WORKFLOW_STATE_PENDING,    /* Queued, not started */
    WORKFLOW_STATE_RUNNING,    /* Currently executing */
    WORKFLOW_STATE_PAUSED,     /* Paused (SIGSTOP) */
    WORKFLOW_STATE_COMPLETED,  /* Finished successfully */
    WORKFLOW_STATE_FAILED,     /* Finished with error */
    WORKFLOW_STATE_ABANDONED   /* User cancelled */
} workflow_state_t;
#endif

/* Workflow entry */
typedef struct {
    char workflow_id[64];      /* Unique ID (e.g., "build-123") */
    char workflow_name[128];   /* Template name (e.g., "ci_build") */
    workflow_state_t state;    /* Current state */
    pid_t executor_pid;        /* Executor PID (0 if not running) */
    time_t start_time;         /* When started (epoch) */
    time_t end_time;           /* When finished (0 if running) */
    int exit_code;             /* Exit code (for completed/failed) */
    bool abandon_requested;    /* User requested abandon (kill + ABANDONED state) */
    int current_step;          /* Current step number */
    int total_steps;           /* Total steps in workflow */
    int timeout_seconds;       /* Workflow timeout (0 = no timeout) */
    int retry_count;           /* Number of retries attempted */
    int max_retries;           /* Maximum retry attempts (0 = no retry) */
    time_t last_retry_time;    /* Timestamp of last retry attempt */
} workflow_entry_t;

/* Opaque registry structure */
typedef struct workflow_registry workflow_registry_t;

/* Create workflow registry
 *
 * Creates empty registry. Call workflow_registry_load() to restore
 * from disk if persistence file exists.
 *
 * Returns:
 *   Registry handle on success
 *   NULL on allocation failure
 */
workflow_registry_t* workflow_registry_create(void);

/* Add workflow to registry
 *
 * Adds new workflow entry. Fails if ID already exists.
 *
 * Parameters:
 *   reg   - Registry handle
 *   entry - Workflow entry to add (copied internally)
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_NULL if reg or entry is NULL
 *   E_WORKFLOW_EXISTS if workflow_id already exists
 *   E_SYSTEM_MEMORY on allocation failure
 */
int workflow_registry_add(workflow_registry_t* reg, const workflow_entry_t* entry);

/* Remove workflow from registry
 *
 * Removes workflow entry by ID. Typically used for terminal states
 * when immediate cleanup is desired.
 *
 * Parameters:
 *   reg - Registry handle
 *   id  - Workflow ID to remove
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_NULL if reg or id is NULL
 *   E_NOT_FOUND if workflow doesn't exist
 */
int workflow_registry_remove(workflow_registry_t* reg, const char* id);

/* Update workflow state
 *
 * Updates existing workflow's state and related fields.
 * If state is COMPLETED/FAILED/ABANDONED, sets end_time.
 *
 * Parameters:
 *   reg   - Registry handle
 *   id    - Workflow ID
 *   state - New state
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_NULL if reg or id is NULL
 *   E_WORKFLOW_NOT_FOUND if workflow doesn't exist
 */
int workflow_registry_update_state(workflow_registry_t* reg, const char* id,
                                    workflow_state_t state);

/* Update workflow progress
 *
 * Updates current_step for running workflow.
 *
 * Parameters:
 *   reg          - Registry handle
 *   id           - Workflow ID
 *   current_step - Current step number
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_NULL if reg or id is NULL
 *   E_WORKFLOW_NOT_FOUND if workflow doesn't exist
 */
int workflow_registry_update_progress(workflow_registry_t* reg, const char* id,
                                       int current_step);

/* Find workflow by ID
 *
 * Retrieves workflow entry by ID.
 *
 * Parameters:
 *   reg - Registry handle
 *   id  - Workflow ID
 *
 * Returns:
 *   Pointer to entry if found (valid until next modify operation)
 *   NULL if not found or reg/id is NULL
 */
const workflow_entry_t* workflow_registry_find(const workflow_registry_t* reg,
                                                 const char* id);

/* List all workflows
 *
 * Returns array of all workflow entries.
 *
 * Parameters:
 *   reg     - Registry handle
 *   entries - Output: array of entries (caller must free)
 *   count   - Output: number of entries
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_NULL if any parameter is NULL
 *   E_SYSTEM_MEMORY on allocation failure
 *
 * Note: Caller must free(*entries) after use
 */
int workflow_registry_list(const workflow_registry_t* reg,
                            workflow_entry_t** entries, int* count);

/* Count workflows by state
 *
 * Returns number of workflows in given state.
 *
 * Parameters:
 *   reg   - Registry handle
 *   state - State to count (or -1 for all)
 *
 * Returns:
 *   Count of workflows, 0 if reg is NULL
 */
int workflow_registry_count(const workflow_registry_t* reg, workflow_state_t state);

/* Save registry to JSON file
 *
 * Persists registry to disk.
 *
 * Parameters:
 *   reg  - Registry handle
 *   path - File path (e.g., ~/.argo/workflow_registry.json)
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_NULL if reg or path is NULL
 *   E_SYSTEM_FILE if file operation fails
 *   E_SYSTEM_MEMORY on allocation failure
 */
int workflow_registry_save(const workflow_registry_t* reg, const char* path);

/* Load registry from JSON file
 *
 * Restores registry from disk. Silently succeeds if file doesn't exist.
 *
 * Parameters:
 *   reg  - Registry handle (existing entries preserved)
 *   path - File path
 *
 * Returns:
 *   ARGO_SUCCESS on success (even if file doesn't exist)
 *   E_INPUT_NULL if reg or path is NULL
 *   E_SYSTEM_FILE if file read fails
 *   E_SYSTEM_MEMORY on allocation failure
 */
int workflow_registry_load(workflow_registry_t* reg, const char* path);

/* Prune old workflows
 *
 * Removes completed/failed/abandoned workflows older than threshold.
 * Does NOT remove running/pending workflows.
 *
 * Parameters:
 *   reg         - Registry handle
 *   older_than  - Remove workflows with end_time < this (epoch)
 *
 * Returns:
 *   Number of workflows pruned, -1 on error
 *
 * Example: Prune workflows older than 7 days
 *   time_t cutoff = time(NULL) - (7 * 24 * 60 * 60);
 *   workflow_registry_prune(reg, cutoff);
 */
int workflow_registry_prune(workflow_registry_t* reg, time_t older_than);

/* Destroy registry
 *
 * Frees all memory. Does NOT save to disk automatically.
 * Call workflow_registry_save() first if persistence needed.
 *
 * Parameters:
 *   reg - Registry handle
 */
void workflow_registry_destroy(workflow_registry_t* reg);

/* Convert state to string
 *
 * Returns human-readable state name.
 *
 * Parameters:
 *   state - Workflow state
 *
 * Returns:
 *   State string (e.g., "running", "completed")
 */
const char* workflow_state_to_string(workflow_state_t state);

/* Convert string to state
 *
 * Parses state string to enum.
 *
 * Parameters:
 *   str - State string
 *
 * Returns:
 *   State enum, WORKFLOW_STATE_PENDING if unknown
 */
workflow_state_t workflow_state_from_string(const char* str);

#endif /* ARGO_WORKFLOW_REGISTRY_H */

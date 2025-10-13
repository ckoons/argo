/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_DAEMON_TASKS_H
#define ARGO_DAEMON_TASKS_H

/* Forward declaration - argo_daemon_t defined in argo_daemon.h */
typedef struct argo_daemon_struct argo_daemon_t;

/* Background task functions for shared services
 *
 * These tasks are registered with shared_services and run periodically:
 * - workflow_timeout_task: Monitors and terminates timed-out workflows
 * - log_rotation_task: Rotates old log files
 * - workflow_completion_task: Detects workflow completion and handles retries
 *
 * All tasks are called from shared services thread.
 * Context parameter is pointer to argo_daemon_t.
 */

/* Workflow timeout monitoring task
 *
 * Checks all running workflows for timeout expiration.
 * Terminates workflows that exceed their timeout.
 * Runs every WORKFLOW_TIMEOUT_CHECK_INTERVAL_SECONDS (10 seconds).
 *
 * Parameters:
 *   context - Pointer to argo_daemon_t
 */
void workflow_timeout_task(void* context);

/* Log rotation task
 *
 * Rotates log files that exceed max age or size.
 * Keeps LOG_ROTATION_KEEP_COUNT backup files.
 * Runs every LOG_ROTATION_CHECK_INTERVAL_SECONDS (1 hour).
 *
 * Parameters:
 *   context - Pointer to argo_daemon_t
 */
void log_rotation_task(void* context);

/* Workflow completion detection and retry task
 *
 * Detects workflow completion by checking if executor process still exists.
 * Handles workflow retry with exponential backoff.
 * Runs every WORKFLOW_COMPLETION_CHECK_INTERVAL_SECONDS (5 seconds).
 *
 * Parameters:
 *   context - Pointer to argo_daemon_t
 */
void workflow_completion_task(void* context);

#endif /* ARGO_DAEMON_TASKS_H */

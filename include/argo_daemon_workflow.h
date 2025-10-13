/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_DAEMON_WORKFLOW_H
#define ARGO_DAEMON_WORKFLOW_H

/* Forward declaration - argo_daemon_t defined in argo_daemon.h */
typedef struct argo_daemon_struct argo_daemon_t;

/* Workflow execution functions
 *
 * Handles execution of bash workflow scripts with security validation:
 * - Input validation (path traversal, command injection prevention)
 * - Environment variable sanitization (blocks dangerous vars)
 * - Process forking and monitoring
 */

/* Execute bash workflow script
 *
 * Forks child process to execute bash script with provided arguments and environment.
 * Validates script path and environment variables for security.
 * Creates workflow entry in registry and monitors execution.
 *
 * Security Features:
 * - Script path validation (no directory traversal, no shell metacharacters)
 * - Environment variable sanitization (blocks LD_PRELOAD, PATH, etc.)
 * - Log file redirection for output capture
 * - Workflow timeout support
 * - Retry with exponential backoff
 *
 * Parameters:
 *   daemon      - Daemon instance
 *   script_path - Path to bash script (must be absolute, validated)
 *   args        - Script arguments (array of strings)
 *   arg_count   - Number of arguments
 *   env_keys    - Environment variable names (array of strings)
 *   env_values  - Environment variable values (array of strings)
 *   env_count   - Number of environment variables
 *   workflow_id - Unique workflow identifier (max 63 characters)
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_NULL if daemon, script_path, or workflow_id is NULL
 *   E_INVALID_PARAMS if script_path or env vars fail validation
 *   E_WORKFLOW_EXISTS if workflow_id already exists
 *   E_SYSTEM_FORK if fork fails
 */
int daemon_execute_bash_workflow(argo_daemon_t* daemon,
                                 const char* script_path,
                                 char** args,
                                 int arg_count,
                                 char** env_keys,
                                 char** env_values,
                                 int env_count,
                                 const char* workflow_id);

#endif /* ARGO_DAEMON_WORKFLOW_H */

/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_ENV_H
#define ARGO_ENV_H

#include <sys/types.h>

/* Isolated environment structure
 *
 * Provides isolated environment management for child processes.
 * Unlike argo_setenv/argo_getenv (which modify process environment),
 * argo_env_t maintains a separate key-value store that can be passed
 * to child processes via execve() without polluting parent environment.
 *
 * Use Cases:
 * - Workflow executors with clean environment
 * - Testing with controlled environment
 * - Parallel processes with different configs
 *
 * Pattern:
 *   argo_env_t* env = argo_env_create();
 *   argo_env_set(env, "WORKFLOW_ID", "build-123");
 *   argo_spawn_with_env("/bin/executor", argv, env, &pid);
 *   argo_env_destroy(env);
 */

/* Opaque environment structure */
typedef struct argo_env argo_env_t;

/* Create isolated environment
 *
 * Creates empty environment. Does not copy parent's environment.
 * Caller must add all needed variables explicitly.
 *
 * Returns:
 *   Environment handle on success
 *   NULL on allocation failure
 */
argo_env_t* argo_env_create(void);

/* Set variable in isolated environment
 *
 * Adds or updates key-value pair in environment.
 * Does NOT modify process environment (unlike argo_setenv).
 *
 * Parameters:
 *   env   - Environment handle
 *   key   - Variable name (copied internally)
 *   value - Variable value (copied internally)
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_NULL if env, key, or value is NULL
 *   E_SYSTEM_MEMORY on allocation failure
 */
int argo_env_set(argo_env_t* env, const char* key, const char* value);

/* Get variable from isolated environment
 *
 * Retrieves value for key from environment.
 * Does NOT check process environment.
 *
 * Parameters:
 *   env - Environment handle
 *   key - Variable name
 *
 * Returns:
 *   Value string if found, NULL if not found or env is NULL
 *   Returned pointer valid until next argo_env_set or argo_env_destroy
 */
const char* argo_env_get(const argo_env_t* env, const char* key);

/* Convert environment to envp array for execve
 *
 * Builds NULL-terminated array of "KEY=VALUE" strings
 * suitable for execve() third argument.
 *
 * Parameters:
 *   env - Environment handle
 *
 * Returns:
 *   envp array on success (must free with argo_env_free_envp)
 *   NULL on allocation failure or if env is NULL
 *
 * Format:
 *   char** envp = argo_env_to_envp(env);
 *   execve("/bin/program", argv, envp);
 *   argo_env_free_envp(envp);
 */
char** argo_env_to_envp(const argo_env_t* env);

/* Free envp array
 *
 * Frees array returned by argo_env_to_envp.
 * Safe to call with NULL.
 *
 * Parameters:
 *   envp - Array to free
 */
void argo_env_free_envp(char** envp);

/* Spawn process with isolated environment
 *
 * Forks and executes program with specified environment.
 * Does not modify parent's environment.
 *
 * Parameters:
 *   path - Program path (absolute or in PATH)
 *   argv - Argument array (NULL-terminated)
 *   env  - Isolated environment (NULL = empty environment)
 *   pid  - Output: child process ID
 *
 * Returns:
 *   ARGO_SUCCESS on success (child spawned)
 *   E_INPUT_NULL if path, argv, or pid is NULL
 *   E_SYSTEM_PROCESS if fork fails
 *   E_SYSTEM_EXEC if exec fails in child
 *
 * Notes:
 *   - Parent continues immediately (non-blocking)
 *   - Use waitpid(*pid, ...) to wait for child
 *   - Child inherits file descriptors unless FD_CLOEXEC set
 */
int argo_spawn_with_env(const char* path, char* const argv[],
                        const argo_env_t* env, pid_t* pid);

/* Destroy isolated environment
 *
 * Frees all memory associated with environment.
 * Safe to call with NULL.
 *
 * Parameters:
 *   env - Environment to destroy
 */
void argo_env_destroy(argo_env_t* env);

/* Count variables in environment
 *
 * Returns number of variables set in environment.
 * Useful for testing and debugging.
 *
 * Parameters:
 *   env - Environment handle
 *
 * Returns:
 *   Number of variables, 0 if env is NULL
 */
int argo_env_size(const argo_env_t* env);

#endif /* ARGO_ENV_H */

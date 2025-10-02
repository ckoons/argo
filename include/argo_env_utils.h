/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_ENV_UTILS_H
#define ARGO_ENV_UTILS_H

#include <stdbool.h>

/* Environment configuration constants */
#define ARGO_ENV_INITIAL_CAPACITY 256
#define ARGO_ENV_GROWTH_QUANTUM 64
#define ARGO_ENV_LINE_MAX 4096
#define ARGO_ENV_VAR_NAME_MAX 256
#define ARGO_ENV_MAX_EXPANSION_DEPTH 10

/* Environment file names */
#define ARGO_ENV_HOME_FILE ".env"
#define ARGO_ENV_PROJECT_FILE ".env.argo"
#define ARGO_ENV_LOCAL_FILE ".env.argo.local"

/* Environment variable for root directory */
#define ARGO_ROOT_VAR "ARGO_ROOT"

/* Load Argo environment
 *
 * Loads environment in this sequence:
 * 1. System environment (environ)
 * 2. ~/.env (optional)
 * 3. ${ARGO_ROOT}/.env.argo or ./.env.argo (required)
 * 4. ${ARGO_ROOT}/.env.argo.local or ./.env.argo.local (optional)
 * 5. Expand ${VAR} references
 *
 * Thread-safe. Can be called multiple times to reload configuration.
 * Frees previous environment if already loaded.
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_SYSTEM_FILE if .env.argo not found
 *   E_SYSTEM_MEMORY if allocation fails
 */
int argo_loadenv(void);

/* Get environment variable from Argo environment
 *
 * Thread-safe. Never accesses system environment after argo_loadenv().
 *
 * Parameters:
 *   name - Variable name to look up
 *
 * Returns:
 *   Value string or NULL if not found
 */
const char* argo_getenv(const char* name);

/* Set environment variable in Argo environment
 *
 * Thread-safe. Creates new entry or overwrites existing.
 *
 * Parameters:
 *   name - Variable name
 *   value - Variable value
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_NULL if name or value is NULL
 *   E_SYSTEM_MEMORY if allocation fails
 */
int argo_setenv(const char* name, const char* value);

/* Unset environment variable in Argo environment
 *
 * Thread-safe. No error if variable doesn't exist.
 *
 * Parameters:
 *   name - Variable name to remove
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_NULL if name is NULL
 */
int argo_unsetenv(const char* name);

/* Clear all variables from Argo environment
 *
 * Thread-safe. Removes all variables but keeps allocation.
 *
 * Returns:
 *   ARGO_SUCCESS on success
 */
int argo_clearenv(void);

/* Free Argo environment and release resources
 *
 * Thread-safe. Frees all allocations and resets to empty state.
 * Safe to call multiple times.
 */
void argo_freeenv(void);

/* Get integer value from environment
 *
 * Thread-safe. Parses variable as base-10 integer with full validation.
 *
 * Parameters:
 *   name - Variable name
 *   value - Pointer to receive integer value (not modified on error)
 *
 * Returns:
 *   ARGO_SUCCESS on success and sets *value
 *   E_INPUT_NULL if name or value pointer is NULL
 *   E_PROTOCOL_FORMAT if variable not found or not valid integer
 */
int argo_getenvint(const char* name, int* value);

/* Print Argo environment to stdout
 *
 * Thread-safe. Prints all variables in NAME=VALUE format, one per line.
 * Similar to 'env' command output.
 */
void argo_env_print(void);

/* Dump Argo environment to file
 *
 * Thread-safe. Writes all variables to specified file in NAME=VALUE format.
 * Useful for debugging and diagnostics.
 *
 * Parameters:
 *   filepath - Path to output file
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_NULL if filepath is NULL
 *   E_SYSTEM_FILE if file cannot be opened
 */
int argo_env_dump(const char* filepath);

#endif /* ARGO_ENV_UTILS_H */

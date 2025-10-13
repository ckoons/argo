/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_ENV_INTERNAL_H
#define ARGO_ENV_INTERNAL_H

#include <pthread.h>
#include <stdbool.h>

/* Environment subsystem - Thread-safe environment variable storage
 *
 * THREAD SAFETY:
 * - All access to argo_env, argo_env_count, argo_env_capacity MUST be protected by argo_env_mutex
 * - argo_env_initialized is accessed atomically (bool reads/writes are atomic)
 * - All public functions in argo_env_utils.h acquire argo_env_mutex
 * - Internal helpers assume caller holds lock (documented per-function)
 *
 * PROTECTED BY argo_env_mutex:
 * - argo_env           - Environment variable array
 * - argo_env_count     - Number of variables
 * - argo_env_capacity  - Array capacity
 */

/* Shared environment state */
extern char** argo_env;              /* PROTECTED BY argo_env_mutex */
extern int argo_env_count;           /* PROTECTED BY argo_env_mutex */
extern int argo_env_capacity;        /* PROTECTED BY argo_env_mutex */
extern pthread_mutex_t argo_env_mutex;  /* PROTECTS: argo_env, argo_env_count, argo_env_capacity */
extern bool argo_env_initialized;    /* Atomic flag (bool read/write is atomic) */

/* Internal helper functions - CALLER MUST HOLD argo_env_mutex */
int grow_env_array(void);            /* LOCKS: none (caller holds argo_env_mutex) */
int find_env_index(const char* name);  /* LOCKS: none (caller holds argo_env_mutex) */
int set_env_internal(const char* name, const char* value);  /* LOCKS: none (caller holds argo_env_mutex) */

#endif /* ARGO_ENV_INTERNAL_H */

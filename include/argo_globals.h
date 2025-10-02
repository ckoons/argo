/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_GLOBALS_H
#define ARGO_GLOBALS_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

/* Global State Registry
 *
 * This file tracks ALL global/static state in the Argo library.
 * Each subsystem's globals are documented here for cleanup tracking.
 *
 * Cleanup order (reverse of initialization):
 * 1. Socket subsystem (if active)
 * 2. Config subsystem
 * 3. Environment subsystem
 */

/* ============================================================================
 * Environment Subsystem (argo_env_load.c)
 * ========================================================================= */

/* Environment variables array */
extern char** argo_env;
extern int argo_env_count;
extern int argo_env_capacity;
extern pthread_mutex_t argo_env_mutex;
extern bool argo_env_initialized;

/* Cleanup: argo_freeenv() */


/* ============================================================================
 * Socket Subsystem (argo_socket.c)
 * ========================================================================= */

/* Socket context (allocated on socket_init) */
/* Note: Not externally visible - cleanup via socket_cleanup() */
/* extern socket_context_t* g_socket_ctx; */

/* Cleanup: socket_cleanup() */


/* ============================================================================
 * Memory Subsystem (argo_memory.c)
 * ========================================================================= */

/* Next memory ID counter */
/* Note: Static counter, no cleanup needed */
/* static uint32_t g_next_memory_id; */


/* ============================================================================
 * Print Subsystem (argo_print_utils.c)
 * ========================================================================= */

/* Output handlers */
/* Note: Function pointers, reset to defaults on cleanup */
/* static argo_output_fn current_output_handler; */
/* static argo_output_fn current_error_handler; */

/* Cleanup: argo_print_reset() */


/* ============================================================================
 * API Provider Subsystem (argo_*_api.c)
 * ========================================================================= */

/* Static const config structs - no cleanup needed */
/* static const api_provider_config_t *_config; */


/* ============================================================================
 * Future Subsystems
 * ========================================================================= */

/* Config subsystem - TBD */
/* Logging subsystem - TBD */
/* Registry subsystem - TBD */


/* ============================================================================
 * CLEANUP FUNCTIONS
 *
 * All subsystem cleanup functions are declared here for argo_exit().
 * These functions are also declared in their respective public headers.
 * This section provides a centralized cleanup contract.
 *
 * CLEANUP ORDER (reverse of initialization):
 * 1. socket_server_cleanup() - if socket server was started
 * 2. argo_config_cleanup() - config subsystem
 * 3. argo_freeenv() - environment subsystem
 * ========================================================================= */

/* Environment subsystem cleanup
 * Declared in: argo_env_utils.h
 * Frees: argo_env, resets argo_env_count, argo_env_capacity, argo_env_initialized
 * Safe to call multiple times */
extern void argo_freeenv(void);

/* Configuration subsystem cleanup
 * Declared in: argo_config.h
 * Frees: config structures (future)
 * Safe to call multiple times */
extern void argo_config_cleanup(void);

/* Socket server cleanup (if started by application)
 * Declared in: argo_socket.h
 * Frees: g_socket_ctx, closes sockets, removes socket file
 * Safe to call multiple times
 * NOTE: Application-managed, not called by argo_exit() */
extern void socket_server_cleanup(void);

#endif /* ARGO_GLOBALS_H */

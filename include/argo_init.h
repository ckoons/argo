/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_INIT_H
#define ARGO_INIT_H

/* Argo Initialization and Cleanup
 *
 * Core lifecycle management for the Argo library.
 * All Argo applications should call argo_init() before using any other
 * Argo functions, and argo_exit() when done.
 *
 * Typical usage:
 *
 *   int main(void) {
 *       if (argo_init() != ARGO_SUCCESS) {
 *           return 1;  // Init handles its own cleanup
 *       }
 *
 *       // Application-specific work using Argo APIs
 *       run_my_application();
 *
 *       argo_exit();
 *       return 0;
 *   }
 */

/* Initialize Argo library
 *
 * Initialization sequence:
 * 1. Load environment (searches for .env.argo, sets ARGO_ROOT)
 * 2. Load configuration (creates .argo/ structure, loads config.json)
 *
 * Does NOT:
 * - Change current working directory
 * - Start background services (socket server, etc.)
 * - Allocate provider instances
 *
 * Those are application responsibilities.
 *
 * If initialization fails at any step, automatically cleans up
 * previously initialized subsystems before returning error.
 *
 * Can be called multiple times (re-initializes). Must call argo_exit()
 * before re-init if you want a clean slate.
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_SYSTEM_FILE if .env.argo not found or directories cannot be created
 *   E_SYSTEM_MEMORY if allocation fails
 */
int argo_init(void);

/* Cleanup Argo library
 *
 * Cleanup sequence (reverse of init):
 * 1. Cleanup configuration subsystem
 * 2. Cleanup environment subsystem
 *
 * Safe to call multiple times.
 * Safe to call even if argo_init() failed.
 * After argo_exit(), must call argo_init() again before using Argo APIs.
 *
 * Note: Does NOT cleanup resources created by the application:
 * - Socket servers (call socket_server_cleanup())
 * - Provider instances (call provider cleanup functions)
 * - Registries (call registry cleanup functions)
 * - etc.
 */
void argo_exit(void);

#endif /* ARGO_INIT_H */

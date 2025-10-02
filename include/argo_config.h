/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_CONFIG_H
#define ARGO_CONFIG_H

/* Configuration subsystem
 *
 * Loads and manages Argo configuration from .argo/config.json
 * Creates required directory structure.
 *
 * Configuration is loaded during argo_init() after environment is loaded,
 * so configuration can access environment variables via argo_getenv().
 */

/* Load Argo configuration
 *
 * Sequence:
 * 1. Create .argo/ directory structure if needed
 * 2. Load .argo/config.json if exists
 * 3. Apply configuration settings
 *
 * Uses ARGO_ROOT from environment to locate .argo directory.
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_SYSTEM_FILE if required directories cannot be created
 *   E_SYSTEM_MEMORY if allocation fails
 */
int argo_config(void);

/* Cleanup configuration subsystem
 *
 * Frees all configuration resources.
 * Safe to call multiple times.
 */
void argo_config_cleanup(void);

#endif /* ARGO_CONFIG_H */

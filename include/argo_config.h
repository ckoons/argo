/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_CONFIG_H
#define ARGO_CONFIG_H

/* Configuration subsystem
 *
 * Loads and manages Argo configuration from config/ directory hierarchies.
 * Unlike .env files (for application config like API keys), config/ directories
 * hold argo-specific settings (daemon config, workflow defaults, etc.).
 *
 * Directory hierarchy (loaded in order, later overrides earlier):
 *   1. ~/.argo/config/           (user-wide argo settings)
 *   2. <project>/.argo/config/   (project argo settings)
 *   3. <project>/workflows/config/ (workflow-specific settings)
 *
 * Configuration is loaded during argo_init() after environment is loaded,
 * so config files can reference environment variables.
 *
 * Format: Simple KEY=VALUE files (one per line, # for comments)
 */

/* Load Argo configuration
 *
 * Sequence:
 * 1. Create .argo/ directory structure if needed
 * 2. Scan and load config/ directories in precedence order
 * 3. Store config in memory for argo_config_get() access
 *
 * Uses ARGO_ROOT from environment to locate project config directories.
 * Silently skips missing directories/files (all are optional).
 *
 * Returns:
 *   ARGO_SUCCESS on success (even if no config found)
 *   E_SYSTEM_FILE if required directories cannot be created
 *   E_SYSTEM_MEMORY if allocation fails
 */
int argo_config(void);

/* Get configuration value
 *
 * Retrieves config value by key. Returns NULL if key not found.
 *
 * Parameters:
 *   key - Configuration key to lookup
 *
 * Returns:
 *   Value string if found, NULL if not found
 *   Returned pointer is valid until argo_config_cleanup() is called
 */
const char* argo_config_get(const char* key);

/* Reload configuration
 *
 * Reloads config from disk, useful if config files changed.
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   Error codes same as argo_config()
 */
int argo_config_reload(void);

/* Cleanup configuration subsystem
 *
 * Frees all configuration resources.
 * Safe to call multiple times.
 */
void argo_config_cleanup(void);

#endif /* ARGO_CONFIG_H */

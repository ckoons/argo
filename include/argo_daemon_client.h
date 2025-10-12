/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_DAEMON_CLIENT_H
#define ARGO_DAEMON_CLIENT_H

/* Daemon client utilities
 *
 * Provides helpers for connecting to argo-daemon from any component.
 * Supports both local (localhost) and remote daemon connections via config.
 *
 * Configuration precedence:
 *   1. Config files: daemon_host, daemon_port from config hierarchy
 *   2. Environment: ARGO_DAEMON_HOST, ARGO_DAEMON_PORT
 *   3. Defaults: localhost:9876
 *
 * This ensures location independence - daemon can run on remote host
 * without changing code, only configuration.
 */

/* Default daemon connection settings */
#define ARGO_DAEMON_DEFAULT_HOST "localhost"
#define ARGO_DAEMON_DEFAULT_PORT 9876
#define ARGO_DAEMON_HOST_ENV "ARGO_DAEMON_HOST"
#define ARGO_DAEMON_PORT_ENV "ARGO_DAEMON_PORT"

/* Get daemon host
 *
 * Returns configured daemon hostname or IP.
 * Checks: config → environment → default
 *
 * Returns:
 *   Hostname/IP string (e.g., "localhost", "daemon.example.com", "192.168.1.10")
 *   Never returns NULL - always has default fallback
 */
const char* argo_get_daemon_host(void);

/* Get daemon port
 *
 * Returns configured daemon port number.
 * Checks: config → environment → default
 *
 * Returns:
 *   Port number (e.g., 9876, 8080)
 *   Never returns 0 - always has default fallback
 */
int argo_get_daemon_port(void);

/* Get complete daemon URL
 *
 * Returns full base URL for daemon API.
 * Format: "http://host:port" (e.g., "http://localhost:9876")
 *
 * Returns:
 *   Base URL string - valid until next call (static buffer)
 *   Never returns NULL
 *
 * Usage:
 *   char url[512];
 *   snprintf(url, sizeof(url), "%s/api/workflow/start", argo_get_daemon_url());
 */
const char* argo_get_daemon_url(void);

#endif /* ARGO_DAEMON_CLIENT_H */

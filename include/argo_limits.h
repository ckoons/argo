/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_LIMITS_H
#define ARGO_LIMITS_H

/*
 * Argo System Limits and Buffer Sizes
 *
 * This header defines all numeric constants used throughout the Argo system
 * to ensure consistency and maintainability.
 */

/* ===== Buffer Sizes ===== */

/* Small buffers for names, identifiers, and short strings */
#define ARGO_BUFFER_TINY 32
#define ARGO_BUFFER_SMALL 64
#define ARGO_BUFFER_NAME 128
#define ARGO_BUFFER_MEDIUM 256

/* Standard I/O and content buffers */
#define ARGO_BUFFER_STANDARD 4096
#define ARGO_BUFFER_LARGE 16384

/* ===== Memory Context Estimates ===== */

/* Additional space for formatting and separators */
#define MEMORY_NOTES_PADDING 50

/* Estimated size per breadcrumb entry */
#define MEMORY_BREADCRUMB_SIZE 100

/* Estimated size per selected memory entry */
#define MEMORY_SELECTED_SIZE 200

/* Buffer overhead for memory context assembly */
#define MEMORY_BUFFER_OVERHEAD 500

/* ===== Port Configuration ===== */

/* Base port for CI service allocation */
#define REGISTRY_BASE_PORT 9000

/* Maximum number of ports per role */
#define REGISTRY_PORT_RANGE 50

/* Port offsets by CI role */
#define REGISTRY_PORT_OFFSET_BUILDER 0
#define REGISTRY_PORT_OFFSET_COORDINATOR 10
#define REGISTRY_PORT_OFFSET_REQUIREMENTS 20
#define REGISTRY_PORT_OFFSET_ANALYSIS 30
#define REGISTRY_PORT_OFFSET_RESERVED 40

/* Number of ports available per role */
#define REGISTRY_PORTS_PER_ROLE 10

/* ===== Timeouts and Intervals ===== */

/* Heartbeat timeout in seconds */
#define HEARTBEAT_TIMEOUT_SECONDS 60

/* Health check stale threshold in seconds */
#define HEALTH_CHECK_STALE_SECONDS 60

/* Periodic workflow cleanup interval in seconds (30 minutes) */
#define WORKFLOW_CLEANUP_INTERVAL_SECONDS 1800

/* ===== Provider Defaults ===== */

/* Ollama default port */
#define OLLAMA_DEFAULT_PORT 11434

/* Claude context window size */
#define CLAUDE_CONTEXT_WINDOW 200000

/* ===== Merge Confidence ===== */

/* Default merge confidence when not specified */
#define MERGE_DEFAULT_CONFIDENCE 50

/* Minimum and maximum confidence values */
#define MERGE_MIN_CONFIDENCE 0
#define MERGE_MAX_CONFIDENCE 100

/* ===== Merge Buffer Sizes ===== */

/* Conflict report buffer size */
#define MERGE_CONFLICT_BUFFER_SIZE 4096

/* Full merge result buffer size */
#define MERGE_RESULT_BUFFER_SIZE 16384

/* ===== Filesystem Paths and Permissions ===== */

/* Maximum path length for Argo filesystem operations */
#define ARGO_PATH_MAX 512

/* Directory permissions for Argo-created directories */
#define ARGO_DIR_PERMISSIONS 0755

/* File permissions for Argo-created files */
#define ARGO_FILE_PERMISSIONS 0644

/* ===== Shutdown and Tracking Limits ===== */

/* Maximum number of workflows tracked for shutdown */
#define MAX_TRACKED_WORKFLOWS 32

/* Maximum number of registries tracked for shutdown */
#define MAX_TRACKED_REGISTRIES 8

/* Maximum number of lifecycles tracked for shutdown */
#define MAX_TRACKED_LIFECYCLES 8

/* ===== HTTP and Network ===== */

/* Maximum valid TCP port number */
#define MAX_TCP_PORT 65535

/* HTTP server configuration */
#define HTTP_BUFFER_SIZE 16384  /* Main HTTP buffer (same as ARGO_BUFFER_LARGE) */
#define HTTP_MAX_ROUTES 64      /* Maximum number of routes */
#define HTTP_BACKLOG 10         /* Listen backlog */
#define HTTP_METHOD_SIZE 16     /* HTTP method string size (GET, POST, etc.) */
#define HTTP_PATH_SIZE 256      /* HTTP path buffer size */

#endif /* ARGO_LIMITS_H */

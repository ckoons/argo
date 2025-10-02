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

#endif /* ARGO_LIMITS_H */

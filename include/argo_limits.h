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

/* ===== Workflow Limits ===== */

/* Maximum workflow log file size (poor man's loop catcher) */
#define MAX_WORKFLOW_LOG_SIZE (100 * 1024 * 1024)  /* 100MB */

/* Maximum queued input lines per workflow */
#define MAX_WORKFLOW_INPUT_QUEUE 10

/* Default workflow timeout (1 hour) */
#define DEFAULT_WORKFLOW_TIMEOUT_SECONDS 3600

/* Maximum workflow timeout (24 hours) */
#define MAX_WORKFLOW_TIMEOUT_SECONDS 86400

/* Default maximum retry attempts */
#define DEFAULT_MAX_RETRY_ATTEMPTS 3

/* Retry delay base (exponential backoff base in seconds) */
#define RETRY_DELAY_BASE_SECONDS 5

/* Standard timeout exit code (matches GNU timeout command) */
#define WORKFLOW_TIMEOUT_EXIT_CODE 124

/* Poll interval for child process status (microseconds, 100ms) */
#define WORKFLOW_POLL_INTERVAL_USEC 100000

/* strtol base for decimal number parsing */
#define DECIMAL_BASE 10

/* Command exit codes */
#define EXIT_CODE_COMMAND_NOT_FOUND 127
#define EXIT_CODE_SUCCESS 0
#define EXIT_CODE_FAILURE 1

/* Port validation */
#define MIN_VALID_PORT 1
#define MAX_VALID_PORT 65535

/* Common system constants */
#define TIMESTAMP_BUFFER_SIZE 32
#define SEARCH_BUFFER_SIZE 256
#define PREFIX_BUFFER_SIZE 128
#define PID_STR_BUFFER_SIZE 32
#define ERROR_MSG_BUFFER_SIZE 256
#define RESPONSE_SIZE_MULTIPLIER 2
#define RESPONSE_SIZE_OVERHEAD 512
#define WORKFLOW_ID_MAX_LENGTH 63
#define WORKFLOW_ID_BUFFER_SIZE 128
#define RESPONSE_JSON_BUFFER_SIZE 512
#define WORKFLOW_LIST_SIZE_PER_ITEM 256
#define WORKFLOW_LIST_SIZE_BASE 100
#define WORKFLOW_LIST_SIZE_MARGIN 10

/* Daemon polling constants */
#define DAEMON_PORT_FREE_MAX_ATTEMPTS 20
#define DAEMON_PORT_FREE_DELAY_USEC 100000  /* 100ms */

/* Workflow timeout check interval (every 10 seconds) */
#define WORKFLOW_TIMEOUT_CHECK_INTERVAL_SECONDS 10

/* Workflow completion check interval (every 5 seconds) */
#define WORKFLOW_COMPLETION_CHECK_INTERVAL_SECONDS 5

/* ===== JSON Workflow Limits ===== */

/* Maximum workflow JSON file size (1MB) */
#define WORKFLOW_MAX_JSON_SIZE (1024 * 1024)

/* Maximum steps per workflow */
#define WORKFLOW_MAX_STEPS 1000

/* Maximum step name length */
#define WORKFLOW_MAX_STEP_NAME 256

/* Maximum workflow name length */
#define WORKFLOW_MAX_NAME 128

/* Maximum workflow description length */
#define WORKFLOW_MAX_DESCRIPTION 512

/* Maximum variable name length */
#define WORKFLOW_MAX_VAR_NAME 64

/* Maximum variable value length (64KB) */
#define WORKFLOW_MAX_VAR_VALUE (64 * 1024)

/* Maximum loop iterations (safety limit) */
#define WORKFLOW_MAX_LOOP_ITERATIONS 1000

/* Maximum nesting depth for loops/conditionals */
#define WORKFLOW_MAX_NESTING_DEPTH 10

/* Maximum number of variables per workflow */
#define WORKFLOW_MAX_VARIABLES 256

/* Default provider name */
#define WORKFLOW_DEFAULT_PROVIDER "claude_code"

/* Default model name */
#define WORKFLOW_DEFAULT_MODEL "claude-sonnet-4-5"

/* Workflow version string */
#define WORKFLOW_SCHEMA_VERSION "1.0"

/* Log rotation check interval (every hour) */
#define LOG_ROTATION_CHECK_INTERVAL_SECONDS 3600

/* Maximum log file age before rotation (7 days) */
#define LOG_MAX_AGE_SECONDS (7 * 24 * 60 * 60)

/* Maximum log file size before rotation (50MB) */
#define LOG_MAX_SIZE_BYTES (50 * 1024 * 1024)

/* Number of rotated log files to keep */
#define LOG_ROTATION_KEEP_COUNT 5

/* ===== Provider Defaults ===== */

/* Ollama default port */
#define OLLAMA_DEFAULT_PORT 11434

/* Claude context window size */
#define CLAUDE_CONTEXT_WINDOW 200000

/* Claude Code provider configuration */
#define CLAUDE_CODE_PROVIDER_NAME "claude_code"
#define CLAUDE_CODE_RESPONSE_BUFFER_SIZE (1024 * 1024)  /* 1MB for response */
#define CLAUDE_CODE_READ_CHUNK_SIZE 4096
#define CLAUDE_CODE_MODEL_SIZE 128

/* ===== Unit Conversions ===== */

/* Percentage calculations */
#define PERCENTAGE_DIVISOR 100

/* Time conversions */
#define MICROSECONDS_PER_MILLISECOND 1000
#define SECONDS_PER_MINUTE 60
#define MINUTES_PER_HOUR 60
#define HOURS_PER_DAY 24
#define SECONDS_PER_DAY (HOURS_PER_DAY * MINUTES_PER_HOUR * SECONDS_PER_MINUTE)

/* Data size conversions */
#define BYTES_PER_KILOBYTE 1024
#define KILOBYTES_PER_MEGABYTE 1024
#define BYTES_PER_MEGABYTE (BYTES_PER_KILOBYTE * KILOBYTES_PER_MEGABYTE)

/* ===== JSON Parsing ===== */

/* sscanf buffer size limits (buffer_size - 1 for null terminator) */
#define SSCANF_FMT_BUFFER_NAME_SIZE 127   /* ARGO_BUFFER_NAME - 1 */
#define SSCANF_FMT_BUFFER_SMALL_SIZE 63   /* ARGO_BUFFER_SMALL - 1 */
#define SSCANF_FMT_BUFFER_TINY_SIZE 31    /* ARGO_BUFFER_TINY - 1 */

/* sscanf format string helpers */
#define SSCANF_FMT_NAME "%127[^\"]"
#define SSCANF_FMT_SMALL "%63[^\"]"
#define SSCANF_FMT_TINY "%31[^\"]"

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

/* File mode strings */
#define FILE_MODE_READ "r"
#define FILE_MODE_WRITE "w"
#define FILE_MODE_APPEND "a"
#define FILE_MODE_READ_BINARY "rb"
#define FILE_MODE_WRITE_BINARY "wb"

/* Common environment variables */
#define ENV_VAR_HOME "HOME"
#define ENV_VAR_PATH "PATH"

/* Common shell/binary paths */
#define SHELL_PATH_BASH "/bin/bash"
#define BINARY_NAME_CLAUDE "claude"

/* Status strings */
#define STATUS_STR_UNKNOWN "UNKNOWN"

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

/* HTTP I/O channel timeouts (seconds) */
#define IO_HTTP_WRITE_TIMEOUT_SEC 5  /* Timeout for HTTP POST output */
#define IO_HTTP_READ_TIMEOUT_SEC 2   /* Timeout for HTTP GET input */

/* HTTP I/O channel polling configuration */
#define IO_HTTP_POLL_DELAY_USEC 100000      /* 100ms between polls */
#define IO_HTTP_POLL_MAX_ATTEMPTS 300       /* 30 seconds total timeout */

#endif /* ARGO_LIMITS_H */

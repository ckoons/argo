/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_ERROR_MESSAGES_H
#define ARGO_ERROR_MESSAGES_H

/*
 * Centralized error messages for internationalization support
 *
 * All error message strings are defined here to enable:
 * - Easy translation to other languages
 * - Consistent messaging across the codebase
 * - Simpler maintenance and updates
 *
 * Usage: argo_report_error(code, "function_name", ERR_MSG_CONSTANT)
 */

/* System-level error messages */
#define ERR_MSG_MEMORY_ALLOC_FAILED     "memory allocation failed"
#define ERR_MSG_FILE_OPEN_FAILED        "failed to open file"
#define ERR_MSG_FILE_READ_FAILED        "failed to read file"
#define ERR_MSG_FILE_WRITE_FAILED       "failed to write file"
#define ERR_MSG_SOCKET_CREATE_FAILED    "socket creation failed"
#define ERR_MSG_SOCKET_BIND_FAILED      "socket bind failed"
#define ERR_MSG_SOCKET_LISTEN_FAILED    "socket listen failed"
#define ERR_MSG_SOCKET_CONNECT_FAILED   "socket connect failed"
#define ERR_MSG_SOCKET_ACCEPT_FAILED    "socket accept failed"
#define ERR_MSG_SOCKET_SEND_FAILED      "socket send failed"
#define ERR_MSG_SOCKET_RECV_FAILED      "socket receive failed"
#define ERR_MSG_SOCKET_POLL_FAILED      "poll() failed"
#define ERR_MSG_SOCKET_ERROR            "socket error"
#define ERR_MSG_QUEUE_FULL              "queue full"
#define ERR_MSG_FORK_FAILED             "process fork failed"
#define ERR_MSG_EXEC_FAILED             "process exec failed"
#define ERR_MSG_PIPE_FAILED             "pipe creation failed"
#define ERR_MSG_MMAP_FAILED             "memory mapping failed"
#define ERR_MSG_MUNMAP_FAILED           "memory unmap failed"
#define ERR_MSG_FTRUNCATE_FAILED        "file truncate failed"
#define ERR_MSG_MSYNC_FAILED            "memory sync failed"
#define ERR_MSG_FCNTL_GETFL_FAILED      "fcntl(F_GETFL) failed"
#define ERR_MSG_FCNTL_SETFL_FAILED      "fcntl(F_SETFL) failed"

/* Network/HTTP error messages */
#define ERR_MSG_HTTP_REQUEST_FAILED     "HTTP request failed"
#define ERR_MSG_HTTP_POST_FAILED        "HTTP POST failed"
#define ERR_MSG_HTTP_RESPONSE_INVALID   "invalid HTTP response"
#define ERR_MSG_CONNECTION_FAILED       "connection failed"
#define ERR_MSG_CONNECTION_TIMEOUT      "connection timeout"
#define ERR_MSG_HTTP_BAD_REQUEST        "HTTP 400: bad request"
#define ERR_MSG_HTTP_UNAUTHORIZED       "HTTP 401: unauthorized"
#define ERR_MSG_HTTP_FORBIDDEN          "HTTP 403: forbidden"
#define ERR_MSG_HTTP_NOT_FOUND          "HTTP 404: not found"
#define ERR_MSG_HTTP_RATE_LIMIT         "HTTP 429: rate limit exceeded"
#define ERR_MSG_HTTP_SERVER_ERROR       "HTTP 500+: server error"

/* CI provider error messages */
#define ERR_MSG_PROVIDER_CREATE_FAILED  "provider creation failed"
#define ERR_MSG_PROVIDER_INIT_FAILED    "provider initialization failed"
#define ERR_MSG_PROVIDER_CONNECT_FAILED "provider connection failed"
#define ERR_MSG_PROVIDER_QUERY_FAILED   "provider query failed"
#define ERR_MSG_PROVIDER_NOT_AVAILABLE  "provider not available"
#define ERR_MSG_PROVIDER_NOT_FOUND      "provider not found"
#define ERR_MSG_CI_TIMEOUT              "CI request timeout"
#define ERR_MSG_CI_DISCONNECTED         "CI disconnected"
#define ERR_MSG_CI_CONFUSED             "CI confused"
#define ERR_MSG_CI_SCOPE_CREEP          "CI scope creep detected"

/* JSON/Protocol error messages */
#define ERR_MSG_JSON_PARSE_FAILED       "JSON parse failed"
#define ERR_MSG_JSON_EXTRACT_FAILED     "JSON extraction failed"
#define ERR_MSG_JSON_BUILD_FAILED       "JSON build failed"
#define ERR_MSG_FIELD_NOT_FOUND         "field not found in JSON"
#define ERR_MSG_PROTOCOL_FORMAT         "protocol format error"
#define ERR_MSG_PROTOCOL_SIZE           "protocol size exceeded"
#define ERR_MSG_INVALID_MESSAGE         "invalid message format"

/* Registry/Workflow error messages */
#define ERR_MSG_REGISTRY_ALLOC_FAILED   "failed to allocate registry"
#define ERR_MSG_REGISTRY_FULL           "registry full"
#define ERR_MSG_CI_NOT_FOUND            "CI not found"
#define ERR_MSG_CI_ALREADY_EXISTS       "CI already exists"
#define ERR_MSG_WORKFLOW_START_FAILED   "failed to start workflow"
#define ERR_MSG_WORKFLOW_INVALID_PHASE  "invalid workflow phase"
#define ERR_MSG_WORKFLOW_ALLOC_FAILED   "failed to allocate workflow"
#define ERR_MSG_TASK_ALLOC_FAILED       "failed to allocate task"
#define ERR_MSG_TASK_NOT_FOUND          "task not found"
#define ERR_MSG_CONFLICT_NOT_FOUND      "conflict not found"
#define ERR_MSG_NO_ACTIVE_MERGE         "no active merge negotiation"
#define ERR_MSG_PORT_ALLOCATION_FAILED  "port allocation failed"
#define ERR_MSG_MESSAGE_SEND_FAILED     "message send failed"
#define ERR_MSG_NO_RESPONSE_FIELD       "no response field in JSON"
#define ERR_MSG_UNTERMINATED_STRING     "unterminated string"
#define ERR_MSG_CHECKPOINT_FAILED       "checkpoint failed"
#define ERR_MSG_RESTORE_FAILED          "restore failed"

/* Input validation error messages */
#define ERR_MSG_NULL_POINTER            "null pointer"
#define ERR_MSG_INVALID_RANGE           "value out of range"
#define ERR_MSG_INVALID_FORMAT          "invalid format"
#define ERR_MSG_INVALID_CONFIG          "invalid configuration"
#define ERR_MSG_CONFIG_IS_NULL          "config is NULL"

/* Internal logic error messages */
#define ERR_MSG_INTERNAL_LOGIC          "internal logic error"
#define ERR_MSG_INTERNAL_CORRUPT        "internal corruption detected"
#define ERR_MSG_INTERNAL_ASSERT         "assertion failed"
#define ERR_MSG_NOT_IMPLEMENTED         "not implemented"

/* API-specific error messages */
#define ERR_MSG_API_KEY_MISSING         "API key missing"
#define ERR_MSG_API_KEY_INVALID         "API key invalid"
#define ERR_MSG_API_RATE_LIMIT          "API rate limit exceeded"
#define ERR_MSG_API_QUOTA_EXCEEDED      "API quota exceeded"

/* Memory/Context error messages */
#define ERR_MSG_MEMORY_LIMIT_EXCEEDED   "memory limit exceeded"
#define ERR_MSG_CONTEXT_LIMIT_EXCEEDED  "context limit exceeded"
#define ERR_MSG_BUFFER_TOO_SMALL        "buffer too small"
#define ERR_MSG_SIZE_TOO_LARGE          "size too large"

/* Ollama-specific messages */
#define ERR_MSG_OLLAMA_NOT_RUNNING      "Ollama not running"
#define ERR_MSG_OLLAMA_CONNECT_FAILED   "failed to connect to Ollama"
#define ERR_MSG_OLLAMA_MODEL_NOT_FOUND  "Ollama model not found"
#define ERR_MSG_NO_JSON_IN_RESPONSE     "no JSON in response"
#define ERR_MSG_RECV_ERROR              "recv() error"

/* Format strings for parameterized messages */
#define ERR_FMT_FAILED_AT_PORT          "failed to connect at localhost:%d"
#define ERR_FMT_FAILED_TO_OPEN          "failed to open %s"
#define ERR_FMT_FROM_TO                 "from %s to %s"
#define ERR_FMT_TARGET_CI               "target CI %s"
#define ERR_FMT_STATUS_CODE             "status %d"
#define ERR_FMT_SIZE_VALUE              "size %zu"
#define ERR_FMT_SYSCALL_ERROR           "%s: %s"

#endif /* ARGO_ERROR_MESSAGES_H */

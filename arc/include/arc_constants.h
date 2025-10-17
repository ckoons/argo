/* © 2025 Casey Koons All rights reserved */
#ifndef ARC_CONSTANTS_H
#define ARC_CONSTANTS_H

/* Buffer sizes */
#define ARC_ENV_NAME_MAX 128
#define ARC_CONTEXT_MAX 128
#define ARC_WORKFLOW_ID_MAX 128
#define ARC_READ_CHUNK_SIZE 4096

/* Common buffer sizes for arc CLI */
#define ARC_RESPONSE_BUFFER_TINY 10
#define ARC_LINE_BUFFER 1024
#define ARC_PATH_BUFFER 512
#define ARC_JSON_BUFFER 4096
#define ARC_STRING_BUFFER_SMALL 32
#define ARC_STRING_BUFFER_MEDIUM 256

/* HTTP status codes arc needs to check */
#define ARC_HTTP_STATUS_OK 200
#define ARC_HTTP_STATUS_NOT_FOUND 404
#define ARC_HTTP_STATUS_CONFLICT 409

/* Parsing constants */
#define ARC_SSCANF_FIELD_SMALL 31
#define ARC_SSCANF_FIELD_MEDIUM 127
#define ARC_SSCANF_FIELD_LARGE 255
#define ARC_SSCANF_FIELD_PATH 511

/* Array limits */
#define ARC_MAX_ENV_VARS 32

/* JSON parsing constants */
#define ARC_JSON_MARGIN 10
#define ARC_JSON_OFFSET_ID 16  /* strlen("{\"workflow_id\":\"") */
#define ARC_MIN_DESC_LEN 10  /* Minimum meaningful description length */

/* Daemon defaults */
#define ARC_DEFAULT_DAEMON_PORT 9876

/* Network limits */
#define ARC_MAX_PORT 65536

/* HTTP client buffers */
#define ARC_URL_BUFFER 256
#define ARC_PORT_STRING_BUFFER 16

/* Polling intervals */
#define ARC_POLLING_INTERVAL_US 100000  /* 100ms in microseconds */

/* JSON buffer for attach */
#define ARC_ATTACH_JSON_BUFFER 2048

#endif /* ARC_CONSTANTS_H */

/* © 2025 Casey Koons All rights reserved */
#ifndef CI_CONSTANTS_H
#define CI_CONSTANTS_H

/* Buffer sizes */
#define CI_QUERY_MAX 8192
#define CI_RESPONSE_MAX 32768
#define CI_READ_CHUNK_SIZE 4096
#define CI_REQUEST_BUFFER_SIZE 16384

/* HTTP timeouts */
#define CI_QUERY_TIMEOUT_SECONDS 120

/* Daemon defaults */
#define CI_DEFAULT_DAEMON_PORT 9876

/* Network limits */
#define CI_MAX_PORT 65536

/* HTTP client buffers */
#define CI_URL_BUFFER 512
#define CI_PORT_STRING_BUFFER 16

/* HTTP status codes */
#define CI_HTTP_STATUS_OK 200

/* JSON size calculation constants */
#define CI_JSON_OVERHEAD 512  /* Extra space for escaping + JSON structure */
#define CI_JSON_SIZE_MULTIPLIER 2  /* Multiply prompt length for escaping */

#endif /* CI_CONSTANTS_H */

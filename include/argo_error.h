/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_ERROR_H
#define ARGO_ERROR_H

/* Success */
#define ARGO_SUCCESS 0

/* Error reporting buffer size */
#define ERROR_LINE_BUFFER_SIZE 512

/* Error type definitions */
#define ERR_SYSTEM   0x01
#define ERR_CI       0x02
#define ERR_INPUT    0x03
#define ERR_PROTOCOL 0x04
#define ERR_INTERNAL 0x05

/* Macro to create error codes: TYPE:NUMBER format */
#define ARGO_ERROR(type, num) (((type) << 16) | (num))

/* Extract type and number from error code */
#define ARGO_ERROR_TYPE(code) (((code) >> 16) & 0xFF)
#define ARGO_ERROR_NUM(code) ((code) & 0xFFFF)

/* System errors (SYSTEM:1xxx) */
#define E_SYSTEM_MEMORY     ARGO_ERROR(ERR_SYSTEM, 1001)
#define E_SYSTEM_SOCKET     ARGO_ERROR(ERR_SYSTEM, 1002)
#define E_SYSTEM_FILE       ARGO_ERROR(ERR_SYSTEM, 1003)
#define E_SYSTEM_FORK       ARGO_ERROR(ERR_SYSTEM, 1004)
#define E_SYSTEM_PERMISSION ARGO_ERROR(ERR_SYSTEM, 1005)
#define E_SYSTEM_TIMEOUT    ARGO_ERROR(ERR_SYSTEM, 1006)
#define E_SYSTEM_SSL        ARGO_ERROR(ERR_SYSTEM, 1007)
#define E_SYSTEM_NETWORK    ARGO_ERROR(ERR_SYSTEM, 1008)
#define E_SYSTEM_PROCESS    ARGO_ERROR(ERR_SYSTEM, 1009)
#define E_SYSTEM_THREAD     ARGO_ERROR(ERR_SYSTEM, 1010)
#define E_SYSTEM_IO         ARGO_ERROR(ERR_SYSTEM, 1011)
#define E_IO_EOF            ARGO_ERROR(ERR_SYSTEM, 1012)  /* End of file/stream */
#define E_IO_WOULDBLOCK     ARGO_ERROR(ERR_SYSTEM, 1013)  /* Non-blocking I/O would block */
#define E_IO_INVALID        ARGO_ERROR(ERR_SYSTEM, 1014)  /* Invalid I/O operation */
#define E_BUFFER_OVERFLOW   ARGO_ERROR(ERR_SYSTEM, 1015)  /* Buffer overflow */

/* CI errors (CI:2xxx) */
#define E_CI_TIMEOUT        ARGO_ERROR(ERR_CI, 2001)
#define E_CI_CONFUSED       ARGO_ERROR(ERR_CI, 2002)
#define E_CI_SCOPE_CREEP    ARGO_ERROR(ERR_CI, 2003)
#define E_CI_INVALID        ARGO_ERROR(ERR_CI, 2004)
#define E_CI_CONFLICT       ARGO_ERROR(ERR_CI, 2005)
#define E_CI_OVERLOAD       ARGO_ERROR(ERR_CI, 2006)
#define E_CI_DISCONNECTED   ARGO_ERROR(ERR_CI, 2007)
#define E_CI_NO_PROVIDER    ARGO_ERROR(ERR_CI, 2008)

/* Input errors (INPUT:3xxx) */
#define E_INPUT_NULL        ARGO_ERROR(ERR_INPUT, 3001)
#define E_INPUT_RANGE       ARGO_ERROR(ERR_INPUT, 3002)
#define E_INPUT_FORMAT      ARGO_ERROR(ERR_INPUT, 3003)
#define E_INPUT_TOO_LARGE   ARGO_ERROR(ERR_INPUT, 3004)
#define E_INPUT_INVALID     ARGO_ERROR(ERR_INPUT, 3005)
#define E_INVALID_PARAMS    ARGO_ERROR(ERR_INPUT, 3006)
#define E_INVALID_STATE     ARGO_ERROR(ERR_INPUT, 3007)
#define E_NOT_FOUND         ARGO_ERROR(ERR_INPUT, 3008)
#define E_DUPLICATE         ARGO_ERROR(ERR_INPUT, 3009)
#define E_RESOURCE_LIMIT    ARGO_ERROR(ERR_INPUT, 3010)

/* Protocol errors (PROTOCOL:4xxx) */
#define E_PROTOCOL_FORMAT   ARGO_ERROR(ERR_PROTOCOL, 4001)
#define E_PROTOCOL_SIZE     ARGO_ERROR(ERR_PROTOCOL, 4002)
#define E_PROTOCOL_SESSION  ARGO_ERROR(ERR_PROTOCOL, 4003)
#define E_PROTOCOL_QUEUE    ARGO_ERROR(ERR_PROTOCOL, 4004)
#define E_PROTOCOL_VERSION  ARGO_ERROR(ERR_PROTOCOL, 4005)
#define E_PROTOCOL_HTTP     ARGO_ERROR(ERR_PROTOCOL, 4006)

/* HTTP-specific errors (PROTOCOL:40xx) */
#define E_HTTP_BAD_REQUEST  ARGO_ERROR(ERR_PROTOCOL, 4007)  /* 400 */
#define E_HTTP_UNAUTHORIZED ARGO_ERROR(ERR_PROTOCOL, 4008)  /* 401 */
#define E_HTTP_FORBIDDEN    ARGO_ERROR(ERR_PROTOCOL, 4009)  /* 403 */
#define E_HTTP_NOT_FOUND    ARGO_ERROR(ERR_PROTOCOL, 4010)  /* 404 */
#define E_HTTP_RATE_LIMIT   ARGO_ERROR(ERR_PROTOCOL, 4011)  /* 429 */
#define E_HTTP_SERVER_ERROR ARGO_ERROR(ERR_PROTOCOL, 4012)  /* 500+ */

/* Internal errors (INTERNAL:5xxx) */
#define E_INTERNAL_ASSERT   ARGO_ERROR(ERR_INTERNAL, 5001)
#define E_INTERNAL_LOGIC    ARGO_ERROR(ERR_INTERNAL, 5002)
#define E_INTERNAL_CORRUPT  ARGO_ERROR(ERR_INTERNAL, 5003)
#define E_INTERNAL_NOTIMPL  ARGO_ERROR(ERR_INTERNAL, 5004)

/* Error detail structure */
typedef struct argo_error_detail {
    int code;                   /* Error code */
    const char* name;          /* Error name string */
    const char* message;       /* Human-readable message */
    const char* suggestion;    /* How to fix it */
    const char* ci_hint;       /* What to tell CI agents */
} argo_error_detail_t;

/* Error string functions */
const char* argo_error_string(int code);
const char* argo_error_name(int code);
const char* argo_error_message(int code);
const char* argo_error_suggestion(int code);
const char* argo_error_ci_hint(int code);

/* Error formatting */
int argo_error_format(char* buffer, size_t size, int code);
void argo_error_print(int code, const char* context);

/* Standard error reporting - routes to stderr/log based on severity */
void argo_report_error(int code, const char* context, const char* fmt, ...);

/* Defensive coding macros */
#define ARGO_CHECK_NULL(ptr) \
    do { if (!(ptr)) return E_INPUT_NULL; } while(0)

#define ARGO_CHECK_RANGE(val, min, max) \
    do { if ((val) < (min) || (val) > (max)) return E_INPUT_RANGE; } while(0)

#define ARGO_CHECK_SIZE(size, max) \
    do { if ((size) > (max)) return E_INPUT_TOO_LARGE; } while(0)

#define ARGO_CHECK_RESULT(call) \
    do { int _r = (call); if (_r != ARGO_SUCCESS) return _r; } while(0)

#define ARGO_ASSERT(cond) \
    do { if (!(cond)) { \
        argo_error_print(E_INTERNAL_ASSERT, #cond); \
        return E_INTERNAL_ASSERT; \
    }} while(0)

#define ARGO_VALIDATE_INPUT(ptr, size, max) \
    do { \
        ARGO_CHECK_NULL(ptr); \
        ARGO_CHECK_SIZE(size, max); \
    } while(0)

/* Error type strings for formatting */
static inline const char* argo_error_type_string(int type) {
    switch(type) {
        case ERR_SYSTEM:   return "SYSTEM";
        case ERR_CI:       return "CI";
        case ERR_INPUT:    return "INPUT";
        case ERR_PROTOCOL: return "PROTOCOL";
        case ERR_INTERNAL: return "INTERNAL";
        default:           return "UNKNOWN";
    }
}

#endif /* ARGO_ERROR_H */
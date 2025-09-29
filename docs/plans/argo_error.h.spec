© 2025 Casey Koons All rights reserved

# argo_error.h Specification

This document defines the error handling approach for Argo, following defensive coding principles.

## Philosophy

1. **Defensive Coding**: Check inputs, anticipate failures, prevent crashes
2. **Clear Messages**: Each error includes what went wrong and how to fix it
3. **CI Assistance**: Errors include hints for CI agents
4. **Deterministic**: Same error conditions always produce same error codes

## Header Structure

```c
/* include/argo_error.h */
/* © 2025 Casey Koons All rights reserved */
#ifndef ARGO_ERROR_H
#define ARGO_ERROR_H

/* Success */
#define ARGO_SUCCESS 0

/* Error Categories */
#define ARGO_ERROR_SYSTEM    1000  /* System/OS errors */
#define ARGO_ERROR_CI        2000  /* CI-related errors */
#define ARGO_ERROR_PROTOCOL  3000  /* Protocol violations */
#define ARGO_ERROR_USER      4000  /* User input errors */
#define ARGO_ERROR_INTERNAL  5000  /* Internal logic errors */

/* System Errors (1000-1999) */
#define ARGO_E_MEMORY        1001  /* Memory allocation failed */
#define ARGO_E_SOCKET        1002  /* Socket operation failed */
#define ARGO_E_FILE          1003  /* File operation failed */
#define ARGO_E_FORK          1004  /* Process creation failed */
#define ARGO_E_PERMISSION    1005  /* Permission denied */

/* CI Errors (2000-2999) */
#define ARGO_E_CI_TIMEOUT    2001  /* CI failed to respond */
#define ARGO_E_CI_CONFUSED   2002  /* CI needs clarification */
#define ARGO_E_CI_SCOPE      2003  /* CI exceeding task bounds */
#define ARGO_E_CI_INVALID    2004  /* CI response unparseable */
#define ARGO_E_CI_CONFLICT   2005  /* CIs cannot agree */
#define ARGO_E_CI_OVERLOAD   2006  /* Too many CI requests */

/* Protocol Errors (3000-3999) */
#define ARGO_E_MSG_FORMAT    3001  /* Invalid message format */
#define ARGO_E_MSG_SIZE      3002  /* Message too large */
#define ARGO_E_SESSION_FULL  3003  /* Session limit reached */
#define ARGO_E_SESSION_NOT   3004  /* Session doesn't exist */
#define ARGO_E_QUEUE_FULL    3005  /* Message queue full */

/* User Errors (4000-4999) */
#define ARGO_E_BAD_INPUT     4001  /* Invalid user input */
#define ARGO_E_NO_CONFIG     4002  /* Configuration missing */
#define ARGO_E_BAD_CONFIG    4003  /* Configuration invalid */
#define ARGO_E_NO_PROVIDER   4004  /* CI provider not set */

/* Internal Errors (5000-5999) */
#define ARGO_E_ASSERT        5001  /* Assertion failed */
#define ARGO_E_LOGIC         5002  /* Logic error detected */
#define ARGO_E_CORRUPT       5003  /* Data corruption detected */
#define ARGO_E_UNIMPLEMENTED 5004  /* Feature not implemented */

/* Error Details Structure */
typedef struct argo_error_detail {
    int code;                    /* Error code */
    const char* name;           /* Error name (e.g., "ARGO_E_CI_TIMEOUT") */
    const char* message;        /* Human-readable message */
    const char* suggestion;     /* How to fix it */
    const char* ci_hint;        /* What to tell CI agents */
} argo_error_detail_t;

/* Error Table (in argo_error.c) */
extern const argo_error_detail_t argo_errors[];
extern const int argo_error_count;

/* Helper Functions */
const char* argo_error_name(int code);
const char* argo_error_message(int code);
const char* argo_error_suggestion(int code);
const char* argo_error_ci_hint(int code);
void argo_error_print(int code, const char* context);

/* Defensive Coding Macros */
#define ARGO_CHECK_NULL(ptr) \
    do { if (!(ptr)) return ARGO_E_BAD_INPUT; } while(0)

#define ARGO_CHECK_RANGE(val, min, max) \
    do { if ((val) < (min) || (val) > (max)) return ARGO_E_BAD_INPUT; } while(0)

#define ARGO_CHECK_RESULT(call) \
    do { int _r = (call); if (_r != ARGO_SUCCESS) return _r; } while(0)

#define ARGO_ASSERT(cond) \
    do { if (!(cond)) { \
        argo_error_print(ARGO_E_ASSERT, #cond); \
        return ARGO_E_ASSERT; \
    }} while(0)

#endif /* ARGO_ERROR_H */
```

## Implementation Example

```c
/* src/argo_error.c */
/* © 2025 Casey Koons All rights reserved */
#include "argo_error.h"

const argo_error_detail_t argo_errors[] = {
    {ARGO_SUCCESS, "SUCCESS", "Operation successful", NULL, NULL},

    /* System Errors */
    {ARGO_E_MEMORY, "ARGO_E_MEMORY",
     "Memory allocation failed",
     "Reduce session count or increase system memory",
     "System resource limitation - simplify request"},

    /* CI Errors */
    {ARGO_E_CI_TIMEOUT, "ARGO_E_CI_TIMEOUT",
     "CI failed to respond within timeout",
     "Check CI provider status or increase timeout",
     "Previous request may have been too complex - try simpler query"},

    {ARGO_E_CI_CONFUSED, "ARGO_E_CI_CONFUSED",
     "CI requesting clarification",
     "Break down task into smaller steps",
     "Task needs clarification - ask for specific guidance"},

    /* ... more entries ... */
};

const int argo_error_count = sizeof(argo_errors) / sizeof(argo_errors[0]);

const char* argo_error_message(int code) {
    for (int i = 0; i < argo_error_count; i++) {
        if (argo_errors[i].code == code) {
            return argo_errors[i].message;
        }
    }
    return "Unknown error";
}
```

## Usage Examples

```c
/* Defensive input checking */
int argo_send_message(argo_context_t* ctx, const char* msg, size_t len) {
    ARGO_CHECK_NULL(ctx);
    ARGO_CHECK_NULL(msg);
    ARGO_CHECK_RANGE(len, 1, MAX_MESSAGE_SIZE);

    if (ctx->session_count >= MAX_SESSIONS) {
        return ARGO_E_SESSION_FULL;
    }

    /* ... rest of function ... */
}

/* Error handling with CI hint */
int result = argo_ci_query(ctx, prompt, &response);
if (result != ARGO_SUCCESS) {
    const char* hint = argo_error_ci_hint(result);
    if (hint) {
        /* Tell CI what went wrong */
        ci_inform(ctx->ci, hint);
    }
    return result;
}
```

## Guidelines for Error Usage

1. **Always check inputs** at function entry
2. **Return immediately** on error (fail fast)
3. **Provide context** when printing errors
4. **Use appropriate category** for new errors
5. **Include CI hints** for CI-visible errors
6. **Test error paths** as thoroughly as success paths

## Future Considerations

- Error logging to file
- Error statistics/metrics
- Recovery strategies per error type
- Internationalization of error messages

---

*"The best error is one that never happens, the second best is one that explains itself."*
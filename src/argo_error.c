/* © 2025 Casey Koons All rights reserved */

/* Error string and formatting functions */

/* System includes */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Project includes */
#include "argo_error.h"
#include "argo_log.h"
#include "argo_limits.h"

/* Error descriptions - human readable messages */
static const char* get_error_description(int code) {
    switch (code) {
        /* System errors */
        case E_SYSTEM_MEMORY:     return "Out of memory";
        case E_SYSTEM_SOCKET:     return "Socket operation failed";
        case E_SYSTEM_FILE:       return "File operation failed";
        case E_SYSTEM_FORK:       return "Process fork failed";
        case E_SYSTEM_PERMISSION: return "Permission denied";
        case E_SYSTEM_TIMEOUT:    return "Operation timed out";
        case E_SYSTEM_SSL:        return "SSL/TLS error";
        case E_SYSTEM_NETWORK:    return "Network error";
        case E_SYSTEM_PROCESS:    return "Process operation failed";

        /* CI errors */
        case E_CI_TIMEOUT:        return "CI response timeout";
        case E_CI_CONFUSED:       return "CI needs clarification";
        case E_CI_SCOPE_CREEP:    return "CI exceeding task scope";
        case E_CI_INVALID:        return "CI response invalid";
        case E_CI_CONFLICT:       return "CI merge conflict";
        case E_CI_OVERLOAD:       return "CI overloaded";
        case E_CI_DISCONNECTED:   return "CI disconnected";
        case E_CI_NO_PROVIDER:    return "No CI provider available";

        /* Input errors */
        case E_INPUT_NULL:        return "Null pointer provided";
        case E_INPUT_RANGE:       return "Value out of range";
        case E_INPUT_FORMAT:      return "Invalid format";
        case E_INPUT_TOO_LARGE:   return "Input too large";
        case E_INPUT_INVALID:     return "Invalid input";

        /* Protocol errors */
        case E_PROTOCOL_FORMAT:   return "Invalid response format";
        case E_PROTOCOL_SIZE:     return "Message too large";
        case E_PROTOCOL_SESSION:  return "Invalid session";
        case E_PROTOCOL_QUEUE:    return "Queue full";
        case E_PROTOCOL_VERSION:  return "Protocol version mismatch";
        case E_PROTOCOL_HTTP:     return "HTTP request failed";

        /* Internal errors */
        case E_INTERNAL_ASSERT:   return "Assertion failed";
        case E_INTERNAL_LOGIC:    return "Internal logic error";
        case E_INTERNAL_CORRUPT:  return "Data corruption detected";
        case E_INTERNAL_NOTIMPL:  return "Not implemented";

        default:                  return "Unknown error";
    }
}

/* Format error as human-readable string */
const char* argo_error_string(int code) {
    static char buffer[ARGO_BUFFER_MEDIUM];

    if (code == ARGO_SUCCESS) {
        return "Success";
    }

    int type = ARGO_ERROR_TYPE(code);
    int num = ARGO_ERROR_NUM(code);
    const char* desc = get_error_description(code);
    const char* type_str = argo_error_type_string(type);

    snprintf(buffer, sizeof(buffer), "%s (%s:%d)",
             desc, type_str, num);

    return buffer;
}

/* Get just the error name (short form) */
const char* argo_error_name(int code) {
    switch (code) {
        case ARGO_SUCCESS:        return "SUCCESS";
        case E_SYSTEM_MEMORY:     return "E_SYSTEM_MEMORY";
        case E_SYSTEM_SOCKET:     return "E_SYSTEM_SOCKET";
        case E_SYSTEM_FILE:       return "E_SYSTEM_FILE";
        case E_SYSTEM_FORK:       return "E_SYSTEM_FORK";
        case E_SYSTEM_PERMISSION: return "E_SYSTEM_PERMISSION";
        case E_SYSTEM_TIMEOUT:    return "E_SYSTEM_TIMEOUT";
        case E_SYSTEM_SSL:        return "E_SYSTEM_SSL";
        case E_SYSTEM_NETWORK:    return "E_SYSTEM_NETWORK";
        case E_SYSTEM_PROCESS:    return "E_SYSTEM_PROCESS";
        case E_CI_TIMEOUT:        return "E_CI_TIMEOUT";
        case E_CI_CONFUSED:       return "E_CI_CONFUSED";
        case E_CI_SCOPE_CREEP:    return "E_CI_SCOPE_CREEP";
        case E_CI_INVALID:        return "E_CI_INVALID";
        case E_CI_CONFLICT:       return "E_CI_CONFLICT";
        case E_CI_OVERLOAD:       return "E_CI_OVERLOAD";
        case E_CI_DISCONNECTED:   return "E_CI_DISCONNECTED";
        case E_CI_NO_PROVIDER:    return "E_CI_NO_PROVIDER";
        case E_INPUT_NULL:        return "E_INPUT_NULL";
        case E_INPUT_RANGE:       return "E_INPUT_RANGE";
        case E_INPUT_FORMAT:      return "E_INPUT_FORMAT";
        case E_INPUT_TOO_LARGE:   return "E_INPUT_TOO_LARGE";
        case E_INPUT_INVALID:     return "E_INPUT_INVALID";
        case E_PROTOCOL_FORMAT:   return "E_PROTOCOL_FORMAT";
        case E_PROTOCOL_SIZE:     return "E_PROTOCOL_SIZE";
        case E_PROTOCOL_SESSION:  return "E_PROTOCOL_SESSION";
        case E_PROTOCOL_QUEUE:    return "E_PROTOCOL_QUEUE";
        case E_PROTOCOL_VERSION:  return "E_PROTOCOL_VERSION";
        case E_PROTOCOL_HTTP:     return "E_PROTOCOL_HTTP";
        case E_INTERNAL_ASSERT:   return "E_INTERNAL_ASSERT";
        case E_INTERNAL_LOGIC:    return "E_INTERNAL_LOGIC";
        case E_INTERNAL_CORRUPT:  return "E_INTERNAL_CORRUPT";
        case E_INTERNAL_NOTIMPL:  return "E_INTERNAL_NOTIMPL";
        default:                  return "E_UNKNOWN";
    }
}

/* Get just the human message (no code) */
const char* argo_error_message(int code) {
    return get_error_description(code);
}

/* Get suggestion for fixing the error */
const char* argo_error_suggestion(int code) {
    switch (code) {
        case E_SYSTEM_MEMORY:     return "Reduce memory usage or increase available memory";
        case E_SYSTEM_SOCKET:     return "Check network connectivity and port availability";
        case E_SYSTEM_FILE:       return "Verify file permissions and disk space";
        case E_SYSTEM_PERMISSION: return "Run with appropriate permissions";
        case E_SYSTEM_TIMEOUT:    return "Increase timeout or check system responsiveness";
        case E_CI_TIMEOUT:        return "Check CI provider availability and network";
        case E_CI_CONFUSED:       return "Rephrase the request with more context";
        case E_CI_SCOPE_CREEP:    return "Break task into smaller, focused steps";
        case E_CI_INVALID:        return "Check CI response format and retry";
        case E_CI_DISCONNECTED:   return "Reconnect to CI provider";
        case E_CI_NO_PROVIDER:    return "Configure at least one CI provider";
        case E_INPUT_NULL:        return "Provide valid non-null input";
        case E_INPUT_RANGE:       return "Use value within valid range";
        case E_INPUT_TOO_LARGE:   return "Reduce input size";
        case E_PROTOCOL_FORMAT:   return "Check API response format";
        case E_PROTOCOL_HTTP:     return "Check HTTP status and API credentials";
        case E_PROTOCOL_QUEUE:    return "Wait for queue space or reduce load";
        case E_INTERNAL_LOGIC:    return "Report this bug with reproduction steps";
        case E_INTERNAL_NOTIMPL:  return "Feature not yet implemented";
        default:                  return "Consult documentation or logs";
    }
}

/* Get hint for CI agents about this error */
const char* argo_error_ci_hint(int code) {
    switch (code) {
        case E_CI_TIMEOUT:        return "Your response took too long - be more concise";
        case E_CI_CONFUSED:       return "Ask clarifying questions before proceeding";
        case E_CI_SCOPE_CREEP:    return "Focus only on the specific task requested";
        case E_CI_INVALID:        return "Your response format was not understood";
        case E_INPUT_TOO_LARGE:   return "Your output exceeded size limits";
        case E_PROTOCOL_FORMAT:   return "Response format was incorrect";
        default:                  return NULL;
    }
}

/* Format error with full context */
int argo_error_format(char* buffer, size_t size, int code) {
    if (!buffer || size == 0) return -1;

    const char* name = argo_error_name(code);
    const char* message = argo_error_message(code);
    const char* suggestion = argo_error_suggestion(code);
    int type = ARGO_ERROR_TYPE(code);
    int num = ARGO_ERROR_NUM(code);

    return snprintf(buffer, size,
                   "Error: %s\n"
                   "Code: %s:%d\n"
                   "Message: %s\n"
                   "Suggestion: %s\n",
                   name,
                   argo_error_type_string(type), num,
                   message,
                   suggestion);
}

/* Print error with context to stderr */
void argo_error_print(int code, const char* context) {
    fprintf(stderr, "Error");
    if (context) {
        fprintf(stderr, " in %s", context);
    }
    fprintf(stderr, ": %s\n", argo_error_string(code));
}

/* Standard error reporting with routing based on severity */
void argo_report_error(int code, const char* context, const char* fmt, ...) {
    if (code == ARGO_SUCCESS) return;

    int type = ARGO_ERROR_TYPE(code);
    int num = ARGO_ERROR_NUM(code);
    const char* type_str = argo_error_type_string(type);
    const char* message = argo_error_message(code);

    /* Format: [ARGO ERROR] context: message (details) [TYPE:NUM] */
    char error_line[ERROR_LINE_BUFFER_SIZE];
    int pos = 0;

    pos += snprintf(error_line + pos, sizeof(error_line) - pos, "[ARGO ERROR]");

    if (context) {
        pos += snprintf(error_line + pos, sizeof(error_line) - pos, " %s:", context);
    }

    pos += snprintf(error_line + pos, sizeof(error_line) - pos, " %s", message);

    /* Add formatted details if provided */
    if (fmt && fmt[0] != '\0') {
        va_list args;
        va_start(args, fmt);
        char details[ARGO_BUFFER_MEDIUM];
        vsnprintf(details, sizeof(details), fmt, args);
        va_end(args);
        pos += snprintf(error_line + pos, sizeof(error_line) - pos, " (%s)", details);
    }

    snprintf(error_line + pos, sizeof(error_line) - pos, " [%s:%d]", type_str, num);

    /* Route based on severity:
     * INTERNAL/SYSTEM -> stderr + log (critical)
     * CI/INPUT/PROTOCOL -> log only (expected)
     */
    int to_stderr = (type == ERR_INTERNAL || type == ERR_SYSTEM);

    if (to_stderr) {
        fprintf(stderr, "%s\n", error_line);
    }

    /* Always log errors */
    LOG_ERROR("%s", error_line);
}
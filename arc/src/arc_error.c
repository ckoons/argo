/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include "arc_error.h"
#include "argo_error.h"

/* Get user-friendly error message */
const char* arc_error_message(int error_code) {
    switch (error_code) {
        case E_SYSTEM_MEMORY:
            return "Out of memory";
        case E_SYSTEM_FILE:
            return "File system error (check permissions and disk space)";
        case E_INVALID_PARAMS:
            return "Invalid parameters";
        case E_DUPLICATE:
            return "Already exists";
        case E_RESOURCE_LIMIT:
            return "Resource limit exceeded";
        case E_PROTOCOL_FORMAT:
            return "Invalid JSON format";
        default:
            return "Unknown error";
    }
}

/* Report error with context and suggestion */
void arc_report_error(int error_code, const char* context, const char* suggestion) {
    fprintf(stderr, "Error: %s\n", arc_error_message(error_code));

    if (context) {
        fprintf(stderr, "  Context: %s\n", context);
    }

    if (suggestion) {
        fprintf(stderr, "  Suggestion: %s\n", suggestion);
    }
}

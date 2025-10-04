/* © 2025 Casey Koons All rights reserved */

#ifndef ARC_ERROR_H
#define ARC_ERROR_H

/* User-friendly error reporting for arc commands */

/* Report error with context and suggestion */
void arc_report_error(int error_code, const char* context, const char* suggestion);

/* Get user-friendly error message */
const char* arc_error_message(int error_code);

#endif /* ARC_ERROR_H */

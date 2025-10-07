/* © 2025 Casey Koons All rights reserved */

/* Print utility functions - output redirection and customization */

/* System includes */
#include <stdio.h>
#include <stdarg.h>

/* Project includes */
#include "argo_print_utils.h"

/* Default output handlers */
static void default_output_handler(const char* fmt, va_list args) {
    vprintf(fmt, args);
}

static void default_error_handler(const char* fmt, va_list args) {
    vfprintf(stderr, fmt, args);
}

/* Current output handlers */
static argo_output_fn current_output_handler = default_output_handler;
static argo_output_fn current_error_handler = default_error_handler;

/* Set custom output handler for stdout */
void argo_set_output_handler(argo_output_fn fn) {
    current_output_handler = fn ? fn : default_output_handler;
}

/* Set custom output handler for stderr */
void argo_set_error_handler(argo_output_fn fn) {
    current_error_handler = fn ? fn : default_error_handler;
}

/* Print to stdout (or custom handler) */
void argo_printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    current_output_handler(fmt, args);
    va_end(args);
}

/* Print to stderr (or custom handler) */
void argo_fprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    current_error_handler(fmt, args);
    va_end(args);
}

/* Print to specific file stream (no redirection) */
void argo_fprintf_file(FILE* fp, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);
}

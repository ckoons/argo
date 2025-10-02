/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_PRINT_UTILS_H
#define ARGO_PRINT_UTILS_H

#include <stdarg.h>
#include <stdio.h>

/*
 * Argo Print Utilities
 *
 * Wraps printf/fprintf to allow output redirection for testing,
 * logging to files, or sending to sockets. Provides single point
 * of control for all user-facing output.
 *
 * These utilities don't count against the 10,000 line diet budget.
 */

/* ===== Output Function Types ===== */

/**
 * Output function signature.
 * Takes format string and variable arguments like printf.
 *
 * @param fmt Format string (printf-style)
 * @param args Variable argument list
 */
typedef void (*argo_output_fn)(const char* fmt, va_list args);

/* ===== Output Configuration ===== */

/**
 * Set custom output handler for stdout messages.
 * Default is vprintf to stdout.
 *
 * @param fn Custom output function (NULL to restore default)
 */
void argo_set_output_handler(argo_output_fn fn);

/**
 * Set custom output handler for stderr messages.
 * Default is vfprintf to stderr.
 *
 * @param fn Custom output function (NULL to restore default)
 */
void argo_set_error_handler(argo_output_fn fn);

/* ===== Output Functions ===== */

/**
 * Print to stdout (or custom handler).
 * Drop-in replacement for printf.
 *
 * @param fmt Format string
 * @param ... Variable arguments
 */
void argo_printf(const char* fmt, ...);

/**
 * Print to stderr (or custom handler).
 * Drop-in replacement for fprintf(stderr, ...).
 *
 * @param fmt Format string
 * @param ... Variable arguments
 */
void argo_fprintf(const char* fmt, ...);

/**
 * Print to specific file stream.
 * For compatibility with existing fprintf(fp, ...) usage.
 * Does NOT use custom handlers.
 *
 * @param fp File pointer
 * @param fmt Format string
 * @param ... Variable arguments
 */
void argo_fprintf_file(FILE* fp, const char* fmt, ...);

#endif /* ARGO_PRINT_UTILS_H */

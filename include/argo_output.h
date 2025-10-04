/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_OUTPUT_H
#define ARGO_OUTPUT_H

#include <stdio.h>

/*
 * Unified Output System for Argo
 *
 * This header provides a single point of control for ALL output in the system.
 * All user-facing messages, workflow logs, and error output go through these macros.
 *
 * Categories:
 * - LOG_USER_*    : User-facing CLI output (arc commands, UI)
 * - LOG_WORKFLOW  : Workflow execution logs (redirected to files via dup2)
 * - FORK_ERROR    : Critical errors in child processes
 * - LOG_INFO/ERROR: Internal library logging (defined elsewhere)
 */

/* =============================================================================
 * User-Facing CLI Output (arc commands, UI)
 * Goes to stderr for consistency (allows stdout for data/pipeable output)
 * ============================================================================= */

/* Informational messages to user */
#define LOG_USER_INFO(fmt, ...) \
    fprintf(stderr, fmt, ##__VA_ARGS__)

/* Error messages with "Error: " prefix */
#define LOG_USER_ERROR(fmt, ...) \
    fprintf(stderr, "Error: " fmt, ##__VA_ARGS__)

/* Warning messages with "Warning: " prefix */
#define LOG_USER_WARN(fmt, ...) \
    fprintf(stderr, "Warning: " fmt, ##__VA_ARGS__)

/* Success messages (no prefix, positive feedback) */
#define LOG_USER_SUCCESS(fmt, ...) \
    fprintf(stderr, fmt, ##__VA_ARGS__)

/* Status/table output (can go to stdout for piping) */
#define LOG_USER_STATUS(fmt, ...) \
    printf(fmt, ##__VA_ARGS__)

/* =============================================================================
 * Workflow Execution Logs (bin/argo_workflow_executor)
 * Output is redirected to ~/.argo/logs/{workflow_id}.log via dup2()
 * These macros use fprintf directly - redirection happens at fd level
 * ============================================================================= */

/* Workflow log to stdout (will be redirected to log file) */
#define LOG_WORKFLOW(fmt, ...) \
    fprintf(stdout, fmt, ##__VA_ARGS__)

/* Workflow error to stderr (will be redirected to log file) */
#define LOG_WORKFLOW_ERROR(fmt, ...) \
    fprintf(stderr, fmt, ##__VA_ARGS__)

/* =============================================================================
 * Fork/Exec Error Path (critical errors in child processes)
 * Used when child process can't complete setup (before/after execv)
 * Goes to stderr which may or may not be redirected depending on when error occurs
 * ============================================================================= */

#define FORK_ERROR(fmt, ...) \
    fprintf(stderr, "Fork error: " fmt, ##__VA_ARGS__)

#endif /* ARGO_OUTPUT_H */

/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_CLAUDE_PROCESS_H
#define ARGO_CLAUDE_PROCESS_H

#include "argo_claude_internal.h"

/* Process management functions for Claude subprocess
 *
 * These functions handle spawning, communication with, and cleanup
 * of the Claude CLI subprocess using fork/exec and pipes.
 */

/* Spawn Claude subprocess with pipes for stdin/stdout/stderr
 *
 * Parameters:
 *   ctx - Claude context with pipe arrays to populate
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_SYSTEM_FORK if fork or pipe creation fails
 */
int spawn_claude_process(claude_context_t* ctx);

/* Kill Claude subprocess and close pipes
 *
 * Parameters:
 *   ctx - Claude context with process to terminate
 *
 * Returns:
 *   ARGO_SUCCESS (always)
 */
int kill_claude_process(claude_context_t* ctx);

/* Write input to Claude's stdin
 *
 * Parameters:
 *   ctx - Claude context
 *   input - String to write to Claude
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_SYSTEM_SOCKET if write fails
 */
int write_to_claude(claude_context_t* ctx, const char* input);

/* Read response from Claude's stdout
 *
 * Parameters:
 *   ctx - Claude context
 *   output - Pointer to receive response string (points to ctx->response_buffer)
 *   timeout_ms - Timeout in milliseconds
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_CI_TIMEOUT if timeout expires
 */
int read_from_claude(claude_context_t* ctx, char** output, int timeout_ms);

#endif /* ARGO_CLAUDE_PROCESS_H */

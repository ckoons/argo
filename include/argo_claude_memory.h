/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_CLAUDE_MEMORY_H
#define ARGO_CLAUDE_MEMORY_H

#include "argo_claude_internal.h"

/* Working memory management for Claude sessions
 *
 * These functions handle memory-mapped persistent storage for
 * Claude session state, including sunset/sunrise notes and
 * Apollo digests.
 */

/* Setup memory-mapped working memory file
 *
 * Parameters:
 *   ctx - Claude context
 *   ci_name - CI name for session identification
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_SYSTEM_FILE if file operations fail
 *   E_SYSTEM_MEMORY if mmap fails
 */
int setup_working_memory(claude_context_t* ctx, const char* ci_name);

/* Build prompt context with working memory contents
 *
 * Constructs a full prompt by prepending sunset notes and Apollo
 * digest from working memory to the current prompt.
 *
 * Parameters:
 *   ctx - Claude context with working memory
 *   prompt - Current user prompt
 *
 * Returns:
 *   Allocated string with full context (caller must free)
 *   NULL on allocation failure
 */
char* build_context_with_memory(claude_context_t* ctx, const char* prompt);

/* Load existing working memory session
 *
 * Parameters:
 *   ctx - Claude context
 *
 * Returns:
 *   ARGO_SUCCESS if memory is valid
 *   E_INTERNAL_CORRUPT if magic number doesn't match
 */
int load_working_memory(claude_context_t* ctx);

/* Save working memory to disk
 *
 * Parameters:
 *   ctx - Claude context
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_SYSTEM_FILE if msync fails
 */
int save_working_memory(claude_context_t* ctx);

/* Update turn count in working memory
 *
 * Called after each successful query to track conversation turns.
 *
 * Parameters:
 *   ctx - Claude context
 */
void claude_memory_update_turn(claude_context_t* ctx);

/* Cleanup working memory
 *
 * Unmaps memory and closes file descriptor.
 *
 * Parameters:
 *   ctx - Claude context
 */
void cleanup_working_memory(claude_context_t* ctx);

#endif /* ARGO_CLAUDE_MEMORY_H */

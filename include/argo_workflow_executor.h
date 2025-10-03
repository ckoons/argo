/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_WORKFLOW_EXECUTOR_H
#define ARGO_WORKFLOW_EXECUTOR_H

#include "argo_workflow_context.h"

/* Forward declaration */
typedef struct jsmntok jsmntok_t;

/* Workflow executor configuration */
#define EXECUTOR_MAX_STEPS 100
#define EXECUTOR_STEP_NAME_MAX 64

/* Workflow executor buffer sizes */
#define EXECUTOR_TYPE_BUFFER_SIZE 64
#define EXECUTOR_NAME_BUFFER_SIZE 128

/* Workflow executor constants */
#define EXECUTOR_STEP_EXIT "EXIT"
#define EXECUTOR_DEFAULT_WORKFLOW_NAME "unknown"

/* Workflow Executor
 *
 * Executes workflow definitions loaded from JSON files.
 * Processes steps sequentially based on next_step directives.
 *
 * Step Flow:
 *   1. Find step by number or "START"
 *   2. Execute step handler based on type
 *   3. Follow next_step to next step
 *   4. Repeat until next_step is "EXIT"
 *
 * Example workflow:
 *   Step 1 (user_ask) → saves to context → next_step: 2
 *   Step 2 (display)  → reads from context → next_step: EXIT
 */

/* Execute workflow from JSON
 *
 * Loads workflow JSON, executes all steps with context,
 * and cleans up when complete.
 *
 * Parameters:
 *   json - Workflow JSON string
 *   tokens - Parsed JSON tokens
 *   token_count - Number of tokens
 *
 * Returns:
 *   ARGO_SUCCESS on successful completion
 *   E_INPUT_NULL if parameters NULL
 *   E_WORKFLOW_* error codes for execution failures
 */
int workflow_execute(const char* json, jsmntok_t* tokens, int token_count);

/* Execute single workflow step
 *
 * Dispatches to appropriate step handler based on type.
 *
 * Parameters:
 *   json - Full JSON string
 *   tokens - Parsed tokens
 *   step_index - Token index of step object
 *   ctx - Workflow context for variable storage
 *   next_step - Output: name/number of next step
 *   next_step_size - Size of next_step buffer
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_NULL if parameters are NULL
 *   E_WORKFLOW_INVALID_STEP if step type unknown
 */
int workflow_execute_step(const char* json, jsmntok_t* tokens, int step_index,
                         workflow_context_t* ctx,
                         char* next_step, size_t next_step_size);

/* Find step in workflow
 *
 * Searches for step by number or name (e.g., "START").
 *
 * Parameters:
 *   json - Workflow JSON string
 *   tokens - Parsed tokens
 *   step_id - Step number or name to find
 *
 * Returns:
 *   Token index of step object (>= 0) on success
 *   -1 if step not found
 */
int workflow_find_step(const char* json, jsmntok_t* tokens, const char* step_id);

#endif /* ARGO_WORKFLOW_EXECUTOR_H */

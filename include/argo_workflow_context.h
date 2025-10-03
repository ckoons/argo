/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_WORKFLOW_CONTEXT_H
#define ARGO_WORKFLOW_CONTEXT_H

#include <stddef.h>
#include <time.h>

/* Workflow context configuration */
#define WORKFLOW_CONTEXT_INITIAL_CAPACITY 16
#define WORKFLOW_CONTEXT_MAX_KEY_LENGTH 256
#define WORKFLOW_CONTEXT_MAX_VALUE_LENGTH 8192

/* Workflow Context
 *
 * Stores workflow variables (key/value pairs) during execution.
 * Variables are set by steps and can be referenced by subsequent steps.
 *
 * Example:
 *   Step 1: user_ask saves to "name"
 *   Step 2: display shows "Hello {name}"
 *
 * Variables persist for the lifetime of the workflow execution.
 */
typedef struct workflow_context {
    char** keys;           /* Variable names */
    char** values;         /* Variable values */
    int count;             /* Current number of variables */
    int capacity;          /* Allocated capacity */
    time_t created;        /* When context was created */
    time_t updated;        /* Last update time */
} workflow_context_t;

/* Create workflow context
 *
 * Allocates and initializes a new workflow context.
 *
 * Returns:
 *   Pointer to context on success
 *   NULL on allocation failure
 */
workflow_context_t* workflow_context_create(void);

/* Destroy workflow context
 *
 * Frees all memory associated with context.
 * Safe to call with NULL pointer.
 *
 * Parameters:
 *   ctx - Context to destroy
 */
void workflow_context_destroy(workflow_context_t* ctx);

/* Set context variable
 *
 * Sets a variable in the context. If variable exists, updates value.
 * If variable doesn't exist, adds it.
 *
 * Parameters:
 *   ctx - Workflow context
 *   key - Variable name
 *   value - Variable value
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_NULL if ctx, key, or value is NULL
 *   E_SYSTEM_MEMORY if allocation fails
 *   E_INPUT_TOO_LARGE if key or value exceeds max length
 */
int workflow_context_set(workflow_context_t* ctx, const char* key, const char* value);

/* Get context variable
 *
 * Retrieves variable value from context.
 *
 * Parameters:
 *   ctx - Workflow context
 *   key - Variable name
 *
 * Returns:
 *   Variable value on success
 *   NULL if variable not found or ctx/key is NULL
 */
const char* workflow_context_get(workflow_context_t* ctx, const char* key);

/* Substitute variables in string
 *
 * Replaces {variable} references with values from context.
 *
 * Example:
 *   context has: name="Casey", project="Argo"
 *   input: "Hello {name}, welcome to {project}"
 *   output: "Hello Casey, welcome to Argo"
 *
 * Parameters:
 *   ctx - Workflow context
 *   template - String with {variable} placeholders
 *   output - Buffer for result
 *   output_size - Size of output buffer
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_NULL if any parameter is NULL
 *   E_INPUT_TOO_LARGE if result exceeds output_size
 */
int workflow_context_substitute(workflow_context_t* ctx,
                               const char* template,
                               char* output,
                               size_t output_size);

/* Check if variable exists
 *
 * Parameters:
 *   ctx - Workflow context
 *   key - Variable name
 *
 * Returns:
 *   1 if variable exists
 *   0 if variable doesn't exist or ctx/key is NULL
 */
int workflow_context_has(workflow_context_t* ctx, const char* key);

/* Clear all variables
 *
 * Removes all variables from context but keeps context allocated.
 *
 * Parameters:
 *   ctx - Workflow context
 */
void workflow_context_clear(workflow_context_t* ctx);

/* Print context (debugging)
 *
 * Prints all variables to stdout.
 *
 * Parameters:
 *   ctx - Workflow context
 */
void workflow_context_print(workflow_context_t* ctx);

#endif /* ARGO_WORKFLOW_CONTEXT_H */

/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_WORKFLOW_STEPS_H
#define ARGO_WORKFLOW_STEPS_H

#include "argo_workflow_context.h"

/* Forward declaration */
typedef struct jsmntok jsmntok_t;

/* Workflow step type constants */
#define STEP_TYPE_USER_ASK "user_ask"
#define STEP_TYPE_DISPLAY "display"
#define STEP_TYPE_SAVE_FILE "save_file"

/* Step JSON field names */
#define STEP_FIELD_PROMPT "prompt"
#define STEP_FIELD_SAVE_TO "save_to"
#define STEP_FIELD_MESSAGE "message"
#define STEP_FIELD_DESTINATION "destination"
#define STEP_FIELD_DATA "data"

/* Step buffer sizes */
#define STEP_INPUT_BUFFER_SIZE 4096
#define STEP_OUTPUT_BUFFER_SIZE 8192
#define STEP_PROMPT_BUFFER_SIZE 512
#define STEP_SAVE_TO_BUFFER_SIZE 256
#define STEP_DESTINATION_BUFFER_SIZE 512
#define STEP_TIMESTAMP_BUFFER_SIZE 64

/* Step: user_ask
 *
 * Prompts user for input and saves response to context variable.
 *
 * JSON Format:
 *   {
 *     "type": "user_ask",
 *     "prompt": "What is your name?",
 *     "save_to": "name",
 *     "next_step": 2
 *   }
 *
 * Parameters:
 *   json - Full JSON string
 *   tokens - Parsed tokens
 *   step_index - Token index of step object
 *   ctx - Workflow context to save response
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_NULL if parameters NULL
 *   E_WORKFLOW_MISSING_FIELD if required fields missing
 */
int step_user_ask(const char* json, jsmntok_t* tokens, int step_index,
                  workflow_context_t* ctx);

/* Step: display
 *
 * Shows message to user with variable substitution.
 *
 * JSON Format:
 *   {
 *     "type": "display",
 *     "message": "Hello {name}!",
 *     "next_step": 3
 *   }
 *
 * Parameters:
 *   json - Full JSON string
 *   tokens - Parsed tokens
 *   step_index - Token index of step object
 *   ctx - Workflow context for variable substitution
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_NULL if parameters NULL
 *   E_WORKFLOW_MISSING_FIELD if message missing
 */
int step_display(const char* json, jsmntok_t* tokens, int step_index,
                 workflow_context_t* ctx);

/* Step: save_file
 *
 * Saves JSON data to file with variable substitution.
 *
 * JSON Format:
 *   {
 *     "type": "save_file",
 *     "data": {
 *       "name": "{name}",
 *       "timestamp": "{timestamp}"
 *     },
 *     "destination": "/tmp/output.json",
 *     "next_step": "EXIT"
 *   }
 *
 * Parameters:
 *   json - Full JSON string
 *   tokens - Parsed tokens
 *   step_index - Token index of step object
 *   ctx - Workflow context for variable substitution
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_NULL if parameters NULL
 *   E_WORKFLOW_MISSING_FIELD if required fields missing
 *   E_FILE_WRITE if file write fails
 */
int step_save_file(const char* json, jsmntok_t* tokens, int step_index,
                   workflow_context_t* ctx);

#endif /* ARGO_WORKFLOW_STEPS_H */

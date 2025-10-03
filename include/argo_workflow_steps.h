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
#define STEP_TYPE_DECIDE "decide"
#define STEP_TYPE_USER_CHOOSE "user_choose"

/* Step JSON field names */
#define STEP_FIELD_PROMPT "prompt"
#define STEP_FIELD_SAVE_TO "save_to"
#define STEP_FIELD_MESSAGE "message"
#define STEP_FIELD_DESTINATION "destination"
#define STEP_FIELD_DATA "data"
#define STEP_FIELD_CONDITION "condition"
#define STEP_FIELD_IF_TRUE "if_true"
#define STEP_FIELD_IF_FALSE "if_false"
#define STEP_FIELD_OPTIONS "options"
#define STEP_FIELD_LABEL "label"
#define STEP_FIELD_VALUE "value"

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

/* Step: decide
 *
 * Evaluates a condition and branches to different next steps.
 *
 * JSON Format:
 *   {
 *     "type": "decide",
 *     "condition": "context.user_input.length > 200",
 *     "if_true": 4,
 *     "if_false": 5
 *   }
 *
 * Parameters:
 *   json - Full JSON string
 *   tokens - Parsed tokens
 *   step_index - Token index of step object
 *   ctx - Workflow context for condition evaluation
 *   next_step - Output: next step ID based on condition
 *   next_step_size - Size of next_step buffer
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_NULL if parameters NULL
 *   E_PROTOCOL_FORMAT if required fields missing
 */
int step_decide(const char* json, jsmntok_t* tokens, int step_index,
                workflow_context_t* ctx,
                char* next_step, size_t next_step_size);

/* Step: user_choose
 *
 * Displays multiple choice menu and branches based on selection.
 *
 * JSON Format:
 *   {
 *     "type": "user_choose",
 *     "prompt": "What would you like to do?",
 *     "options": [
 *       {"label": "Option A", "value": "a", "next_step": 10},
 *       {"label": "Option B", "value": "b", "next_step": 20}
 *     ]
 *   }
 *
 * Parameters:
 *   json - Full JSON string
 *   tokens - Parsed tokens
 *   step_index - Token index of step object
 *   ctx - Workflow context
 *   next_step - Output: next step ID based on choice
 *   next_step_size - Size of next_step buffer
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_NULL if parameters NULL
 *   E_PROTOCOL_FORMAT if required fields missing
 */
int step_user_choose(const char* json, jsmntok_t* tokens, int step_index,
                     workflow_context_t* ctx,
                     char* next_step, size_t next_step_size);

#endif /* ARGO_WORKFLOW_STEPS_H */

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
#define STEP_TYPE_CI_ASK "ci_ask"
#define STEP_TYPE_CI_ANALYZE "ci_analyze"
#define STEP_TYPE_CI_ASK_SERIES "ci_ask_series"
#define STEP_TYPE_CI_PRESENT "ci_present"
#define STEP_TYPE_WORKFLOW_CALL "workflow_call"

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

/* CI step field names */
#define STEP_FIELD_PERSONA "persona"
#define STEP_FIELD_PROMPT_TEMPLATE "prompt_template"
#define STEP_FIELD_TASK "task"
#define STEP_FIELD_ANALYZE "analyze"
#define STEP_FIELD_DETERMINE "determine"
#define STEP_FIELD_EXTRACT "extract"
#define STEP_FIELD_IDENTIFY "identify"
#define STEP_FIELD_GENERATE "generate"
#define STEP_FIELD_REVIEW "review"
#define STEP_FIELD_CALCULATE "calculate"
#define STEP_FIELD_QUESTIONS "questions"
#define STEP_FIELD_QUESTIONS_FROM "questions_from"
#define STEP_FIELD_INTRO "intro"
#define STEP_FIELD_FORMAT "format"
#define STEP_FIELD_TEMPLATE "template"
#define STEP_FIELD_SECTIONS "sections"
#define STEP_FIELD_VALIDATION "validation"
#define STEP_FIELD_VALIDATION_ERROR "validation_error"

/* Loop control field names */
#define STEP_FIELD_MAX_ITERATIONS "max_iterations"

/* Workflow call field names */
#define STEP_FIELD_WORKFLOW "workflow"
#define STEP_FIELD_INPUT "input"

/* Workflow recursion limits */
#define WORKFLOW_MAX_RECURSION_DEPTH 10

/* Step buffer sizes */
#define STEP_INPUT_BUFFER_SIZE 4096
#define STEP_OUTPUT_BUFFER_SIZE 8192
#define STEP_PROMPT_BUFFER_SIZE 512
#define STEP_SAVE_TO_BUFFER_SIZE 256
#define STEP_DESTINATION_BUFFER_SIZE 512
#define STEP_TIMESTAMP_BUFFER_SIZE 64
#define STEP_PERSONA_BUFFER_SIZE 64
#define STEP_TASK_BUFFER_SIZE 1024
#define STEP_CI_RESPONSE_BUFFER_SIZE 16384

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

/* Forward declarations */
typedef struct ci_provider ci_provider_t;
typedef struct workflow_controller workflow_controller_t;

/* Step: ci_ask
 *
 * AI asks user a question with persona and context.
 *
 * JSON Format:
 *   {
 *     "type": "ci_ask",
 *     "persona": "maia",
 *     "prompt_template": "What would you like to build?",
 *     "save_to": "context.user_input",
 *     "validation": "^[a-z0-9-]+$",
 *     "validation_error": "Please use lowercase letters, numbers, and hyphens",
 *     "next_step": 2
 *   }
 *
 * Parameters:
 *   provider - CI provider to use for query
 *   json - Full JSON string
 *   tokens - Parsed tokens
 *   step_index - Token index of step object
 *   ctx - Workflow context
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_NULL if parameters NULL
 *   E_PROTOCOL_FORMAT if required fields missing
 */
int step_ci_ask(workflow_controller_t* workflow,
                const char* json, jsmntok_t* tokens, int step_index);

/* Step: ci_analyze
 *
 * AI analyzes data and extracts structured information.
 *
 * JSON Format:
 *   {
 *     "type": "ci_analyze",
 *     "persona": "maia",
 *     "task": "Analyze user input and determine proposal type",
 *     "analyze": "context.user_input",
 *     "determine": ["proposal_type", "domain", "complexity"],
 *     "save_to": "context.analysis",
 *     "next_step": 3
 *   }
 *
 * Parameters:
 *   provider - CI provider to use for query
 *   json - Full JSON string
 *   tokens - Parsed tokens
 *   step_index - Token index of step object
 *   ctx - Workflow context
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_NULL if parameters NULL
 *   E_PROTOCOL_FORMAT if required fields missing
 */
int step_ci_analyze(workflow_controller_t* workflow,
                    const char* json, jsmntok_t* tokens, int step_index);

/* Step: ci_ask_series
 *
 * AI conducts multi-question interview.
 *
 * JSON Format:
 *   {
 *     "type": "ci_ask_series",
 *     "persona": "maia",
 *     "intro": "I have a few questions:",
 *     "questions": [
 *       {"id": "what", "question": "What are you building?", "required": true},
 *       {"id": "why", "question": "Why is this needed?", "required": true}
 *     ],
 *     "save_to": "context.analysis",
 *     "next_step": 5
 *   }
 *
 * Parameters:
 *   workflow - Workflow controller (contains provider, personas, context)
 *   json - Full JSON string
 *   tokens - Parsed tokens
 *   step_index - Token index of step object
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_NULL if parameters NULL
 *   E_PROTOCOL_FORMAT if required fields missing
 */
int step_ci_ask_series(workflow_controller_t* workflow,
                       const char* json, jsmntok_t* tokens, int step_index);

/* Step: ci_present
 *
 * AI formats and presents structured data.
 *
 * JSON Format:
 *   {
 *     "type": "ci_present",
 *     "persona": "maia",
 *     "format": "structured_proposal",
 *     "template": "proposal_template_v1",
 *     "data": "context.analysis",
 *     "sections": ["Overview", "Details", "Summary"],
 *     "next_step": 10
 *   }
 *
 * Parameters:
 *   workflow - Workflow controller (contains provider, personas, context)
 *   json - Full JSON string
 *   tokens - Parsed tokens
 *   step_index - Token index of step object
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_NULL if parameters NULL
 *   E_PROTOCOL_FORMAT if required fields missing
 */
int step_ci_present(workflow_controller_t* workflow,
                    const char* json, jsmntok_t* tokens, int step_index);

/* Step: workflow_call
 *
 * Calls another workflow and passes context data.
 *
 * JSON Format:
 *   {
 *     "type": "workflow_call",
 *     "workflow": "workflows/shared/validation.json",
 *     "input": {
 *       "data": "{context.user_input}",
 *       "rules": "strict"
 *     },
 *     "save_to": "context.result",
 *     "next_step": 5
 *   }
 *
 * Parameters:
 *   workflow - Workflow controller (parent workflow)
 *   json - Full JSON string
 *   tokens - Parsed tokens
 *   step_index - Token index of step object
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_NULL if parameters NULL
 *   E_PROTOCOL_FORMAT if required fields missing
 *   E_INPUT_INVALID if recursion depth exceeded
 */
int step_workflow_call(workflow_controller_t* workflow,
                       const char* json, jsmntok_t* tokens, int step_index);

#endif /* ARGO_WORKFLOW_STEPS_H */

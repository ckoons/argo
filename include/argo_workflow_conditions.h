/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_WORKFLOW_CONDITIONS_H
#define ARGO_WORKFLOW_CONDITIONS_H

#include "argo_workflow_context.h"

/* Condition evaluation buffer sizes */
#define CONDITION_MAX_LENGTH 512
#define CONDITION_OPERAND_MAX 256
#define CONDITION_OPERATOR_MAX 8

/* Condition operators */
typedef enum {
    CONDITION_OP_GREATER,         /* > */
    CONDITION_OP_LESS,            /* < */
    CONDITION_OP_EQUAL,           /* == */
    CONDITION_OP_NOT_EQUAL,       /* != */
    CONDITION_OP_GREATER_EQUAL,   /* >= */
    CONDITION_OP_LESS_EQUAL,      /* <= */
    CONDITION_OP_INVALID
} condition_operator_t;

/* Parsed condition structure */
typedef struct {
    char left_operand[CONDITION_OPERAND_MAX];
    condition_operator_t operator;
    char right_operand[CONDITION_OPERAND_MAX];
} parsed_condition_t;

/* Evaluate condition string
 *
 * Parses and evaluates a condition against workflow context.
 * Supports:
 *   - Context paths: context.field.subfield
 *   - Property access: .length
 *   - Comparisons: >, <, ==, !=, >=, <=
 *   - Numeric literals
 *
 * Example: "context.user_input.length > 200"
 *
 * Parameters:
 *   ctx - Workflow context
 *   condition - Condition string to evaluate
 *   result - Output: 1 if true, 0 if false
 *
 * Returns:
 *   ARGO_SUCCESS on successful evaluation
 *   E_INPUT_NULL if parameters NULL
 *   E_INPUT_INVALID if condition syntax invalid
 */
int workflow_evaluate_condition(workflow_context_t* ctx,
                               const char* condition,
                               int* result);

/* Parse condition string into components
 *
 * Splits condition into left operand, operator, right operand.
 *
 * Parameters:
 *   condition - Condition string to parse
 *   parsed - Output: parsed condition structure
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_INPUT_INVALID if parse fails
 */
int workflow_parse_condition(const char* condition,
                            parsed_condition_t* parsed);

/* Evaluate parsed condition
 *
 * Evaluates a parsed condition against context.
 *
 * Parameters:
 *   ctx - Workflow context
 *   parsed - Parsed condition
 *   result - Output: 1 if true, 0 if false
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_NOT_FOUND if context value not found
 */
int workflow_eval_parsed_condition(workflow_context_t* ctx,
                                   const parsed_condition_t* parsed,
                                   int* result);

/* Get context value from path
 *
 * Resolves context path like "context.field.subfield" or
 * "context.field.length" to a string value.
 *
 * Parameters:
 *   ctx - Workflow context
 *   path - Context path (e.g., "context.user_input")
 *   value - Output buffer for string value
 *   value_size - Size of output buffer
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   E_NOT_FOUND if path doesn't exist
 */
int workflow_get_context_path(workflow_context_t* ctx,
                             const char* path,
                             char* value,
                             size_t value_size);

#endif /* ARGO_WORKFLOW_CONDITIONS_H */

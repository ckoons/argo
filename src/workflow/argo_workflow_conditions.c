/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Project includes */
#include "argo_workflow_conditions.h"
#include "argo_error.h"
#include "argo_log.h"

/* Helper: trim whitespace from string */
static void trim_whitespace(char* str) {
    if (!str) return;

    /* Trim leading whitespace */
    char* start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    /* Trim trailing whitespace */
    char* end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) {
        end--;
    }
    *(end + 1) = '\0';

    /* Move trimmed string to beginning if needed */
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

/* Parse operator from string */
static condition_operator_t parse_operator(const char* op_str) {
    if (strcmp(op_str, ">") == 0) return CONDITION_OP_GREATER;
    if (strcmp(op_str, "<") == 0) return CONDITION_OP_LESS;
    if (strcmp(op_str, "==") == 0) return CONDITION_OP_EQUAL;
    if (strcmp(op_str, "!=") == 0) return CONDITION_OP_NOT_EQUAL;
    if (strcmp(op_str, ">=") == 0) return CONDITION_OP_GREATER_EQUAL;
    if (strcmp(op_str, "<=") == 0) return CONDITION_OP_LESS_EQUAL;
    return CONDITION_OP_INVALID;
}

/* Parse condition string */
int workflow_parse_condition(const char* condition, parsed_condition_t* parsed) {
    if (!condition || !parsed) {
        argo_report_error(E_INPUT_NULL, "workflow_parse_condition", "parameter is NULL");
        return E_INPUT_NULL;
    }

    /* Make a working copy */
    char work[CONDITION_MAX_LENGTH];
    strncpy(work, condition, sizeof(work) - 1);
    work[sizeof(work) - 1] = '\0';

    /* Find operator - try two-char operators first */
    char* op_pos = NULL;
    char op_str[CONDITION_OPERATOR_MAX];

    /* Try >= */
    op_pos = strstr(work, ">=");
    if (op_pos) {
        strncpy(op_str, ">=", sizeof(op_str) - 1);
        goto found_operator;
    }

    /* Try <= */
    op_pos = strstr(work, "<=");
    if (op_pos) {
        strncpy(op_str, "<=", sizeof(op_str) - 1);
        goto found_operator;
    }

    /* Try == */
    op_pos = strstr(work, "==");
    if (op_pos) {
        strncpy(op_str, "==", sizeof(op_str) - 1);
        goto found_operator;
    }

    /* Try != */
    op_pos = strstr(work, "!=");
    if (op_pos) {
        strncpy(op_str, "!=", sizeof(op_str) - 1);
        goto found_operator;
    }

    /* Try single-char operators */
    op_pos = strchr(work, '>');
    if (op_pos) {
        strncpy(op_str, ">", sizeof(op_str) - 1);
        goto found_operator;
    }

    op_pos = strchr(work, '<');
    if (op_pos) {
        strncpy(op_str, "<", sizeof(op_str) - 1);
        goto found_operator;
    }

    /* No operator found */
    argo_report_error(E_INPUT_INVALID, "workflow_parse_condition", "no operator found");
    return E_INPUT_INVALID;

found_operator:
    /* Parse operator */
    parsed->operator = parse_operator(op_str);
    if (parsed->operator == CONDITION_OP_INVALID) {
        argo_report_error(E_INPUT_INVALID, "workflow_parse_condition", "invalid operator");
        return E_INPUT_INVALID;
    }

    /* Extract left operand */
    size_t left_len = op_pos - work;
    if (left_len >= sizeof(parsed->left_operand)) {
        argo_report_error(E_INPUT_TOO_LARGE, "workflow_parse_condition", "left operand too long");
        return E_INPUT_TOO_LARGE;
    }
    strncpy(parsed->left_operand, work, left_len);
    parsed->left_operand[left_len] = '\0';
    trim_whitespace(parsed->left_operand);

    /* Extract right operand */
    const char* right_start = op_pos + strlen(op_str);
    strncpy(parsed->right_operand, right_start, sizeof(parsed->right_operand) - 1);
    parsed->right_operand[sizeof(parsed->right_operand) - 1] = '\0';
    trim_whitespace(parsed->right_operand);

    LOG_DEBUG("Parsed condition: '%s' %d '%s'",
             parsed->left_operand, parsed->operator, parsed->right_operand);

    return ARGO_SUCCESS;
}

/* Get context value from path */
int workflow_get_context_path(workflow_context_t* ctx,
                             const char* path,
                             char* value,
                             size_t value_size) {
    if (!ctx || !path || !value) {
        argo_report_error(E_INPUT_NULL, "workflow_get_context_path", "parameter is NULL");
        return E_INPUT_NULL;
    }

    /* Check if path starts with "context." */
    const char* context_prefix = "context.";
    size_t prefix_len = strlen(context_prefix);

    if (strncmp(path, context_prefix, prefix_len) != 0) {
        argo_report_error(E_INPUT_INVALID, "workflow_get_context_path", "path must start with 'context.'");
        return E_INPUT_INVALID;
    }

    /* Get the field name after "context." */
    const char* field_path = path + prefix_len;

    /* Check for .length property */
    const char* length_suffix = ".length";
    char* length_pos = strstr(field_path, length_suffix);

    if (length_pos) {
        /* Extract field name without .length */
        char field_name[CONDITION_OPERAND_MAX];
        size_t field_len = length_pos - field_path;
        if (field_len >= sizeof(field_name)) {
            argo_report_error(E_INPUT_TOO_LARGE, "workflow_get_context_path", "field name too long");
            return E_INPUT_TOO_LARGE;
        }
        strncpy(field_name, field_path, field_len);
        field_name[field_len] = '\0';

        /* Get the value from context */
        const char* ctx_value = workflow_context_get(ctx, field_name);
        if (!ctx_value) {
            argo_report_error(E_NOT_FOUND, "workflow_get_context_path", field_name);
            return E_NOT_FOUND;
        }

        /* Return the length as a string */
        snprintf(value, value_size, "%zu", strlen(ctx_value));
        return ARGO_SUCCESS;
    }

    /* Simple field lookup (no .length) */
    const char* ctx_value = workflow_context_get(ctx, field_path);
    if (!ctx_value) {
        argo_report_error(E_NOT_FOUND, "workflow_get_context_path", field_path);
        return E_NOT_FOUND;
    }

    strncpy(value, ctx_value, value_size - 1);
    value[value_size - 1] = '\0';
    return ARGO_SUCCESS;
}

/* Evaluate parsed condition */
int workflow_eval_parsed_condition(workflow_context_t* ctx,
                                   const parsed_condition_t* parsed,
                                   int* result) {
    if (!ctx || !parsed || !result) {
        argo_report_error(E_INPUT_NULL, "workflow_eval_parsed_condition", "parameter is NULL");
        return E_INPUT_NULL;
    }

    /* Get left operand value from context */
    char left_value_str[CONDITION_OPERAND_MAX];
    int ret = workflow_get_context_path(ctx, parsed->left_operand,
                                        left_value_str, sizeof(left_value_str));
    if (ret != ARGO_SUCCESS) {
        return ret;
    }

    /* Convert both operands to numbers */
    char* endptr;
    long left_value = strtol(left_value_str, &endptr, 10);
    if (*endptr != '\0') {
        argo_report_error(E_INPUT_INVALID, "workflow_eval_parsed_condition",
                         "left operand not numeric");
        return E_INPUT_INVALID;
    }

    long right_value = strtol(parsed->right_operand, &endptr, 10);
    if (*endptr != '\0') {
        argo_report_error(E_INPUT_INVALID, "workflow_eval_parsed_condition",
                         "right operand not numeric");
        return E_INPUT_INVALID;
    }

    /* Perform comparison */
    int comparison_result = 0;
    switch (parsed->operator) {
        case CONDITION_OP_GREATER:
            comparison_result = (left_value > right_value);
            break;
        case CONDITION_OP_LESS:
            comparison_result = (left_value < right_value);
            break;
        case CONDITION_OP_EQUAL:
            comparison_result = (left_value == right_value);
            break;
        case CONDITION_OP_NOT_EQUAL:
            comparison_result = (left_value != right_value);
            break;
        case CONDITION_OP_GREATER_EQUAL:
            comparison_result = (left_value >= right_value);
            break;
        case CONDITION_OP_LESS_EQUAL:
            comparison_result = (left_value <= right_value);
            break;
        default:
            argo_report_error(E_INTERNAL_LOGIC, "workflow_eval_parsed_condition",
                             "invalid operator");
            return E_INTERNAL_LOGIC;
    }

    *result = comparison_result;
    LOG_DEBUG("Condition evaluated: %ld vs %ld = %d", left_value, right_value, comparison_result);
    return ARGO_SUCCESS;
}

/* Evaluate condition string */
int workflow_evaluate_condition(workflow_context_t* ctx,
                               const char* condition,
                               int* result) {
    if (!ctx || !condition || !result) {
        argo_report_error(E_INPUT_NULL, "workflow_evaluate_condition", "parameter is NULL");
        return E_INPUT_NULL;
    }

    /* Parse condition */
    parsed_condition_t parsed;
    int ret = workflow_parse_condition(condition, &parsed);
    if (ret != ARGO_SUCCESS) {
        return ret;
    }

    /* Evaluate parsed condition */
    return workflow_eval_parsed_condition(ctx, &parsed, result);
}

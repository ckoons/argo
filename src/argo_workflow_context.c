/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Project includes */
#include "argo_workflow_context.h"
#include "argo_error.h"
#include "argo_log.h"

/* Create workflow context */
workflow_context_t* workflow_context_create(void) {
    workflow_context_t* ctx = calloc(1, sizeof(workflow_context_t));
    if (!ctx) {
        argo_report_error(E_SYSTEM_MEMORY, "workflow_context_create", "context allocation");
        return NULL;
    }

    ctx->capacity = WORKFLOW_CONTEXT_INITIAL_CAPACITY;
    ctx->keys = calloc(ctx->capacity, sizeof(char*));
    ctx->values = calloc(ctx->capacity, sizeof(char*));

    if (!ctx->keys || !ctx->values) {
        free(ctx->keys);
        free(ctx->values);
        free(ctx);
        argo_report_error(E_SYSTEM_MEMORY, "workflow_context_create", "arrays allocation");
        return NULL;
    }

    ctx->count = 0;
    ctx->created = time(NULL);
    ctx->updated = ctx->created;

    LOG_DEBUG("Created workflow context (capacity: %d)", ctx->capacity);
    return ctx;
}

/* Destroy workflow context */
void workflow_context_destroy(workflow_context_t* ctx) {
    if (!ctx) return;

    for (int i = 0; i < ctx->count; i++) {
        free(ctx->keys[i]);
        free(ctx->values[i]);
    }

    free(ctx->keys);
    free(ctx->values);
    free(ctx);

    LOG_DEBUG("Destroyed workflow context");
}

/* Ensure capacity for new variable */
static int ensure_capacity(workflow_context_t* ctx) {
    if (ctx->count < ctx->capacity) {
        return ARGO_SUCCESS;
    }

    int new_capacity = ctx->capacity * 2;

    char** new_keys = realloc(ctx->keys, new_capacity * sizeof(char*));
    char** new_values = realloc(ctx->values, new_capacity * sizeof(char*));

    if (!new_keys || !new_values) {
        free(new_keys);
        free(new_values);
        argo_report_error(E_SYSTEM_MEMORY, "ensure_capacity", "realloc failed");
        return E_SYSTEM_MEMORY;
    }

    ctx->keys = new_keys;
    ctx->values = new_values;
    ctx->capacity = new_capacity;

    LOG_DEBUG("Expanded context capacity to %d", new_capacity);
    return ARGO_SUCCESS;
}

/* Find variable index */
static int find_variable(workflow_context_t* ctx, const char* key) {
    for (int i = 0; i < ctx->count; i++) {
        if (strcmp(ctx->keys[i], key) == 0) {
            return i;
        }
    }
    return -1;
}

/* Set context variable */
int workflow_context_set(workflow_context_t* ctx, const char* key, const char* value) {
    if (!ctx || !key || !value) {
        argo_report_error(E_INPUT_NULL, "workflow_context_set", "ctx, key, or value is NULL");
        return E_INPUT_NULL;
    }

    if (strlen(key) >= WORKFLOW_CONTEXT_MAX_KEY_LENGTH) {
        argo_report_error(E_INPUT_TOO_LARGE, "workflow_context_set", "key too long");
        return E_INPUT_TOO_LARGE;
    }

    if (strlen(value) >= WORKFLOW_CONTEXT_MAX_VALUE_LENGTH) {
        argo_report_error(E_INPUT_TOO_LARGE, "workflow_context_set", "value too long");
        return E_INPUT_TOO_LARGE;
    }

    /* Check if variable exists */
    int idx = find_variable(ctx, key);

    if (idx >= 0) {
        /* Update existing variable */
        free(ctx->values[idx]);
        ctx->values[idx] = strdup(value);
        if (!ctx->values[idx]) {
            argo_report_error(E_SYSTEM_MEMORY, "workflow_context_set", "strdup value");
            return E_SYSTEM_MEMORY;
        }
        LOG_DEBUG("Updated context variable: %s", key);
    } else {
        /* Add new variable */
        int result = ensure_capacity(ctx);
        if (result != ARGO_SUCCESS) {
            return result;
        }

        ctx->keys[ctx->count] = strdup(key);
        ctx->values[ctx->count] = strdup(value);

        if (!ctx->keys[ctx->count] || !ctx->values[ctx->count]) {
            free(ctx->keys[ctx->count]);
            free(ctx->values[ctx->count]);
            argo_report_error(E_SYSTEM_MEMORY, "workflow_context_set", "strdup key/value");
            return E_SYSTEM_MEMORY;
        }

        ctx->count++;
        LOG_DEBUG("Added context variable: %s", key);
    }

    ctx->updated = time(NULL);
    return ARGO_SUCCESS;
}

/* Get context variable */
const char* workflow_context_get(workflow_context_t* ctx, const char* key) {
    if (!ctx || !key) {
        return NULL;
    }

    int idx = find_variable(ctx, key);
    if (idx < 0) {
        LOG_DEBUG("Variable not found: %s", key);
        return NULL;
    }

    return ctx->values[idx];
}

/* Substitute variables in string */
int workflow_context_substitute(workflow_context_t* ctx,
                               const char* template,
                               char* output,
                               size_t output_size) {
    if (!ctx || !template || !output) {
        argo_report_error(E_INPUT_NULL, "workflow_context_substitute", "parameter is NULL");
        return E_INPUT_NULL;
    }

    const char* src = template;
    char* dst = output;
    size_t remaining = output_size - 1;

    while (*src && remaining > 0) {
        if (*src == '{') {
            /* Find end of variable name */
            const char* end = strchr(src + 1, '}');
            if (!end) {
                /* No closing brace, copy literal */
                *dst++ = *src++;
                remaining--;
                continue;
            }

            /* Extract variable name */
            size_t var_len = end - src - 1;
            char var_name[WORKFLOW_CONTEXT_MAX_KEY_LENGTH];

            if (var_len >= sizeof(var_name)) {
                argo_report_error(E_INPUT_TOO_LARGE, "workflow_context_substitute", "variable name too long");
                return E_INPUT_TOO_LARGE;
            }

            strncpy(var_name, src + 1, var_len);
            var_name[var_len] = '\0';

            /* Get variable value */
            const char* value = workflow_context_get(ctx, var_name);

            if (value) {
                /* Substitute value */
                size_t value_len = strlen(value);
                if (value_len > remaining) {
                    argo_report_error(E_INPUT_TOO_LARGE, "workflow_context_substitute", "output buffer too small");
                    return E_INPUT_TOO_LARGE;
                }
                memcpy(dst, value, value_len);
                dst += value_len;
                remaining -= value_len;
            } else {
                /* Variable not found, keep placeholder */
                size_t placeholder_len = end - src + 1;
                if (placeholder_len > remaining) {
                    argo_report_error(E_INPUT_TOO_LARGE, "workflow_context_substitute", "output buffer too small");
                    return E_INPUT_TOO_LARGE;
                }
                strncpy(dst, src, placeholder_len);
                dst += placeholder_len;
                remaining -= placeholder_len;
            }

            src = end + 1;
        } else {
            /* Copy literal character */
            *dst++ = *src++;
            remaining--;
        }
    }

    if (*src) {
        /* Output buffer too small */
        argo_report_error(E_INPUT_TOO_LARGE, "workflow_context_substitute", "output buffer too small");
        return E_INPUT_TOO_LARGE;
    }

    *dst = '\0';
    return ARGO_SUCCESS;
}

/* Check if variable exists */
int workflow_context_has(workflow_context_t* ctx, const char* key) {
    if (!ctx || !key) {
        return 0;
    }
    return find_variable(ctx, key) >= 0 ? 1 : 0;
}

/* Clear all variables */
void workflow_context_clear(workflow_context_t* ctx) {
    if (!ctx) return;

    for (int i = 0; i < ctx->count; i++) {
        free(ctx->keys[i]);
        free(ctx->values[i]);
    }

    ctx->count = 0;
    ctx->updated = time(NULL);

    LOG_DEBUG("Cleared workflow context");
}

/* Print context (debugging) */
void workflow_context_print(workflow_context_t* ctx) {
    if (!ctx) {
        printf("Context: NULL\n");
        return;
    }

    printf("Workflow Context (%d variables):\n", ctx->count);
    for (int i = 0; i < ctx->count; i++) {
        printf("  %s = %s\n", ctx->keys[i], ctx->values[i]);
    }
}

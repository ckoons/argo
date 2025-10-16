/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "argo_yaml.h"
#include "argo_error.h"
#include "argo_limits.h"
#include "argo_file_utils.h"

/* Simple YAML parser for key: value pairs
 *
 * Supports:
 * - Single-line key: value pairs
 * - Comments (# prefix)
 * - Empty lines
 * - Leading/trailing whitespace trimming
 *
 * Does NOT support:
 * - Multi-line values
 * - Lists/arrays (except as callback per-item)
 * - Nested objects
 * - Quoted strings with colons
 * - YAML anchors/aliases
 *
 * This is intentionally simple for config files.
 */
int yaml_parse_simple(const char* content, yaml_kv_callback_t callback, void* userdata) {
    if (!content || !callback) {
        argo_report_error(E_INVALID_PARAMS, "yaml_parse_simple", "null parameters");
        return E_INVALID_PARAMS;
    }

    char line[ARGO_BUFFER_STANDARD];
    const char* ptr = content;

    while (*ptr) {
        /* Read line */
        size_t i = 0;
        while (*ptr && *ptr != '\n' && i < sizeof(line) - 1) {
            line[i++] = *ptr++;
        }
        line[i] = '\0';
        if (*ptr == '\n') ptr++;

        /* Skip empty lines and comments */
        char* trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        if (*trimmed == '\0' || *trimmed == '#') continue;

        /* Parse key: value */
        char* colon = strchr(trimmed, ':');
        if (!colon) continue;

        *colon = '\0';
        char* key = trimmed;
        char* value = colon + 1;

        /* Trim key */
        size_t key_len = strlen(key);
        while (key_len > 0 && (key[key_len-1] == ' ' || key[key_len-1] == '\t')) {
            key[--key_len] = '\0';
        }

        /* Trim value and strip inline comments */
        while (*value == ' ' || *value == '\t') value++;

        /* Find and remove inline comment (but not in quoted strings) */
        char* comment = strchr(value, '#');
        if (comment) {
            *comment = '\0';
        }

        size_t val_len = strlen(value);
        while (val_len > 0 && (value[val_len-1] == ' ' || value[val_len-1] == '\t' ||
                               value[val_len-1] == '\r')) {
            value[--val_len] = '\0';
        }

        /* Remove quotes if present */
        if (val_len >= 2 && ((value[0] == '"' && value[val_len-1] == '"') ||
                             (value[0] == '\'' && value[val_len-1] == '\''))) {
            value[val_len-1] = '\0';
            value++;
        }

        /* Call callback with key-value pair */
        callback(key, value, userdata);
    }

    return ARGO_SUCCESS;
}

/* Parse YAML file directly */
int yaml_parse_file(const char* path, yaml_kv_callback_t callback, void* userdata) {
    if (!path || !callback) {
        argo_report_error(E_INVALID_PARAMS, "yaml_parse_file", "null parameters");
        return E_INVALID_PARAMS;
    }

    char* content = NULL;
    size_t size = 0;
    int result = file_read_all(path, &content, &size);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    result = yaml_parse_simple(content, callback, userdata);
    free(content);
    return result;
}

/* Helper structure for single value lookup */
typedef struct {
    const char* search_key;
    char* value_buffer;
    size_t value_size;
    bool found;
} yaml_lookup_ctx_t;

/* Callback for single value lookup */
static void yaml_lookup_callback(const char* key, const char* value, void* userdata) {
    yaml_lookup_ctx_t* ctx = (yaml_lookup_ctx_t*)userdata;
    if (!ctx->found && strcmp(key, ctx->search_key) == 0) {
        strncpy(ctx->value_buffer, value, ctx->value_size - 1);
        ctx->value_buffer[ctx->value_size - 1] = '\0';
        ctx->found = true;
    }
}

/* Utility: Get single value by key from YAML content */
int yaml_get_value(const char* content, const char* key, char* value, size_t value_size) {
    if (!content || !key || !value || value_size == 0) {
        argo_report_error(E_INVALID_PARAMS, "yaml_get_value", "null parameters");
        return E_INVALID_PARAMS;
    }

    yaml_lookup_ctx_t ctx = {
        .search_key = key,
        .value_buffer = value,
        .value_size = value_size,
        .found = false
    };

    int result = yaml_parse_simple(content, yaml_lookup_callback, &ctx);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    return ctx.found ? ARGO_SUCCESS : E_WORKFLOW_NOT_FOUND;
}

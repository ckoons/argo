/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Project includes */
#include "argo_json.h"
#include "argo_error.h"
#include "argo_error_messages.h"
#include "argo_log.h"

/* Extract string field from JSON */
int json_extract_string_field(const char* json, const char* field_name,
                              char** out_value, size_t* out_len) {
    ARGO_CHECK_NULL(json);
    ARGO_CHECK_NULL(field_name);
    ARGO_CHECK_NULL(out_value);
    ARGO_CHECK_NULL(out_len);

    /* Build search string: "field_name" */
    char search_key[JSON_FIELD_NAME_SIZE + 3];
    snprintf(search_key, sizeof(search_key), "\"%s\"", field_name);

    /* Find the field */
    char* field_start = strstr(json, search_key);
    if (!field_start) {
        argo_report_error(E_PROTOCOL_FORMAT, "json_extract_string", ERR_MSG_FIELD_NOT_FOUND);
        return E_PROTOCOL_FORMAT;
    }

    /* Find opening quote after the field name and colon */
    char* content_start = strchr(field_start + strlen(search_key), '"');
    if (!content_start) {
        argo_report_error(E_PROTOCOL_FORMAT, "json_extract_string", ERR_MSG_FIELD_NOT_FOUND);
        return E_PROTOCOL_FORMAT;
    }
    content_start++; /* Move past the quote */

    /* Find closing quote */
    char* content_end = content_start;
    while (*content_end && *content_end != '"') {
        /* Handle escaped quotes */
        if (*content_end == '\\' && *(content_end + 1) == '"') {
            content_end += 2;
            continue;
        }
        content_end++;
    }

    if (*content_end != '"') {
        argo_report_error(E_PROTOCOL_FORMAT, "json_extract_string", ERR_MSG_FIELD_NOT_FOUND);
        return E_PROTOCOL_FORMAT;
    }

    /* Calculate length and allocate */
    size_t len = content_end - content_start;
    char* value = malloc(len + 1);
    if (!value) {
        return E_SYSTEM_MEMORY;
    }

    /* Copy content */
    memcpy(value, content_start, len);
    value[len] = '\0';

    *out_value = value;
    *out_len = len;
    return ARGO_SUCCESS;
}

/* Extract nested string field from JSON */
int json_extract_nested_string(const char* json, const char** field_path,
                               int path_depth, char** out_value, size_t* out_len) {
    ARGO_CHECK_NULL(json);
    ARGO_CHECK_NULL(field_path);
    ARGO_CHECK_NULL(out_value);
    ARGO_CHECK_NULL(out_len);

    if (path_depth < 1 || path_depth > JSON_MAX_FIELD_DEPTH) {
        return E_INPUT_RANGE;
    }

    /* Navigate through the path */
    const char* current_pos = json;
    for (int i = 0; i < path_depth - 1; i++) {
        /* Build search string for this level */
        char search_key[JSON_FIELD_NAME_SIZE + 3];
        snprintf(search_key, sizeof(search_key), "\"%s\"", field_path[i]);

        /* Find the field at this level */
        char* field_pos = strstr(current_pos, search_key);
        if (!field_pos) {
            argo_report_error(E_PROTOCOL_FORMAT, "json_extract_nested_string",
                             ERR_MSG_FIELD_NOT_FOUND);
            return E_PROTOCOL_FORMAT;
        }

        /* Move position forward to continue searching within this object */
        current_pos = field_pos + strlen(search_key);
    }

    /* Extract the final field as a string */
    return json_extract_string_field(current_pos, field_path[path_depth - 1],
                                     out_value, out_len);
}

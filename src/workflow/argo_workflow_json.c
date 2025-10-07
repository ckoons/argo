/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* External library */
#define JSMN_STATIC
#include "jsmn.h"

/* Project includes */
#include "argo_workflow_json.h"
#include "argo_error.h"
#include "argo_log.h"

/* Count tokens in subtree (current token + all children) */
int workflow_json_count_tokens(jsmntok_t* tokens, int index) {
    jsmntok_t* t = &tokens[index];
    int count = 1;  /* Count current token */
    int children = t->size;

    if (t->type == JSMN_OBJECT) {
        /* Object: size = number of key-value pairs */
        index++;
        for (int i = 0; i < children; i++) {
            /* Skip key */
            int key_tokens = workflow_json_count_tokens(tokens, index);
            count += key_tokens;
            index += key_tokens;
            /* Skip value */
            int val_tokens = workflow_json_count_tokens(tokens, index);
            count += val_tokens;
            index += val_tokens;
        }
    } else if (t->type == JSMN_ARRAY) {
        /* Array: size = number of elements */
        index++;
        for (int i = 0; i < children; i++) {
            int elem_tokens = workflow_json_count_tokens(tokens, index);
            count += elem_tokens;
            index += elem_tokens;
        }
    }

    return count;
}

/* Load JSON file into buffer */
char* workflow_json_load_file(const char* path, size_t* out_size) {
    if (!path || !out_size) {
        argo_report_error(E_INPUT_NULL, "workflow_json_load_file", "path or out_size is NULL");
        return NULL;
    }

    FILE* f = fopen(path, "r");
    if (!f) {
        argo_report_error(E_NOT_FOUND, "workflow_json_load_file", path);
        return NULL;
    }

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > WORKFLOW_JSON_MAX_FILE_SIZE) {
        argo_report_error(E_SYSTEM_FILE, "workflow_json_load_file", "invalid file size");
        fclose(f);
        return NULL;
    }

    /* Allocate buffer */
    char* buffer = malloc(size + 1);
    if (!buffer) {
        argo_report_error(E_SYSTEM_MEMORY, "workflow_json_load_file", "buffer allocation");
        fclose(f);
        return NULL;
    }

    /* Read file */
    size_t read = fread(buffer, 1, size, f);
    fclose(f);

    if (read != (size_t)size) {
        argo_report_error(E_SYSTEM_FILE, "workflow_json_load_file", path);
        free(buffer);
        return NULL;
    }

    buffer[size] = '\0';
    *out_size = size;

    LOG_DEBUG("Loaded workflow JSON: %s (%zu bytes)", path, size);
    return buffer;
}

/* Parse JSON string */
int workflow_json_parse(const char* json, jsmntok_t* tokens, int max_tokens) {
    if (!json || !tokens) {
        argo_report_error(E_INPUT_NULL, "workflow_json_parse", "json or tokens is NULL");
        return -1;
    }

    jsmn_parser parser;
    jsmn_init(&parser);

    size_t json_len = strlen(json);
    int token_count = jsmn_parse(&parser, json, json_len, tokens, (unsigned int)max_tokens);
    if (token_count < 0) {
        argo_report_error(E_PROTOCOL_FORMAT, "workflow_json_parse", "JSON parse failed");
        return token_count;
    }

    return token_count;
}

/* Find object field by name */
int workflow_json_find_field(const char* json, jsmntok_t* tokens,
                             int object_index, const char* field_name) {
    if (!json || !tokens || !field_name) {
        return -1;
    }

    jsmntok_t* object = &tokens[object_index];
    if (object->type != JSMN_OBJECT) {
        return -1;
    }

    /* Search object fields */
    int field_count = object->size;
    int current_token = object_index + 1;

    for (int i = 0; i < field_count; i++) {
        jsmntok_t* key = &tokens[current_token];
        if (key->type != JSMN_STRING) {
            return -1;
        }

        /* Check if key matches */
        int key_len = key->end - key->start;
        if ((int)strlen(field_name) == key_len &&
            strncmp(json + key->start, field_name, key_len) == 0) {
            return current_token + 1;  /* Return value token index */
        }

        /* Skip to next field (key + value) */
        current_token++;  /* Skip key */
        int value_tokens = workflow_json_count_tokens(tokens, current_token);
        current_token += value_tokens;  /* Skip value and its children */
    }

    return -1;
}

/* Extract string from token */
int workflow_json_extract_string(const char* json, jsmntok_t* token,
                                 char* buffer, size_t buffer_size) {
    if (!json || !token || !buffer) {
        argo_report_error(E_INPUT_NULL, "workflow_json_extract_string", "parameter is NULL");
        return E_INPUT_NULL;
    }

    int len = token->end - token->start;
    if (len >= (int)buffer_size) {
        argo_report_error(E_INPUT_TOO_LARGE, "workflow_json_extract_string", "value too long");
        return E_INPUT_TOO_LARGE;
    }

    strncpy(buffer, json + token->start, len);
    buffer[len] = '\0';

    return ARGO_SUCCESS;
}

/* Extract integer from token */
int workflow_json_extract_int(const char* json, jsmntok_t* token, int* out_value) {
    if (!json || !token || !out_value) {
        argo_report_error(E_INPUT_NULL, "workflow_json_extract_int", "parameter is NULL");
        return E_INPUT_NULL;
    }

    char buffer[WORKFLOW_JSON_INT_BUFFER_SIZE];
    int len = token->end - token->start;
    if (len >= (int)sizeof(buffer)) {
        argo_report_error(E_INPUT_INVALID, "workflow_json_extract_int", "number too long");
        return E_INPUT_INVALID;
    }

    strncpy(buffer, json + token->start, len);
    buffer[len] = '\0';

    char* endptr;
    long val = strtol(buffer, &endptr, 10);
    if (*endptr != '\0') {
        argo_report_error(E_INPUT_INVALID, "workflow_json_extract_int", "not a valid integer");
        return E_INPUT_INVALID;
    }

    *out_value = (int)val;
    return ARGO_SUCCESS;
}

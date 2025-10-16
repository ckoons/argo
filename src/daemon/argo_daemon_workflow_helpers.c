/* © 2025 Casey Koons All rights reserved */
/* Daemon Workflow Helpers - JSON parsing and ID generation */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <ctype.h>

/* Project includes */
#include "argo_daemon_workflow_helpers.h"
#include "argo_workflow_registry.h"
#include "argo_error.h"
#include "argo_log.h"

/* Parse args array from JSON body */
int parse_args_from_json(const char* json_body, char*** args_out, int* arg_count_out) {
    *args_out = NULL;
    *arg_count_out = 0;

    const char* args_start = strstr(json_body, "\"args\"");
    if (!args_start) {
        return ARGO_SUCCESS;  /* No args field - not an error */
    }

    /* Find array bounds */
    const char* array_start = strchr(args_start, '[');
    const char* array_end = strchr(args_start, ']');

    if (!array_start || !array_end || array_end <= array_start) {
        return ARGO_SUCCESS;  /* Empty or malformed - not fatal */
    }

    /* Count args by counting quotes */
    int arg_count = 0;
    const char* p = array_start;
    while (p < array_end) {
        if (*p == '"') arg_count++;
        p++;
    }
    arg_count /= 2;  /* Each arg has open and close quote */

    if (arg_count == 0) {
        return ARGO_SUCCESS;
    }

    char** args = malloc(sizeof(char*) * arg_count);
    if (!args) {
        return E_SYSTEM_MEMORY;
    }

    /* Extract each arg */
    int idx = 0;
    p = array_start + 1;
    while (p < array_end && idx < arg_count) {
        /* Skip whitespace and commas */
        while (*p == ' ' || *p == ',') p++;

        if (*p == '"') {
            p++;  /* Skip opening quote */
            const char* arg_start = p;
            const char* arg_end = strchr(p, '"');
            if (arg_end) {
                size_t len = arg_end - arg_start;
                args[idx] = malloc(len + 1);
                if (args[idx]) {
                    memcpy(args[idx], arg_start, len);
                    args[idx][len] = '\0';
                    idx++;
                }
                p = arg_end + 1;
            }
        } else {
            break;
        }
    }

    *args_out = args;
    *arg_count_out = idx;  /* Actual number extracted */
    return ARGO_SUCCESS;
}

/* Parse env object from JSON body */
int parse_env_from_json(const char* json_body, char*** env_keys_out,
                        char*** env_values_out, int* env_count_out) {
    *env_keys_out = NULL;
    *env_values_out = NULL;
    *env_count_out = 0;

    const char* env_start = strstr(json_body, "\"env\"");
    if (!env_start) {
        return ARGO_SUCCESS;  /* No env field - not an error */
    }

    /* Find object bounds */
    const char* obj_start = strchr(env_start, '{');
    const char* obj_end = strchr(env_start, '}');

    if (!obj_start || !obj_end || obj_end <= obj_start) {
        return ARGO_SUCCESS;  /* Empty or malformed - not fatal */
    }

    /* Count env vars by counting colons */
    int env_count = 0;
    const char* p = obj_start;
    while (p < obj_end) {
        if (*p == ':') env_count++;
        p++;
    }

    if (env_count == 0) {
        return ARGO_SUCCESS;
    }

    char** env_keys = malloc(sizeof(char*) * env_count);
    char** env_values = malloc(sizeof(char*) * env_count);
    if (!env_keys || !env_values) {
        free(env_keys);
        free(env_values);
        return E_SYSTEM_MEMORY;
    }

    /* Extract each env var (KEY":"VALUE" format) */
    int idx = 0;
    p = obj_start + 1;
    while (p < obj_end && idx < env_count) {
        /* Skip whitespace and commas */
        while (*p == ' ' || *p == ',') p++;

        if (*p == '"') {
            /* Extract key */
            p++;  /* Skip opening quote */
            const char* key_start = p;
            const char* key_end = strchr(p, '"');
            if (key_end) {
                size_t key_len = key_end - key_start;
                env_keys[idx] = malloc(key_len + 1);
                if (env_keys[idx]) {
                    memcpy(env_keys[idx], key_start, key_len);
                    env_keys[idx][key_len] = '\0';
                }
                p = key_end + 1;

                /* Skip to value (past ":") */
                p = strchr(p, ':');
                if (p) {
                    p++;  /* Skip colon */
                    while (*p == ' ') p++;  /* Skip whitespace */
                    if (*p == '"') {
                        p++;  /* Skip opening quote */
                        const char* val_start = p;
                        const char* val_end = strchr(p, '"');
                        if (val_end) {
                            size_t val_len = val_end - val_start;
                            env_values[idx] = malloc(val_len + 1);
                            if (env_values[idx]) {
                                memcpy(env_values[idx], val_start, val_len);
                                env_values[idx][val_len] = '\0';
                                idx++;
                            }
                            p = val_end + 1;
                        }
                    }
                }
            }
        } else {
            break;
        }
    }

    *env_keys_out = env_keys;
    *env_values_out = env_values;
    *env_count_out = idx;  /* Actual number extracted */
    return ARGO_SUCCESS;
}

/* Free allocated args/env arrays */
void free_workflow_params(char** args, int arg_count, char** env_keys,
                          char** env_values, int env_count) {
    for (int i = 0; i < arg_count; i++) {
        free(args[i]);
    }
    free(args);

    for (int i = 0; i < env_count; i++) {
        free(env_keys[i]);
        free(env_values[i]);
    }
    free(env_keys);
    free(env_values);
}

/* Generate workflow instance ID from template and instance suffix */
int generate_workflow_id(workflow_registry_t* registry, const char* template_name,
                        const char* instance_suffix, char* workflow_id,
                        size_t workflow_id_size) {
    if (!template_name || !workflow_id) {
        return E_INPUT_NULL;
    }

    /* If instance suffix provided, use it directly: template_instance */
    if (instance_suffix && strlen(instance_suffix) > 0) {
        snprintf(workflow_id, workflow_id_size, "%s_%s", template_name, instance_suffix);
        return ARGO_SUCCESS;
    }

    /* Auto-generate numeric suffix by checking existing workflows */
    workflow_entry_t* entries = NULL;
    int count = 0;
    int result = workflow_registry_list(registry, &entries, &count);
    if (result != ARGO_SUCCESS) {
        /* If can't list, fall back to timestamp-based ID */
        struct timeval tv;
        gettimeofday(&tv, NULL);
        snprintf(workflow_id, workflow_id_size, "%s_%ld", template_name, (long)tv.tv_usec);
        return ARGO_SUCCESS;
    }

    /* Build search prefix: "template_name_" */
    char prefix[128];
    snprintf(prefix, sizeof(prefix), "%s_", template_name);
    size_t prefix_len = strlen(prefix);

    /* Find highest numeric suffix (format: _NN where NN is 2 digits) */
    int max_num = 0;
    for (int i = 0; i < count; i++) {
        /* Check if workflow_id starts with our prefix */
        if (strncmp(entries[i].workflow_id, prefix, prefix_len) == 0) {
            const char* id_suffix = entries[i].workflow_id + prefix_len;

            /* Check if suffix starts with 2 digits */
            if (strlen(id_suffix) >= 2 && isdigit(id_suffix[0]) && isdigit(id_suffix[1])) {
                /* Extract first 2 digits */
                char num_str[3] = {id_suffix[0], id_suffix[1], '\0'};
                int num = atoi(num_str);
                if (num > max_num) {
                    max_num = num;
                }
            }
        }
    }

    free(entries);

    /* Generate ID with next number */
    snprintf(workflow_id, workflow_id_size, "%s_%02d", template_name, max_num + 1);
    return ARGO_SUCCESS;
}

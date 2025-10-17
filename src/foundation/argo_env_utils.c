/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <limits.h>

/* Project includes */
#include "argo_env_utils.h"
#include "argo_env_internal.h"
#include "argo_error.h"
#include "argo_limits.h"
#include "argo_log.h"

/* Get environment variable */
const char* argo_getenv(const char* name) {
    if (!name) return NULL;

    int lock_result = pthread_mutex_lock(&argo_env_mutex);
    if (lock_result != 0) {
        LOG_ERROR("Failed to acquire mutex in argo_getenv: %d", lock_result);
        return NULL;
    }

    int idx = find_env_index(name);
    const char* result = NULL;

    if (idx >= 0) {
        char* eq = strchr(argo_env[idx], '=');
        if (eq) result = eq + 1;
    }

    pthread_mutex_unlock(&argo_env_mutex);
    return result;
}

/* Set environment variable */
int argo_setenv(const char* name, const char* value) {
    ARGO_CHECK_NULL(name);
    ARGO_CHECK_NULL(value);

    int lock_result = pthread_mutex_lock(&argo_env_mutex);
    if (lock_result != 0) {
        argo_report_error(E_SYSTEM_PROCESS, "argo_setenv", "Failed to acquire mutex");
        return E_SYSTEM_PROCESS;
    }
    int result = set_env_internal(name, value);
    pthread_mutex_unlock(&argo_env_mutex);

    return result;
}

/* Unset environment variable */
int argo_unsetenv(const char* name) {
    ARGO_CHECK_NULL(name);

    int lock_result = pthread_mutex_lock(&argo_env_mutex);
    if (lock_result != 0) {
        argo_report_error(E_SYSTEM_PROCESS, "argo_unsetenv", "Failed to acquire mutex");
        return E_SYSTEM_PROCESS;
    }

    int idx = find_env_index(name);
    if (idx >= 0) {
        free(argo_env[idx]);

        for (int i = idx; i < argo_env_count - 1; i++) {
            argo_env[i] = argo_env[i + 1];
        }

        argo_env_count--;
        argo_env[argo_env_count] = NULL;
    }

    pthread_mutex_unlock(&argo_env_mutex);
    return ARGO_SUCCESS;
}

/* Get integer environment variable */
int argo_getenvint(const char* name, int* value) {
    ARGO_CHECK_NULL(name);
    ARGO_CHECK_NULL(value);

    const char* str_value = argo_getenv(name);
    if (!str_value) return E_PROTOCOL_FORMAT;

    char* endptr;
    errno = 0;
    long result = strtol(str_value, &endptr, DECIMAL_BASE);

    if (errno == ERANGE || result > INT_MAX || result < INT_MIN) {
        return E_PROTOCOL_FORMAT;
    }

    if (endptr == str_value || *endptr != '\0') {
        return E_PROTOCOL_FORMAT;
    }

    *value = (int)result;
    return ARGO_SUCCESS;
}

/* Print environment */
void argo_env_print(void) {
    int lock_result = pthread_mutex_lock(&argo_env_mutex);
    if (lock_result != 0) {
        LOG_ERROR("Failed to acquire mutex in argo_env_print: %d", lock_result);
        return;
    }

    for (int i = 0; i < argo_env_count; i++) {
        if (argo_env[i]) {
            printf("%s\n", argo_env[i]);
        }
    }

    pthread_mutex_unlock(&argo_env_mutex);
}

/* Dump environment to file */
int argo_env_dump(const char* filepath) {
    ARGO_CHECK_NULL(filepath);

    int lock_result = pthread_mutex_lock(&argo_env_mutex);
    if (lock_result != 0) {
        argo_report_error(E_SYSTEM_PROCESS, "argo_env_dump", "Failed to acquire mutex");
        return E_SYSTEM_PROCESS;
    }

    FILE* fp = fopen(filepath, "w");
    if (!fp) {
        pthread_mutex_unlock(&argo_env_mutex);
        return E_SYSTEM_FILE;
    }

    fprintf(fp, "# Argo Environment Dump\n");
    fprintf(fp, "# Total variables: %d\n\n", argo_env_count);

    for (int i = 0; i < argo_env_count; i++) {
        if (argo_env[i]) {
            fprintf(fp, "%s\n", argo_env[i]);
        }
    }

    fclose(fp);
    pthread_mutex_unlock(&argo_env_mutex);

    return ARGO_SUCCESS;
}

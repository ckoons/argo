/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/* Project includes */
#include "argo_env.h"
#include "argo_error.h"
#include "argo_log.h"

/* Environment entry */
typedef struct env_entry {
    char* key;
    char* value;
    struct env_entry* next;
} env_entry_t;

/* Environment structure */
struct argo_env {
    env_entry_t* head;
    int count;
};

/* Create isolated environment */
argo_env_t* argo_env_create(void) {
    argo_env_t* env = calloc(1, sizeof(argo_env_t));
    if (!env) {
        argo_report_error(E_SYSTEM_MEMORY, "argo_env_create", "allocation failed");
        return NULL;
    }

    env->head = NULL;
    env->count = 0;

    return env;
}

/* Find entry by key */
static env_entry_t* find_entry(const argo_env_t* env, const char* key) {
    if (!env || !key) return NULL;

    env_entry_t* entry = env->head;
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

/* Set variable in isolated environment */
int argo_env_set(argo_env_t* env, const char* key, const char* value) {
    if (!env || !key || !value) {
        return E_INPUT_NULL;
    }

    /* Check if key exists */
    env_entry_t* existing = find_entry(env, key);
    if (existing) {
        /* Update existing */
        char* new_value = strdup(value);
        if (!new_value) {
            argo_report_error(E_SYSTEM_MEMORY, "argo_env_set", "value allocation failed");
            return E_SYSTEM_MEMORY;
        }
        free(existing->value);
        existing->value = new_value;
        return ARGO_SUCCESS;
    }

    /* Create new entry */
    env_entry_t* entry = calloc(1, sizeof(env_entry_t));
    if (!entry) {
        argo_report_error(E_SYSTEM_MEMORY, "argo_env_set", "entry allocation failed");
        return E_SYSTEM_MEMORY;
    }

    entry->key = strdup(key);
    entry->value = strdup(value);

    if (!entry->key || !entry->value) {
        free(entry->key);
        free(entry->value);
        free(entry);
        argo_report_error(E_SYSTEM_MEMORY, "argo_env_set", "string allocation failed");
        return E_SYSTEM_MEMORY;
    }

    /* Add to head of list */
    entry->next = env->head;
    env->head = entry;
    env->count++;

    return ARGO_SUCCESS;
}

/* Get variable from isolated environment */
const char* argo_env_get(const argo_env_t* env, const char* key) {
    if (!env || !key) return NULL;

    env_entry_t* entry = find_entry(env, key);
    return entry ? entry->value : NULL;
}

/* Convert environment to envp array */
char** argo_env_to_envp(const argo_env_t* env) {
    if (!env) return NULL;

    /* Allocate array (count + 1 for NULL terminator) */
    char** envp = calloc(env->count + 1, sizeof(char*));
    if (!envp) {
        argo_report_error(E_SYSTEM_MEMORY, "argo_env_to_envp", "array allocation failed");
        return NULL;
    }

    /* Build KEY=VALUE strings */
    int index = 0;
    env_entry_t* entry = env->head;
    while (entry) {
        size_t len = strlen(entry->key) + strlen(entry->value) + 2; /* key=value\0 */
        envp[index] = malloc(len);
        if (!envp[index]) {
            /* Cleanup on failure */
            for (int i = 0; i < index; i++) {
                free(envp[i]);
            }
            free(envp);
            argo_report_error(E_SYSTEM_MEMORY, "argo_env_to_envp", "string allocation failed");
            return NULL;
        }

        snprintf(envp[index], len, "%s=%s", entry->key, entry->value);
        index++;
        entry = entry->next;
    }

    envp[index] = NULL; /* NULL terminator */
    return envp;
}

/* Free envp array */
void argo_env_free_envp(char** envp) {
    if (!envp) return;

    for (int i = 0; envp[i] != NULL; i++) {
        free(envp[i]);
    }
    free(envp);
}

/* Spawn process with isolated environment */
int argo_spawn_with_env(const char* path, char* const argv[],
                        const argo_env_t* env, pid_t* pid) {
    if (!path || !argv || !pid) {
        return E_INPUT_NULL;
    }

    /* Convert env to envp */
    char** envp = NULL;
    if (env) {
        envp = argo_env_to_envp(env);
        if (!envp) {
            return E_SYSTEM_MEMORY;
        }
    }

    /* Fork */
    pid_t child_pid = fork();
    if (child_pid < 0) {
        argo_env_free_envp(envp);
        argo_report_error(E_SYSTEM_PROCESS, "argo_spawn_with_env", "fork failed");
        return E_SYSTEM_PROCESS;
    }

    if (child_pid == 0) {
        /* Child process */
        execve(path, argv, envp ? envp : (char*[]){NULL});

        /* If execve returns, it failed */
        perror("execve failed");
        exit(1);
    }

    /* Parent process */
    *pid = child_pid;
    argo_env_free_envp(envp);

    LOG_DEBUG("Spawned process %d: %s", child_pid, path);
    return ARGO_SUCCESS;
}

/* Destroy isolated environment */
void argo_env_destroy(argo_env_t* env) {
    if (!env) return;

    env_entry_t* entry = env->head;
    while (entry) {
        env_entry_t* next = entry->next;
        free(entry->key);
        free(entry->value);
        free(entry);
        entry = next;
    }

    free(env);
}

/* Count variables in environment */
int argo_env_size(const argo_env_t* env) {
    return env ? env->count : 0;
}

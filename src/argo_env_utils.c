/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <limits.h>
#include <pwd.h>

/* Project includes */
#include "argo_env_utils.h"
#include "argo_error.h"
#include "argo_error_messages.h"
#include "argo_log.h"

/* Global environment state */
static char** argo_env = NULL;
static int argo_env_count = 0;
static int argo_env_capacity = 0;
static pthread_mutex_t argo_env_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool argo_env_initialized = false;

/* External system environment */
extern char** environ;

/* Static helper functions */
static int grow_env_array(void);
static int find_env_index(const char* name);
static int set_env_internal(const char* name, const char* value);
static int load_system_environ(void);
static int load_env_file(const char* filepath, bool required);
static int parse_env_line(const char* line, char** key, char** value);
static char* expand_value(const char* value, int depth);
static int expand_all_variables(void);
static char* get_home_dir(void);
static const char* find_env_argo_file(void);
static void trim_whitespace(char* str);
static void strip_quotes(char* str);

/* Grow environment array by quantum */
static int grow_env_array(void) {
    int new_capacity = argo_env_capacity + ARGO_ENV_GROWTH_QUANTUM;
    char** new_env = realloc(argo_env, (new_capacity + 1) * sizeof(char*));
    if (!new_env) {
        return E_SYSTEM_MEMORY;
    }

    /* Clear new entries */
    for (int i = argo_env_capacity; i <= new_capacity; i++) {
        new_env[i] = NULL;
    }

    argo_env = new_env;
    argo_env_capacity = new_capacity;
    return ARGO_SUCCESS;
}

/* Find index of variable by name */
static int find_env_index(const char* name) {
    size_t name_len = strlen(name);

    for (int i = 0; i < argo_env_count; i++) {
        if (argo_env[i] && strncmp(argo_env[i], name, name_len) == 0 &&
            argo_env[i][name_len] == '=') {
            return i;
        }
    }

    return -1;
}

/* Set environment variable (internal, assumes mutex held) */
static int set_env_internal(const char* name, const char* value) {
    /* Build NAME=VALUE string */
    size_t len = strlen(name) + strlen(value) + 2;
    char* entry = malloc(len);
    if (!entry) {
        return E_SYSTEM_MEMORY;
    }

    snprintf(entry, len, "%s=%s", name, value);

    /* Find existing or add new */
    int idx = find_env_index(name);
    if (idx >= 0) {
        /* Overwrite existing */
        free(argo_env[idx]);
        argo_env[idx] = entry;
    } else {
        /* Add new */
        if (argo_env_count >= argo_env_capacity) {
            int result = grow_env_array();
            if (result != ARGO_SUCCESS) {
                free(entry);
                return result;
            }
        }

        argo_env[argo_env_count++] = entry;
        argo_env[argo_env_count] = NULL;
    }

    return ARGO_SUCCESS;
}

/* Load system environment */
static int load_system_environ(void) {
    for (char** env = environ; *env != NULL; env++) {
        char* eq = strchr(*env, '=');
        if (!eq) continue;

        size_t name_len = eq - *env;
        char* name = strndup(*env, name_len);
        char* value = strdup(eq + 1);

        if (!name || !value) {
            free(name);
            free(value);
            return E_SYSTEM_MEMORY;
        }

        int result = set_env_internal(name, value);
        free(name);
        free(value);

        if (result != ARGO_SUCCESS) {
            return result;
        }
    }

    LOG_DEBUG("Loaded %d variables from system environment", argo_env_count);
    return ARGO_SUCCESS;
}

/* Trim leading and trailing whitespace */
static void trim_whitespace(char* str) {
    if (!str) return;

    /* Trim leading */
    char* start = str;
    while (*start && isspace(*start)) {
        start++;
    }

    /* Trim trailing */
    char* end = start + strlen(start) - 1;
    while (end > start && isspace(*end)) {
        end--;
    }
    *(end + 1) = '\0';

    /* Move trimmed string to start */
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

/* Strip surrounding quotes */
static void strip_quotes(char* str) {
    if (!str) return;

    size_t len = strlen(str);
    if (len >= 2 &&
        ((str[0] == '"' && str[len-1] == '"') ||
         (str[0] == '\'' && str[len-1] == '\''))) {
        memmove(str, str + 1, len - 2);
        str[len - 2] = '\0';
    }
}

/* Parse environment file line */
static int parse_env_line(const char* line, char** key, char** value) {
    *key = NULL;
    *value = NULL;

    /* Make working copy - allocate dynamically for large lines */
    char* buffer = strdup(line);
    if (!buffer) {
        return E_SYSTEM_MEMORY;
    }

    trim_whitespace(buffer);

    /* Skip empty lines and comments */
    if (buffer[0] == '\0' || buffer[0] == '#') {
        free(buffer);
        return ARGO_SUCCESS;
    }

    /* Strip 'export ' prefix if present */
    char* parse_start = buffer;
    if (strncmp(buffer, "export ", 7) == 0) {
        parse_start = buffer + 7;
        trim_whitespace(parse_start);
    }

    /* Find '=' separator */
    char* eq = strchr(parse_start, '=');
    if (!eq) {
        /* Not a valid KEY=VALUE line */
        free(buffer);
        return ARGO_SUCCESS;
    }

    /* Extract key */
    *eq = '\0';
    trim_whitespace(parse_start);

    /* Validate non-empty key */
    if (parse_start[0] == '\0') {
        free(buffer);
        return ARGO_SUCCESS;
    }

    *key = strdup(parse_start);

    /* Extract value */
    char* val = eq + 1;
    trim_whitespace(val);
    strip_quotes(val);
    *value = strdup(val);

    if (!*key || !*value) {
        free(*key);
        free(*value);
        *key = *value = NULL;
        return E_SYSTEM_MEMORY;
    }

    return ARGO_SUCCESS;
}

/* Load environment file */
static int load_env_file(const char* filepath, bool required) {
    FILE* fp = fopen(filepath, "r");
    if (!fp) {
        if (required) {
            LOG_ERROR("Required environment file not found: %s", filepath);
            return E_SYSTEM_FILE;
        } else {
            LOG_INFO("Optional environment file not found: %s", filepath);
            return ARGO_SUCCESS;
        }
    }

    LOG_INFO("Loading environment from: %s", filepath);

    char line[ARGO_ENV_LINE_MAX];
    int line_num = 0;
    int vars_loaded = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;

        /* Remove newline */
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }

        char* key = NULL;
        char* value = NULL;
        int result = parse_env_line(line, &key, &value);

        if (result != ARGO_SUCCESS) {
            LOG_WARN("Malformed line in %s:%d: %s", filepath, line_num, line);
            continue;
        }

        if (key && value) {
            result = set_env_internal(key, value);
            if (result == ARGO_SUCCESS) {
                vars_loaded++;
            }
            free(key);
            free(value);
        }
    }

    fclose(fp);
    LOG_INFO("Loaded %d variables from %s", vars_loaded, filepath);
    return ARGO_SUCCESS;
}

/* Get home directory */
static char* get_home_dir(void) {
    const char* home = getenv("HOME");
    if (home) {
        return strdup(home);
    }

    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) {
        return strdup(pw->pw_dir);
    }

    return NULL;
}

/* Find .env.argo file by searching upward */
static const char* find_env_argo_file(void) {
    static char found_path[PATH_MAX];
    char cwd[PATH_MAX];

    if (!getcwd(cwd, sizeof(cwd))) {
        return NULL;
    }

    char search_dir[PATH_MAX];
    strncpy(search_dir, cwd, sizeof(search_dir) - 1);

    while (1) {
        snprintf(found_path, sizeof(found_path), "%s/%s",
                search_dir, ARGO_ENV_PROJECT_FILE);

        if (access(found_path, F_OK) == 0) {
            LOG_DEBUG("Found %s in %s", ARGO_ENV_PROJECT_FILE, search_dir);
            return found_path;
        }

        /* Move to parent directory */
        char* last_slash = strrchr(search_dir, '/');
        if (!last_slash || last_slash == search_dir) {
            /* Reached root */
            break;
        }

        *last_slash = '\0';
    }

    return NULL;
}

/* Expand ${VAR} references in value */
static char* expand_value(const char* value, int depth) {
    if (depth >= ARGO_ENV_MAX_EXPANSION_DEPTH) {
        LOG_WARN("Variable expansion depth limit reached");
        return strdup(value);
    }

    char var_name[256];
    char result[ARGO_ENV_LINE_MAX];
    result[0] = '\0';

    const char* src = value;
    size_t result_len = 0;

    while (*src && result_len < sizeof(result) - 1) {
        if (src[0] == '$' && src[1] == '{') {
            /* Found ${VAR} */
            const char* var_start = src + 2;
            const char* var_end = strchr(var_start, '}');

            if (var_end) {
                /* Extract variable name */
                size_t var_len = var_end - var_start;
                if (var_len < sizeof(var_name)) {
                    strncpy(var_name, var_start, var_len);
                    var_name[var_len] = '\0';

                    /* Look up variable */
                    int idx = find_env_index(var_name);
                    if (idx >= 0) {
                        /* Found - get value after '=' */
                        char* eq = strchr(argo_env[idx], '=');
                        if (eq) {
                            const char* var_value = eq + 1;

                            /* Recursive expansion */
                            char* expanded = expand_value(var_value, depth + 1);
                            size_t exp_len = strlen(expanded);

                            if (result_len + exp_len < sizeof(result) - 1) {
                                strcpy(result + result_len, expanded);
                                result_len += exp_len;
                            }

                            free(expanded);
                        }
                    }

                    src = var_end + 1;
                    continue;
                }
            }
        }

        /* Regular character */
        result[result_len++] = *src++;
    }

    result[result_len] = '\0';
    return strdup(result);
}

/* Expand all variables in environment */
static int expand_all_variables(void) {
    for (int i = 0; i < argo_env_count; i++) {
        char* eq = strchr(argo_env[i], '=');
        if (!eq) continue;

        const char* value = eq + 1;

        /* Check if expansion needed */
        if (!strstr(value, "${")) {
            continue;
        }

        /* Extract name */
        size_t name_len = eq - argo_env[i];
        char* name = strndup(argo_env[i], name_len);
        if (!name) {
            return E_SYSTEM_MEMORY;
        }

        /* Expand value */
        char* expanded = expand_value(value, 0);

        /* Replace */
        free(argo_env[i]);
        size_t total_len = name_len + strlen(expanded) + 2;
        argo_env[i] = malloc(total_len);

        if (!argo_env[i]) {
            free(name);
            free(expanded);
            return E_SYSTEM_MEMORY;
        }

        snprintf(argo_env[i], total_len, "%s=%s", name, expanded);

        free(name);
        free(expanded);
    }

    return ARGO_SUCCESS;
}

/* Load Argo environment */
int argo_loadenv(void) {
    pthread_mutex_lock(&argo_env_mutex);

    /* Free existing environment if reloading */
    if (argo_env_initialized) {
        for (int i = 0; i < argo_env_count; i++) {
            free(argo_env[i]);
        }
        argo_env_count = 0;
    }

    /* Initialize array if first time */
    if (!argo_env) {
        argo_env_capacity = ARGO_ENV_INITIAL_CAPACITY;
        argo_env = calloc(argo_env_capacity + 1, sizeof(char*));
        if (!argo_env) {
            pthread_mutex_unlock(&argo_env_mutex);
            return E_SYSTEM_MEMORY;
        }
    }

    LOG_INFO("Loading Argo environment");

    /* 1. Load system environment */
    int result = load_system_environ();
    if (result != ARGO_SUCCESS) {
        pthread_mutex_unlock(&argo_env_mutex);
        return result;
    }

    /* 2. Load ~/.env (optional) */
    char* home = get_home_dir();
    if (home) {
        char home_env[PATH_MAX];
        snprintf(home_env, sizeof(home_env), "%s/%s", home, ARGO_ENV_HOME_FILE);
        load_env_file(home_env, false);
        free(home);
    }

    /* 3. Find .env.argo (check ARGO_ROOT or search upward) */
    const char* argo_root = NULL;
    int idx = find_env_index(ARGO_ROOT_VAR);
    if (idx >= 0) {
        char* eq = strchr(argo_env[idx], '=');
        if (eq) {
            argo_root = eq + 1;
        }
    }

    char project_env[PATH_MAX];
    char local_env[PATH_MAX];

    if (argo_root && argo_root[0]) {
        /* Use ARGO_ROOT */
        snprintf(project_env, sizeof(project_env), "%s/%s",
                argo_root, ARGO_ENV_PROJECT_FILE);
        snprintf(local_env, sizeof(local_env), "%s/%s",
                argo_root, ARGO_ENV_LOCAL_FILE);
    } else {
        /* Search upward */
        const char* found = find_env_argo_file();
        if (!found) {
            LOG_ERROR("Cannot find %s (searched upward from cwd, no ARGO_ROOT set)",
                     ARGO_ENV_PROJECT_FILE);
            pthread_mutex_unlock(&argo_env_mutex);
            return E_SYSTEM_FILE;
        }

        strncpy(project_env, found, sizeof(project_env) - 1);

        /* Extract directory for ARGO_ROOT and local file */
        char* last_slash = strrchr(project_env, '/');
        if (last_slash) {
            size_t dir_len = last_slash - project_env;
            char root_dir[PATH_MAX];
            strncpy(root_dir, project_env, dir_len);
            root_dir[dir_len] = '\0';

            /* Set ARGO_ROOT to discovered directory */
            set_env_internal(ARGO_ROOT_VAR, root_dir);

            /* Build local file path */
            strncpy(local_env, project_env, dir_len);
            local_env[dir_len] = '\0';
            snprintf(local_env + dir_len, sizeof(local_env) - dir_len,
                    "/%s", ARGO_ENV_LOCAL_FILE);
        }
    }

    /* 4. Load .env.argo (required) */
    result = load_env_file(project_env, true);
    if (result != ARGO_SUCCESS) {
        pthread_mutex_unlock(&argo_env_mutex);
        return result;
    }

    /* 5. Load .env.argo.local (optional) */
    load_env_file(local_env, false);

    /* 6. Expand all ${VAR} references */
    result = expand_all_variables();
    if (result != ARGO_SUCCESS) {
        pthread_mutex_unlock(&argo_env_mutex);
        return result;
    }

    argo_env_initialized = true;
    LOG_INFO("Argo environment loaded: %d variables", argo_env_count);

    pthread_mutex_unlock(&argo_env_mutex);
    return ARGO_SUCCESS;
}

/* Get environment variable */
const char* argo_getenv(const char* name) {
    if (!name) return NULL;

    pthread_mutex_lock(&argo_env_mutex);

    int idx = find_env_index(name);
    const char* result = NULL;

    if (idx >= 0) {
        char* eq = strchr(argo_env[idx], '=');
        if (eq) {
            result = eq + 1;
        }
    }

    pthread_mutex_unlock(&argo_env_mutex);
    return result;
}

/* Set environment variable */
int argo_setenv(const char* name, const char* value) {
    if (!name || !value) {
        return E_INPUT_NULL;
    }

    pthread_mutex_lock(&argo_env_mutex);
    int result = set_env_internal(name, value);
    pthread_mutex_unlock(&argo_env_mutex);

    return result;
}

/* Unset environment variable */
int argo_unsetenv(const char* name) {
    if (!name) {
        return E_INPUT_NULL;
    }

    pthread_mutex_lock(&argo_env_mutex);

    int idx = find_env_index(name);
    if (idx >= 0) {
        free(argo_env[idx]);

        /* Shift remaining entries */
        for (int i = idx; i < argo_env_count - 1; i++) {
            argo_env[i] = argo_env[i + 1];
        }

        argo_env_count--;
        argo_env[argo_env_count] = NULL;
    }

    pthread_mutex_unlock(&argo_env_mutex);
    return ARGO_SUCCESS;
}

/* Clear environment */
int argo_clearenv(void) {
    pthread_mutex_lock(&argo_env_mutex);

    for (int i = 0; i < argo_env_count; i++) {
        free(argo_env[i]);
        argo_env[i] = NULL;
    }

    argo_env_count = 0;

    pthread_mutex_unlock(&argo_env_mutex);
    return ARGO_SUCCESS;
}

/* Free environment */
void argo_freeenv(void) {
    pthread_mutex_lock(&argo_env_mutex);

    for (int i = 0; i < argo_env_count; i++) {
        free(argo_env[i]);
    }

    free(argo_env);
    argo_env = NULL;
    argo_env_count = 0;
    argo_env_capacity = 0;
    argo_env_initialized = false;

    pthread_mutex_unlock(&argo_env_mutex);
}

/* Get integer environment variable */
int argo_getenvint(const char* name, int* value) {
    if (!name || !value) {
        return E_INPUT_NULL;
    }

    const char* str_value = argo_getenv(name);
    if (!str_value) {
        return E_PROTOCOL_FORMAT;
    }

    char* endptr;
    errno = 0;
    long result = strtol(str_value, &endptr, 10);

    /* Check for conversion errors */
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
    pthread_mutex_lock(&argo_env_mutex);

    for (int i = 0; i < argo_env_count; i++) {
        if (argo_env[i]) {
            printf("%s\n", argo_env[i]);
        }
    }

    pthread_mutex_unlock(&argo_env_mutex);
}

/* Dump environment to file */
int argo_env_dump(const char* filepath) {
    if (!filepath) {
        return E_INPUT_NULL;
    }

    pthread_mutex_lock(&argo_env_mutex);

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

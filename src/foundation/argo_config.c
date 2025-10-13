/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

/* Project includes */
#include "argo_config.h"
#include "argo_env_utils.h"
#include "argo_error.h"
#include "argo_log.h"
#include "argo_limits.h"

/* Config entry structure */
typedef struct config_entry {
    char* key;
    char* value;
    struct config_entry* next;
} config_entry_t;

/* Global configuration state */
static config_entry_t* config_head = NULL;
static bool config_initialized = false;

/* Forward declarations */
static int load_config_directory(const char* dir_path);
static int load_config_file(const char* file_path);
static int parse_config_line(const char* line, char** key, char** value);
static void set_config_internal(const char* key, const char* value);
static config_entry_t* find_config_entry(const char* key);
static void free_config_entries(void);
static void create_directory_structure(void);
static void trim_whitespace(char* str);
static void strip_quotes(char* str);

/* Create .argo directory structure */
static void create_directory_structure(void) {
    const char* home = getenv("HOME");
    if (!home) return;

    char dir_path[ARGO_PATH_MAX];

    /* Create ~/.argo */
    snprintf(dir_path, sizeof(dir_path), "%s/.argo", home);
    mkdir(dir_path, ARGO_DIR_PERMISSIONS);

    /* Create ~/.argo/config */
    snprintf(dir_path, sizeof(dir_path), "%s/.argo/config", home);
    mkdir(dir_path, ARGO_DIR_PERMISSIONS);

    /* Create ~/.argo/logs */
    snprintf(dir_path, sizeof(dir_path), "%s/.argo/logs", home);
    mkdir(dir_path, ARGO_DIR_PERMISSIONS);

    /* Create ~/.argo/sessions */
    snprintf(dir_path, sizeof(dir_path), "%s/.argo/sessions", home);
    mkdir(dir_path, ARGO_DIR_PERMISSIONS);

    /* If we have ARGO_ROOT, create project directories */
    const char* argo_root = argo_getenv("ARGO_ROOT");
    if (argo_root && argo_root[0]) {
        /* Create <project>/.argo */
        snprintf(dir_path, sizeof(dir_path), "%s/.argo", argo_root);
        mkdir(dir_path, ARGO_DIR_PERMISSIONS);

        /* Create <project>/.argo/config */
        snprintf(dir_path, sizeof(dir_path), "%s/.argo/config", argo_root);
        mkdir(dir_path, ARGO_DIR_PERMISSIONS);

        /* Create <project>/workflows */
        snprintf(dir_path, sizeof(dir_path), "%s/workflows", argo_root);
        mkdir(dir_path, ARGO_DIR_PERMISSIONS);

        /* Create <project>/workflows/config */
        snprintf(dir_path, sizeof(dir_path), "%s/workflows/config", argo_root);
        mkdir(dir_path, ARGO_DIR_PERMISSIONS);
    }
}

/* Trim leading and trailing whitespace */
static void trim_whitespace(char* str) {
    if (!str) return;

    char* start = str;
    while (*start && isspace(*start)) start++;

    char* end = start + strlen(start) - 1;
    while (end > start && isspace(*end)) end--;
    *(end + 1) = '\0';

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

/* Parse config line into key/value */
static int parse_config_line(const char* line, char** key, char** value) {
    *key = NULL;
    *value = NULL;

    char* buffer = strdup(line);
    if (!buffer) return E_SYSTEM_MEMORY;

    trim_whitespace(buffer);

    /* Skip empty lines and comments */
    if (buffer[0] == '\0' || buffer[0] == '#') {
        free(buffer);
        return ARGO_SUCCESS;
    }

    /* Find = separator */
    char* eq = strchr(buffer, '=');
    if (!eq) {
        free(buffer);
        return ARGO_SUCCESS;  /* Not an error, just skip */
    }

    *eq = '\0';
    char* key_str = buffer;
    char* val_str = eq + 1;

    trim_whitespace(key_str);
    trim_whitespace(val_str);
    strip_quotes(val_str);

    if (key_str[0] == '\0') {
        free(buffer);
        return ARGO_SUCCESS;
    }

    *key = strdup(key_str);
    *value = strdup(val_str);

    free(buffer);

    if (!*key || !*value) {
        free(*key);
        free(*value);
        *key = *value = NULL;
        return E_SYSTEM_MEMORY;
    }

    return ARGO_SUCCESS;
}

/* Find config entry by key */
static config_entry_t* find_config_entry(const char* key) {
    if (!key) return NULL;

    config_entry_t* entry = config_head;
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

/* Set config value (internal, creates or updates) */
static void set_config_internal(const char* key, const char* value) {
    if (!key || !value) return;

    /* Check if key exists */
    config_entry_t* existing = find_config_entry(key);
    if (existing) {
        /* Update existing */
        free(existing->value);
        existing->value = strdup(value);
        return;
    }

    /* Create new entry */
    config_entry_t* entry = calloc(1, sizeof(config_entry_t));
    if (!entry) return;

    entry->key = strdup(key);
    entry->value = strdup(value);

    if (!entry->key || !entry->value) {
        free(entry->key);
        free(entry->value);
        free(entry);
        return;
    }

    /* Add to head of list */
    entry->next = config_head;
    config_head = entry;
}

/* Load single config file */
static int load_config_file(const char* file_path) {
    FILE* fp = fopen(file_path, "r");
    if (!fp) {
        /* Not an error - config files are optional */
        return ARGO_SUCCESS;
    }

    LOG_DEBUG("Loading config file: %s", file_path);

    char line[ARGO_BUFFER_STANDARD];
    int loaded = 0;

    while (fgets(line, sizeof(line), fp)) { /* GUIDELINE_APPROVED: fgets in while condition */
        /* Remove newline */
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
        }

        char* key = NULL;
        char* value = NULL;
        int result = parse_config_line(line, &key, &value);

        if (result == ARGO_SUCCESS && key && value) {
            set_config_internal(key, value);
            loaded++;
        }

        free(key);
        free(value);
    }

    /* Check for read errors */
    if (ferror(fp)) {
        LOG_WARN("Error reading config file: %s", file_path);
    }

    fclose(fp);

    if (loaded > 0) {
        LOG_INFO("Loaded %d config values from %s", loaded, file_path);
    }

    return ARGO_SUCCESS;
}

/* Load all config files from directory */
static int load_config_directory(const char* dir_path) {
    DIR* dir = opendir(dir_path);
    if (!dir) {
        /* Not an error - directories are optional */
        return ARGO_SUCCESS;
    }

    LOG_DEBUG("Scanning config directory: %s", dir_path);

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Skip . and .. */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        /* Build full path */
        char file_path[ARGO_PATH_MAX];
        snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry->d_name);

        /* Check if it's a regular file */
        struct stat st;
        if (stat(file_path, &st) == 0 && S_ISREG(st.st_mode)) {
            load_config_file(file_path);
        }
    }

    closedir(dir);
    return ARGO_SUCCESS;
}

/* Free all config entries */
static void free_config_entries(void) {
    config_entry_t* entry = config_head;
    while (entry) {
        config_entry_t* next = entry->next;
        free(entry->key);
        free(entry->value);
        free(entry);
        entry = next;
    }
    config_head = NULL;
}

/* Load Argo configuration */
int argo_config(void) {
    if (config_initialized) {
        LOG_DEBUG("Config already initialized");
        return ARGO_SUCCESS;
    }

    LOG_INFO("Loading Argo configuration");

    /* Create directory structure */
    create_directory_structure();

    /* Load config in precedence order (later overrides earlier) */
    const char* home = getenv("HOME");
    if (home) {
        char config_dir[ARGO_PATH_MAX];

        /* 1. Load ~/.argo/config/ directory */
        snprintf(config_dir, sizeof(config_dir), "%s/.argo/config", home);
        load_config_directory(config_dir);
    }

    /* 2. Load <project>/.argo/config/ directory */
    const char* argo_root = argo_getenv("ARGO_ROOT");
    if (argo_root && argo_root[0]) {
        char config_dir[ARGO_PATH_MAX];

        snprintf(config_dir, sizeof(config_dir), "%s/.argo/config", argo_root);
        load_config_directory(config_dir);

        /* 3. Load <project>/workflows/config/ directory */
        snprintf(config_dir, sizeof(config_dir), "%s/workflows/config", argo_root);
        load_config_directory(config_dir);
    }

    config_initialized = true;

    int count = 0;
    config_entry_t* entry = config_head;
    while (entry) {
        count++;
        entry = entry->next;
    }

    LOG_INFO("Configuration loaded: %d values", count);
    return ARGO_SUCCESS;
}

/* Get configuration value */
const char* argo_config_get(const char* key) {
    if (!key) return NULL;

    config_entry_t* entry = find_config_entry(key);
    return entry ? entry->value : NULL;
}

/* Reload configuration */
int argo_config_reload(void) {
    LOG_INFO("Reloading configuration");

    /* Clear existing config */
    free_config_entries();
    config_initialized = false;

    /* Reload */
    return argo_config();
}

/* Cleanup configuration subsystem */
void argo_config_cleanup(void) {
    if (!config_initialized) return;

    LOG_DEBUG("Cleaning up configuration");

    free_config_entries();
    config_initialized = false;
}

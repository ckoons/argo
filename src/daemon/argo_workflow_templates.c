/* © 2025 Casey Koons All rights reserved */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "argo_workflow_templates.h"
#include "argo_workflow_json.h"
#include "argo_error.h"
#include "argo_log.h"

/* Forward declarations from jsmn.h */
typedef struct jsmntok {
    int type;
    int start;
    int end;
    int size;
} jsmntok_t;

#define JSMN_OBJECT (1 << 0)

/* Create template collection */
workflow_template_collection_t* workflow_templates_create(void) {
    workflow_template_collection_t* collection = calloc(1, sizeof(workflow_template_collection_t));
    if (!collection) {
        LOG_ERROR("Failed to allocate template collection");
        return NULL;
    }

    collection->count = 0;
    return collection;
}

/* Destroy template collection */
void workflow_templates_destroy(workflow_template_collection_t* collection) {
    free(collection);
}

/* Check if file has .json extension */
static bool is_json_file(const char* filename) {
    const char* ext = strrchr(filename, '.');
    return ext && strcmp(ext, ".json") == 0;
}

/* Extract template metadata from JSON file */
static int extract_template_metadata(const char* path, char* name, size_t name_size,
                                     char* description, size_t desc_size) {
    /* Load JSON file */
    size_t json_size;
    char* json_content = workflow_json_load_file(path, &json_size);
    if (!json_content) {
        return E_SYSTEM_FILE;
    }

    /* Parse JSON */
    jsmntok_t* tokens = malloc(sizeof(jsmntok_t) * WORKFLOW_JSON_MAX_TOKENS);
    if (!tokens) {
        free(json_content);
        return E_SYSTEM_MEMORY;
    }

    int token_count = workflow_json_parse(json_content, tokens, WORKFLOW_JSON_MAX_TOKENS);
    if (token_count < 0) {
        free(tokens);
        free(json_content);
        return E_PROTOCOL_FORMAT;
    }

    /* Extract workflow_name field */
    int name_idx = workflow_json_find_field(json_content, tokens, 0, "workflow_name");
    if (name_idx >= 0) {
        workflow_json_extract_string(json_content, &tokens[name_idx], name, name_size);
    } else {
        /* Fallback to filename without .json */
        const char* basename = strrchr(path, '/');
        basename = basename ? basename + 1 : path;
        strncpy(name, basename, name_size - 1);
        name[name_size - 1] = '\0';

        /* Remove .json extension */
        char* ext = strrchr(name, '.');
        if (ext && strcmp(ext, ".json") == 0) {
            *ext = '\0';
        }
    }

    /* Extract description field */
    int desc_idx = workflow_json_find_field(json_content, tokens, 0, "description");
    if (desc_idx >= 0) {
        workflow_json_extract_string(json_content, &tokens[desc_idx], description, desc_size);
    } else {
        strncpy(description, "No description available", desc_size - 1);
        description[desc_size - 1] = '\0';
    }

    free(tokens);
    free(json_content);
    return ARGO_SUCCESS;
}

/* Scan directory for template files */
static int scan_template_directory(const char* dir_path, bool is_system,
                                   workflow_template_collection_t* collection) {
    DIR* dir = opendir(dir_path);
    if (!dir) {
        /* Directory doesn't exist - not an error for user templates */
        if (!is_system) {
            LOG_DEBUG("User template directory not found: %s", dir_path);
            return ARGO_SUCCESS;
        }
        LOG_WARN("System template directory not found: %s", dir_path);
        return E_SYSTEM_FILE;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Skip . and .. */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        /* Only process .json files */
        if (!is_json_file(entry->d_name)) {
            continue;
        }

        /* Check collection capacity */
        if (collection->count >= WORKFLOW_TEMPLATE_MAX_COUNT) {
            LOG_WARN("Template collection full, skipping: %s", entry->d_name);
            break;
        }

        /* Build full path */
        char full_path[WORKFLOW_TEMPLATE_PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        /* Extract metadata */
        workflow_template_t* tmpl = &collection->templates[collection->count];
        int result = extract_template_metadata(full_path,
                                               tmpl->name, sizeof(tmpl->name),
                                               tmpl->description, sizeof(tmpl->description));
        if (result != ARGO_SUCCESS) {
            LOG_WARN("Failed to parse template: %s", full_path);
            continue;
        }

        /* Store path and system flag */
        strncpy(tmpl->path, full_path, sizeof(tmpl->path) - 1);
        tmpl->path[sizeof(tmpl->path) - 1] = '\0';
        tmpl->is_system = is_system;

        collection->count++;
        LOG_DEBUG("Discovered template: %s (%s)", tmpl->name, is_system ? "system" : "user");
    }

    closedir(dir);
    return ARGO_SUCCESS;
}

/* Get user templates path */
static const char* get_user_templates_path(void) {
    const char* env_path = getenv(WORKFLOW_TEMPLATE_USER_PATH_ENV);
    if (env_path && env_path[0] != '\0') {
        return env_path;
    }

    /* Expand ~ to home directory */
    static char expanded_path[WORKFLOW_TEMPLATE_PATH_MAX];
    const char* home = getenv("HOME");
    if (home) {
        snprintf(expanded_path, sizeof(expanded_path), "%s/%s",
                 home, WORKFLOW_TEMPLATE_USER_PATH_DEFAULT);
        return expanded_path;
    }

    return WORKFLOW_TEMPLATE_USER_PATH_DEFAULT;
}

/* Discover templates from filesystem */
int workflow_templates_discover(workflow_template_collection_t* collection) {
    if (!collection) {
        return E_INVALID_PARAMS;
    }

    collection->count = 0;

    /* Scan system templates */
    int result = scan_template_directory(WORKFLOW_TEMPLATE_SYSTEM_PATH, true, collection);
    if (result != ARGO_SUCCESS && result != E_SYSTEM_FILE) {
        return result;
    }

    /* Scan user templates */
    const char* user_path = get_user_templates_path();
    result = scan_template_directory(user_path, false, collection);
    if (result != ARGO_SUCCESS && result != E_SYSTEM_FILE) {
        return result;
    }

    LOG_INFO("Discovered %d templates", collection->count);
    return ARGO_SUCCESS;
}

/* Find template by name */
workflow_template_t* workflow_templates_find(workflow_template_collection_t* collection,
                                             const char* name) {
    if (!collection || !name) {
        return NULL;
    }

    for (int i = 0; i < collection->count; i++) {
        if (strcmp(collection->templates[i].name, name) == 0) {
            return &collection->templates[i];
        }
    }

    return NULL;
}

/* Validate template file */
int workflow_template_validate(const char* path) {
    if (!path) {
        return E_INVALID_PARAMS;
    }

    /* Check file exists */
    struct stat st;
    if (stat(path, &st) != 0) {
        return E_SYSTEM_FILE;
    }

    /* Load and parse JSON */
    size_t json_size;
    char* json_content = workflow_json_load_file(path, &json_size);
    if (!json_content) {
        return E_SYSTEM_FILE;
    }

    jsmntok_t* tokens = malloc(sizeof(jsmntok_t) * WORKFLOW_JSON_MAX_TOKENS);
    if (!tokens) {
        free(json_content);
        return E_SYSTEM_MEMORY;
    }

    int token_count = workflow_json_parse(json_content, tokens, WORKFLOW_JSON_MAX_TOKENS);
    free(tokens);
    free(json_content);

    if (token_count < 0) {
        return E_PROTOCOL_FORMAT;
    }

    return ARGO_SUCCESS;
}

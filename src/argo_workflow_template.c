/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include "argo_workflow_template.h"
#include "argo_error.h"
#include "argo_limits.h"
#include "argo_file_utils.h"

/* Check if path is a directory */
static bool is_directory(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

/* Check if file exists */
static bool file_exists(const char* path) {
    return access(path, F_OK) == 0;
}

/* Parse YAML metadata file (simple key: value parser) */
static int parse_metadata_yaml(const char* content, workflow_template_t* template) {
    if (!content || !template) {
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

        /* Trim value */
        while (*value == ' ' || *value == '\t') value++;
        size_t len = strlen(value);
        while (len > 0 && (value[len-1] == ' ' || value[len-1] == '\t' || value[len-1] == '\r')) {
            value[--len] = '\0';
        }

        /* Store values */
        if (strcmp(key, "name") == 0) {
            strncpy(template->name, value, sizeof(template->name) - 1);
        } else if (strcmp(key, "description") == 0) {
            strncpy(template->description, value, sizeof(template->description) - 1);
        } else if (strcmp(key, "version") == 0) {
            strncpy(template->version, value, sizeof(template->version) - 1);
        } else if (strcmp(key, "author") == 0) {
            strncpy(template->author, value, sizeof(template->author) - 1);
        }
    }

    return ARGO_SUCCESS;
}

/* Load directory-based template */
static int load_directory_template(const char* dir_path, workflow_template_t* template) {
    char path_buffer[ARGO_PATH_MAX];

    /* Set template directory */
    strncpy(template->template_dir, dir_path, sizeof(template->template_dir) - 1);
    template->is_directory = true;

    /* Check for workflow.sh */
    snprintf(path_buffer, sizeof(path_buffer), "%s/workflow.sh", dir_path);
    if (!file_exists(path_buffer)) {
        argo_report_error(E_WORKFLOW_NOT_FOUND, "load_directory_template",
                         "workflow.sh not found");
        return E_WORKFLOW_NOT_FOUND;
    }
    strncpy(template->workflow_script, path_buffer, sizeof(template->workflow_script) - 1);

    /* Check for metadata.yaml */
    snprintf(path_buffer, sizeof(path_buffer), "%s/metadata.yaml", dir_path);
    if (file_exists(path_buffer)) {
        strncpy(template->metadata_path, path_buffer, sizeof(template->metadata_path) - 1);

        /* Read and parse metadata */
        char* metadata_content = NULL;
        size_t metadata_size = 0;
        if (file_read_all(path_buffer, &metadata_content, &metadata_size) == ARGO_SUCCESS) {
            parse_metadata_yaml(metadata_content, template);
            free(metadata_content);
        }
    }

    /* Check for README.md */
    snprintf(path_buffer, sizeof(path_buffer), "%s/README.md", dir_path);
    if (file_exists(path_buffer)) {
        template->has_readme = true;
        strncpy(template->readme_path, path_buffer, sizeof(template->readme_path) - 1);
    }

    /* Check for tests/ directory */
    snprintf(path_buffer, sizeof(path_buffer), "%s/tests", dir_path);
    if (is_directory(path_buffer)) {
        template->has_tests = true;
        strncpy(template->tests_dir, path_buffer, sizeof(template->tests_dir) - 1);
    }

    /* Check for config/ directory */
    snprintf(path_buffer, sizeof(path_buffer), "%s/config", dir_path);
    if (is_directory(path_buffer)) {
        template->has_config = true;
        strncpy(template->config_dir, path_buffer, sizeof(template->config_dir) - 1);
    }

    /* If no name in metadata, use directory name */
    if (template->name[0] == '\0') {
        const char* dir_name = strrchr(dir_path, '/');
        if (dir_name) {
            strncpy(template->name, dir_name + 1, sizeof(template->name) - 1);
        }
    }

    return ARGO_SUCCESS;
}

/* Load single-file template (legacy format) */
static int load_file_template(const char* file_path, workflow_template_t* template) {
    /* Set workflow script path */
    strncpy(template->workflow_script, file_path, sizeof(template->workflow_script) - 1);
    template->is_directory = false;

    /* Extract name from filename */
    const char* filename = strrchr(file_path, '/');
    if (filename) {
        filename++;
    } else {
        filename = file_path;
    }

    /* Remove .sh extension if present */
    char name_buf[TEMPLATE_NAME_MAX];
    strncpy(name_buf, filename, sizeof(name_buf) - 1);
    char* ext = strstr(name_buf, ".sh");
    if (ext) *ext = '\0';

    strncpy(template->name, name_buf, sizeof(template->name) - 1);

    return ARGO_SUCCESS;
}

/* Load workflow template from path (directory or file) */
int workflow_template_load(const char* path, workflow_template_t* template) {
    if (!path || !template) {
        argo_report_error(E_INVALID_PARAMS, "workflow_template_load", "null parameters");
        return E_INVALID_PARAMS;
    }

    /* Initialize template */
    memset(template, 0, sizeof(workflow_template_t));

    /* Check if path exists */
    if (!file_exists(path)) {
        argo_report_error(E_WORKFLOW_NOT_FOUND, "workflow_template_load", "path not found");
        return E_WORKFLOW_NOT_FOUND;
    }

    /* Determine if directory or file */
    if (is_directory(path)) {
        return load_directory_template(path, template);
    } else {
        return load_file_template(path, template);
    }
}

/* Validate template structure */
int workflow_template_validate(const workflow_template_t* template) {
    if (!template) {
        return E_INVALID_PARAMS;
    }

    /* Must have workflow script */
    if (template->workflow_script[0] == '\0') {
        argo_report_error(E_WORKFLOW_INVALID, "workflow_template_validate",
                         "no workflow script");
        return E_WORKFLOW_INVALID;
    }

    /* Workflow script must exist */
    if (!file_exists(template->workflow_script)) {
        argo_report_error(E_WORKFLOW_NOT_FOUND, "workflow_template_validate",
                         "workflow script not found");
        return E_WORKFLOW_NOT_FOUND;
    }

    /* Must have a name */
    if (template->name[0] == '\0') {
        argo_report_error(E_WORKFLOW_INVALID, "workflow_template_validate",
                         "no template name");
        return E_WORKFLOW_INVALID;
    }

    return ARGO_SUCCESS;
}

/* Free template resources */
void workflow_template_free(workflow_template_t* template) {
    if (!template) {
        return;
    }
    /* Currently no dynamic allocations, but keep for future use */
    memset(template, 0, sizeof(workflow_template_t));
}

/* List all templates in directory */
int workflow_template_list(const char* templates_dir, workflow_template_t** templates, int* count) {
    if (!templates_dir || !templates || !count) {
        return E_INVALID_PARAMS;
    }

    *templates = NULL;
    *count = 0;

    DIR* dir = opendir(templates_dir);
    if (!dir) {
        argo_report_error(E_WORKFLOW_NOT_FOUND, "workflow_template_list",
                         "templates directory not found");
        return E_WORKFLOW_NOT_FOUND;
    }

    /* Count templates first */
    int template_count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char path[ARGO_PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", templates_dir, entry->d_name);

        if (is_directory(path)) {
            /* Check if it has workflow.sh */
            char workflow_path[ARGO_PATH_MAX];
            snprintf(workflow_path, sizeof(workflow_path), "%s/workflow.sh", path);
            if (file_exists(workflow_path)) {
                template_count++;
            }
        } else if (strstr(entry->d_name, ".sh")) {
            template_count++;
        }
    }

    if (template_count == 0) {
        closedir(dir);
        return ARGO_SUCCESS;
    }

    /* Allocate template array */
    workflow_template_t* template_array = calloc(template_count, sizeof(workflow_template_t));
    if (!template_array) {
        closedir(dir);
        return E_SYSTEM_MEMORY;
    }

    /* Load templates */
    rewinddir(dir);
    int index = 0;
    while ((entry = readdir(dir)) != NULL && index < template_count) {
        if (entry->d_name[0] == '.') continue;

        char path[ARGO_PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", templates_dir, entry->d_name);

        bool should_load = false;
        if (is_directory(path)) {
            char workflow_path[ARGO_PATH_MAX];
            snprintf(workflow_path, sizeof(workflow_path), "%s/workflow.sh", path);
            should_load = file_exists(workflow_path);
        } else if (strstr(entry->d_name, ".sh")) {
            should_load = true;
        }

        if (should_load) {
            if (workflow_template_load(path, &template_array[index]) == ARGO_SUCCESS) {
                index++;
            }
        }
    }

    closedir(dir);

    *templates = template_array;
    *count = index;
    return ARGO_SUCCESS;
}

/* Find specific template by name */
int workflow_template_find(const char* templates_dir, const char* name,
                          workflow_template_t* template) {
    if (!templates_dir || !name || !template) {
        return E_INVALID_PARAMS;
    }

    /* Try directory first */
    char path[ARGO_PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", templates_dir, name);

    if (is_directory(path)) {
        return workflow_template_load(path, template);
    }

    /* Try as .sh file */
    snprintf(path, sizeof(path), "%s/%s.sh", templates_dir, name);
    if (file_exists(path)) {
        return workflow_template_load(path, template);
    }

    argo_report_error(E_WORKFLOW_NOT_FOUND, "workflow_template_find",
                     "template not found");
    return E_WORKFLOW_NOT_FOUND;
}

/* Read template metadata */
int workflow_template_read_metadata(const char* metadata_path, workflow_template_t* template) {
    if (!metadata_path || !template) {
        return E_INVALID_PARAMS;
    }

    char* content = NULL;
    size_t size = 0;
    int result = file_read_all(metadata_path, &content, &size);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    result = parse_metadata_yaml(content, template);
    free(content);
    return result;
}

/* Read template README */
int workflow_template_read_readme(const char* readme_path, char** content) {
    if (!readme_path || !content) {
        return E_INVALID_PARAMS;
    }

    size_t size = 0;
    return file_read_all(readme_path, content, &size);
}

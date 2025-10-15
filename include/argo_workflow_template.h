/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_WORKFLOW_TEMPLATE_H
#define ARGO_WORKFLOW_TEMPLATE_H

#include <stdbool.h>

/* Maximum sizes for template components */
#define TEMPLATE_NAME_MAX 128
#define TEMPLATE_DESC_MAX 512
#define TEMPLATE_PARAM_MAX 32
#define TEMPLATE_PARAM_NAME_MAX 64
#define TEMPLATE_PARAM_DESC_MAX 256

/* Template parameter definition */
typedef struct {
    char name[TEMPLATE_PARAM_NAME_MAX];
    char description[TEMPLATE_PARAM_DESC_MAX];
    char default_value[TEMPLATE_PARAM_DESC_MAX];
    bool required;
} template_param_t;

/* Workflow template structure */
typedef struct {
    char name[TEMPLATE_NAME_MAX];
    char description[TEMPLATE_DESC_MAX];
    char version[32];
    char author[128];

    /* Parameters */
    template_param_t params[TEMPLATE_PARAM_MAX];
    int param_count;

    /* Paths */
    char template_dir[512];
    char workflow_script[512];
    char readme_path[512];
    char metadata_path[512];
    char tests_dir[512];
    char config_dir[512];

    /* Flags */
    bool is_directory;  /* true = directory, false = single file */
    bool has_tests;
    bool has_config;
    bool has_readme;
} workflow_template_t;

/* Template operations */
int workflow_template_load(const char* path, workflow_template_t* template);
int workflow_template_validate(const workflow_template_t* template);
void workflow_template_free(workflow_template_t* template);

/* Template discovery */
int workflow_template_list(const char* templates_dir, workflow_template_t** templates, int* count);
int workflow_template_find(const char* templates_dir, const char* name, workflow_template_t* template);

/* Template metadata operations */
int workflow_template_read_metadata(const char* metadata_path, workflow_template_t* template);
int workflow_template_read_readme(const char* readme_path, char** content);

#endif /* ARGO_WORKFLOW_TEMPLATE_H */

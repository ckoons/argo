/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_WORKFLOW_TEMPLATES_H
#define ARGO_WORKFLOW_TEMPLATES_H

#include <stdbool.h>

/* Template discovery paths */
#define WORKFLOW_TEMPLATE_SYSTEM_PATH "workflows/templates"
#define WORKFLOW_TEMPLATE_USER_PATH_ENV "ARGO_TEMPLATES_PATH"
#define WORKFLOW_TEMPLATE_USER_PATH_DEFAULT ".argo/workflows/templates"

/* Template limits */
#define WORKFLOW_TEMPLATE_MAX_COUNT 100
#define WORKFLOW_TEMPLATE_NAME_MAX 64
#define WORKFLOW_TEMPLATE_DESC_MAX 256
#define WORKFLOW_TEMPLATE_PATH_MAX 512

/* Workflow template */
typedef struct {
    char name[WORKFLOW_TEMPLATE_NAME_MAX];           /* From filename (source of truth) */
    char display_name[WORKFLOW_TEMPLATE_NAME_MAX];   /* Optional from JSON, fallback to name */
    char description[WORKFLOW_TEMPLATE_DESC_MAX];
    char path[WORKFLOW_TEMPLATE_PATH_MAX];
    bool is_system;  /* true = system template, false = user template */
} workflow_template_t;

/* Template collection */
typedef struct {
    workflow_template_t templates[WORKFLOW_TEMPLATE_MAX_COUNT];
    int count;
} workflow_template_collection_t;

/* Create template collection */
workflow_template_collection_t* workflow_templates_create(void);

/* Destroy template collection */
void workflow_templates_destroy(workflow_template_collection_t* collection);

/* Discover templates from filesystem
 *
 * Scans system and user template directories for .json workflow files.
 * Parses each template to extract name and description.
 *
 * Parameters:
 *   collection - Collection to populate
 *
 * Returns:
 *   ARGO_SUCCESS on success
 *   Error code on failure
 */
int workflow_templates_discover(workflow_template_collection_t* collection);

/* Find template by name
 *
 * Parameters:
 *   collection - Collection to search
 *   name - Template name to find
 *
 * Returns:
 *   Pointer to template if found
 *   NULL if not found
 */
workflow_template_t* workflow_templates_find(workflow_template_collection_t* collection,
                                             const char* name);

/* Validate template file
 *
 * Checks if template file exists and is valid JSON with required fields.
 *
 * Parameters:
 *   path - Path to template file
 *
 * Returns:
 *   ARGO_SUCCESS if valid
 *   Error code if invalid
 */
int workflow_template_validate(const char* path);

#endif /* ARGO_WORKFLOW_TEMPLATES_H */

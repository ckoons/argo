/* © 2025 Casey Koons All rights reserved */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "argo_workflow_registry.h"
#include "argo_error.h"
#include "argo_log.h"

/* Create workflow registry */
workflow_registry_t* workflow_registry_create(const char* registry_path) {
    if (!registry_path) {
        return NULL;
    }

    workflow_registry_t* registry = calloc(1, sizeof(workflow_registry_t));
    if (!registry) {
        LOG_ERROR("Failed to allocate workflow registry");
        return NULL;
    }

    strncpy(registry->registry_path, registry_path,
            sizeof(registry->registry_path) - 1);

    registry->workflow_count = 0;
    registry->last_saved = 0;
    registry->last_modified = 0;
    registry->dirty = false;
    registry->shared_services = NULL;

    LOG_DEBUG("Created workflow registry at %s", registry_path);
    return registry;
}

/* Destroy workflow registry */
void workflow_registry_destroy(workflow_registry_t* registry) {
    if (!registry) {
        return;
    }

    /* Save any pending changes */
    if (registry->dirty) {
        workflow_registry_save(registry);
    }

    free(registry);
}

/* Add new workflow instance */
int workflow_registry_add_workflow(workflow_registry_t* registry,
                                   const char* template_name,
                                   const char* instance_name,
                                   const char* initial_branch) {
    if (!registry || !template_name || !instance_name) {
        return E_INVALID_PARAMS;
    }

    if (registry->workflow_count >= WORKFLOW_REGISTRY_MAX_WORKFLOWS) {
        return E_RESOURCE_LIMIT;
    }

    /* Create workflow ID: template_instance */
    char workflow_id[WORKFLOW_REGISTRY_ID_MAX];
    snprintf(workflow_id, sizeof(workflow_id), "%s_%s",
             template_name, instance_name);

    /* Check for duplicates */
    for (int i = 0; i < registry->workflow_count; i++) {
        if (strcmp(registry->workflows[i].id, workflow_id) == 0) {
            return E_DUPLICATE;
        }
    }

    /* Add new workflow */
    workflow_instance_t* wf = &registry->workflows[registry->workflow_count];

    strncpy(wf->id, workflow_id, sizeof(wf->id) - 1);
    strncpy(wf->template_name, template_name, sizeof(wf->template_name) - 1);
    strncpy(wf->instance_name, instance_name, sizeof(wf->instance_name) - 1);
    strncpy(wf->active_branch, initial_branch ? initial_branch : "main",
            sizeof(wf->active_branch) - 1);

    wf->status = WORKFLOW_STATUS_ACTIVE;
    wf->created_at = time(NULL);
    wf->last_active = wf->created_at;
    wf->pid = 0;  /* No process yet */

    registry->workflow_count++;

    LOG_INFO("Added workflow: %s (template=%s, instance=%s, branch=%s)",
             wf->id, wf->template_name, wf->instance_name, wf->active_branch);

    return workflow_registry_schedule_save(registry);
}

/* Remove workflow */
int workflow_registry_remove_workflow(workflow_registry_t* registry,
                                      const char* workflow_id) {
    if (!registry || !workflow_id) {
        return E_INVALID_PARAMS;
    }

    /* Find workflow */
    int found = -1;
    for (int i = 0; i < registry->workflow_count; i++) {
        if (strcmp(registry->workflows[i].id, workflow_id) == 0) {
            found = i;
            break;
        }
    }

    if (found == -1) {
        return E_NOT_FOUND;
    }

    /* Shift remaining workflows down */
    for (int i = found; i < registry->workflow_count - 1; i++) {
        registry->workflows[i] = registry->workflows[i + 1];
    }

    registry->workflow_count--;

    LOG_INFO("Removed workflow: %s", workflow_id);
    return workflow_registry_schedule_save(registry);
}

/* Get workflow by ID */
workflow_instance_t* workflow_registry_get_workflow(workflow_registry_t* registry,
                                                    const char* workflow_id) {
    if (!registry || !workflow_id) {
        return NULL;
    }

    for (int i = 0; i < registry->workflow_count; i++) {
        if (strcmp(registry->workflows[i].id, workflow_id) == 0) {
            return &registry->workflows[i];
        }
    }

    return NULL;
}

/* Update active branch */
int workflow_registry_update_branch(workflow_registry_t* registry,
                                    const char* workflow_id,
                                    const char* branch_name) {
    if (!registry || !workflow_id || !branch_name) {
        return E_INVALID_PARAMS;
    }

    workflow_instance_t* wf = workflow_registry_get_workflow(registry, workflow_id);
    if (!wf) {
        return E_NOT_FOUND;
    }

    strncpy(wf->active_branch, branch_name, sizeof(wf->active_branch) - 1);
    wf->last_active = time(NULL);

    LOG_DEBUG("Updated workflow %s branch to %s", workflow_id, branch_name);
    return workflow_registry_schedule_save(registry);
}

/* Set workflow status */
int workflow_registry_set_status(workflow_registry_t* registry,
                                 const char* workflow_id,
                                 workflow_status_t status) {
    if (!registry || !workflow_id) {
        return E_INVALID_PARAMS;
    }

    workflow_instance_t* wf = workflow_registry_get_workflow(registry, workflow_id);
    if (!wf) {
        return E_NOT_FOUND;
    }

    wf->status = status;
    wf->last_active = time(NULL);

    LOG_DEBUG("Updated workflow %s status to %s",
              workflow_id, workflow_status_string(status));
    return workflow_registry_schedule_save(registry);
}

/* List all workflows */
int workflow_registry_list(workflow_registry_t* registry,
                          workflow_instance_t** workflows,
                          int* count) {
    if (!registry || !workflows || !count) {
        return E_INVALID_PARAMS;
    }

    *workflows = registry->workflows;
    *count = registry->workflow_count;

    return ARGO_SUCCESS;
}

/* Get active workflow from environment */
workflow_instance_t* workflow_registry_get_active(workflow_registry_t* registry) {
    if (!registry) {
        return NULL;
    }

    const char* active_id = getenv("ARGO_ACTIVE_WORKFLOW");
    if (!active_id) {
        return NULL;
    }

    return workflow_registry_get_workflow(registry, active_id);
}

/* Get workflow count */
int workflow_registry_count(workflow_registry_t* registry) {
    if (!registry) {
        return 0;
    }
    return registry->workflow_count;
}

/* Convert status to string */
const char* workflow_status_string(workflow_status_t status) {
    switch (status) {
        case WORKFLOW_STATUS_ACTIVE:    return "active";
        case WORKFLOW_STATUS_SUSPENDED: return "suspended";
        case WORKFLOW_STATUS_COMPLETED: return "completed";
        default:                        return "unknown";
    }
}

/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_WORKFLOW_REGISTRY_H
#define ARGO_WORKFLOW_REGISTRY_H

#include <time.h>
#include <stdbool.h>

/* Maximum sizes */
#define WORKFLOW_REGISTRY_MAX_WORKFLOWS 64
#define WORKFLOW_REGISTRY_ID_MAX 128
#define WORKFLOW_REGISTRY_TEMPLATE_MAX 64
#define WORKFLOW_REGISTRY_INSTANCE_MAX 64
#define WORKFLOW_REGISTRY_BRANCH_MAX 64
#define WORKFLOW_REGISTRY_PATH_MAX 512

/* Batched write timeouts */
#define WORKFLOW_REGISTRY_IDLE_TIMEOUT_SEC 5
#define WORKFLOW_REGISTRY_BUSY_TIMEOUT_SEC 30

/* Workflow status */
typedef enum {
    WORKFLOW_STATUS_ACTIVE,
    WORKFLOW_STATUS_SUSPENDED,
    WORKFLOW_STATUS_COMPLETED
} workflow_status_t;

/* Workflow instance */
typedef struct workflow_instance {
    char id[WORKFLOW_REGISTRY_ID_MAX];                  /* template_instance */
    char template_name[WORKFLOW_REGISTRY_TEMPLATE_MAX];
    char instance_name[WORKFLOW_REGISTRY_INSTANCE_MAX];
    char active_branch[WORKFLOW_REGISTRY_BRANCH_MAX];
    workflow_status_t status;
    time_t created_at;
    time_t last_active;
} workflow_instance_t;

/* Forward declaration */
typedef struct shared_services shared_services_t;

/* Workflow registry */
typedef struct workflow_registry {
    workflow_instance_t workflows[WORKFLOW_REGISTRY_MAX_WORKFLOWS];
    int workflow_count;

    /* Persistence */
    char registry_path[WORKFLOW_REGISTRY_PATH_MAX];
    time_t last_saved;
    time_t last_modified;
    bool dirty;  /* Needs save */

    /* Batched write tracking */
    shared_services_t* shared_services;
} workflow_registry_t;

/* Lifecycle */
workflow_registry_t* workflow_registry_create(const char* registry_path);
void workflow_registry_destroy(workflow_registry_t* registry);

/* Persistence */
int workflow_registry_load(workflow_registry_t* registry);
int workflow_registry_save(workflow_registry_t* registry);
int workflow_registry_schedule_save(workflow_registry_t* registry);

/* Workflow management */
int workflow_registry_add_workflow(workflow_registry_t* registry,
                                   const char* template_name,
                                   const char* instance_name,
                                   const char* initial_branch);

int workflow_registry_remove_workflow(workflow_registry_t* registry,
                                      const char* workflow_id);

workflow_instance_t* workflow_registry_get_workflow(workflow_registry_t* registry,
                                                    const char* workflow_id);

int workflow_registry_update_branch(workflow_registry_t* registry,
                                    const char* workflow_id,
                                    const char* branch_name);

int workflow_registry_set_status(workflow_registry_t* registry,
                                 const char* workflow_id,
                                 workflow_status_t status);

/* Query */
int workflow_registry_list(workflow_registry_t* registry,
                          workflow_instance_t** workflows,
                          int* count);

workflow_instance_t* workflow_registry_get_active(workflow_registry_t* registry);

/* Statistics */
int workflow_registry_count(workflow_registry_t* registry);
const char* workflow_status_string(workflow_status_t status);

#endif /* ARGO_WORKFLOW_REGISTRY_H */

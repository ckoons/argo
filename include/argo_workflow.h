/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_WORKFLOW_H
#define ARGO_WORKFLOW_H

#include "argo_registry.h"
#include "argo_lifecycle.h"
#include "argo_error.h"

/* Workflow phases */
typedef enum {
    WORKFLOW_INIT,        /* Initialize workflow and assign CIs */
    WORKFLOW_PLAN,        /* Planning phase - requirements CI leads */
    WORKFLOW_DEVELOP,     /* Parallel development - builders work */
    WORKFLOW_REVIEW,      /* Code review - analysis CI checks */
    WORKFLOW_TEST,        /* Testing phase - run tests */
    WORKFLOW_MERGE,       /* Merge negotiation if conflicts */
    WORKFLOW_COMPLETE     /* Workflow finished */
} workflow_phase_t;

/* Workflow state */
typedef enum {
    WORKFLOW_STATE_IDLE,
    WORKFLOW_STATE_RUNNING,
    WORKFLOW_STATE_PAUSED,
    WORKFLOW_STATE_FAILED,
    WORKFLOW_STATE_DONE
} workflow_state_t;

/* Task structure */
typedef struct ci_task {
    char id[64];
    char description[512];
    char assigned_to[REGISTRY_NAME_MAX];
    workflow_phase_t phase;
    int completed;
    time_t assigned_at;
    time_t completed_at;
    struct ci_task* next;
} ci_task_t;

/* Workflow controller */
typedef struct workflow_controller {
    char workflow_id[64];
    workflow_phase_t current_phase;
    workflow_state_t state;

    /* Associated managers */
    ci_registry_t* registry;
    lifecycle_manager_t* lifecycle;

    /* Task management */
    ci_task_t* tasks;
    int total_tasks;
    int completed_tasks;

    /* Phase tracking */
    time_t phase_start_time;
    time_t workflow_start_time;

    /* Branch information */
    char base_branch[128];
    char feature_branch[128];

} workflow_controller_t;

/* Workflow lifecycle */
workflow_controller_t* workflow_create(ci_registry_t* registry,
                                      lifecycle_manager_t* lifecycle,
                                      const char* workflow_id);
void workflow_destroy(workflow_controller_t* workflow);

/* Workflow control */
int workflow_start(workflow_controller_t* workflow, const char* base_branch);
int workflow_pause(workflow_controller_t* workflow);
int workflow_resume(workflow_controller_t* workflow);
int workflow_advance_phase(workflow_controller_t* workflow);

/* Task management */
ci_task_t* workflow_create_task(workflow_controller_t* workflow,
                               const char* description,
                               workflow_phase_t phase);
int workflow_assign_task(workflow_controller_t* workflow,
                        const char* task_id,
                        const char* ci_name);
int workflow_complete_task(workflow_controller_t* workflow,
                          const char* task_id);
ci_task_t* workflow_find_task(workflow_controller_t* workflow,
                             const char* task_id);

/* Phase management */
int workflow_can_advance(workflow_controller_t* workflow);
const char* workflow_phase_name(workflow_phase_t phase);

/* Auto-assignment based on CI roles */
int workflow_auto_assign_tasks(workflow_controller_t* workflow);

/* Checkpoint save/restore for pause/resume */
char* workflow_save_checkpoint(workflow_controller_t* workflow);
int workflow_restore_checkpoint(workflow_controller_t* workflow,
                               const char* checkpoint_json);
int workflow_checkpoint_to_file(workflow_controller_t* workflow,
                               const char* filepath);
workflow_controller_t* workflow_restore_from_file(ci_registry_t* registry,
                                                  lifecycle_manager_t* lifecycle,
                                                  const char* filepath);

#endif /* ARGO_WORKFLOW_H */

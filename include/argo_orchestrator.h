/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_ORCHESTRATOR_H
#define ARGO_ORCHESTRATOR_H

#include "argo_workflow.h"
#include "argo_registry.h"
#include "argo_lifecycle.h"
#include "argo_merge.h"
#include "argo_error.h"

/* Orchestrator ties together all workflow components */
typedef struct argo_orchestrator {
    /* Core components */
    ci_registry_t* registry;
    lifecycle_manager_t* lifecycle;
    workflow_controller_t* workflow;

    /* Active merge negotiation */
    merge_negotiation_t* active_merge;

    /* Session info */
    char session_id[64];
    char base_branch[128];
    char feature_branch[128];

    /* State */
    int running;
    time_t started_at;

} argo_orchestrator_t;

/* Orchestrator lifecycle */
argo_orchestrator_t* orchestrator_create(const char* session_id,
                                        const char* base_branch);
void orchestrator_destroy(argo_orchestrator_t* orch);

/* CI management */
int orchestrator_add_ci(argo_orchestrator_t* orch,
                       const char* name,
                       const char* role,
                       const char* provider);

int orchestrator_start_ci(argo_orchestrator_t* orch, const char* name);
int orchestrator_stop_ci(argo_orchestrator_t* orch, const char* name);

/* Workflow execution */
int orchestrator_start_workflow(argo_orchestrator_t* orch);
int orchestrator_advance_workflow(argo_orchestrator_t* orch);
int orchestrator_pause_workflow(argo_orchestrator_t* orch);
int orchestrator_resume_workflow(argo_orchestrator_t* orch);

/* Task management */
int orchestrator_create_task(argo_orchestrator_t* orch,
                            const char* description,
                            workflow_phase_t phase);

int orchestrator_assign_all_tasks(argo_orchestrator_t* orch);

int orchestrator_complete_task(argo_orchestrator_t* orch,
                              const char* task_id,
                              const char* ci_name);

/* Messaging */
int orchestrator_send_message(argo_orchestrator_t* orch,
                             const char* from_ci,
                             const char* to_ci,
                             const char* type,
                             const char* content);

int orchestrator_broadcast_message(argo_orchestrator_t* orch,
                                  const char* from_ci,
                                  const char* role_filter,
                                  const char* type,
                                  const char* content);

/* Merge negotiation */
int orchestrator_start_merge(argo_orchestrator_t* orch,
                            const char* branch_a,
                            const char* branch_b);

int orchestrator_add_conflict(argo_orchestrator_t* orch,
                             const char* file,
                             int line_start,
                             int line_end,
                             const char* content_a,
                             const char* content_b);

int orchestrator_propose_resolution(argo_orchestrator_t* orch,
                                   const char* ci_name,
                                   int conflict_index,
                                   const char* resolution,
                                   int confidence);

int orchestrator_finalize_merge(argo_orchestrator_t* orch);

/* Status queries */
int orchestrator_is_workflow_complete(argo_orchestrator_t* orch);
int orchestrator_can_advance_phase(argo_orchestrator_t* orch);
const char* orchestrator_current_phase_name(argo_orchestrator_t* orch);

/* Statistics and reporting */
void orchestrator_print_status(argo_orchestrator_t* orch);
char* orchestrator_get_status_json(argo_orchestrator_t* orch);

/* Complete workflow lifecycle - one call from start to finish */
int run_workflow(const char* session_id,
                const char* base_branch,
                void (*setup_fn)(argo_orchestrator_t* orch, void* userdata),
                void* userdata);

#endif /* ARGO_ORCHESTRATOR_H */

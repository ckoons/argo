/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Project includes */
#include "argo_orchestrator.h"
#include "argo_log.h"

/* Create orchestrator */
argo_orchestrator_t* orchestrator_create(const char* session_id,
                                        const char* base_branch) {
    if (!session_id || !base_branch) {
        LOG_ERROR("Invalid parameters for orchestrator creation");
        return NULL;
    }

    argo_orchestrator_t* orch = calloc(1, sizeof(argo_orchestrator_t));
    if (!orch) {
        LOG_ERROR("Failed to allocate orchestrator");
        return NULL;
    }

    /* Create core components */
    orch->registry = registry_create();
    if (!orch->registry) {
        free(orch);
        return NULL;
    }

    orch->lifecycle = lifecycle_manager_create(orch->registry);
    if (!orch->lifecycle) {
        registry_destroy(orch->registry);
        free(orch);
        return NULL;
    }

    orch->workflow = workflow_create(orch->registry, orch->lifecycle, session_id);
    if (!orch->workflow) {
        lifecycle_manager_destroy(orch->lifecycle);
        registry_destroy(orch->registry);
        free(orch);
        return NULL;
    }

    /* Initialize session info */
    strncpy(orch->session_id, session_id, sizeof(orch->session_id) - 1);
    strncpy(orch->base_branch, base_branch, sizeof(orch->base_branch) - 1);
    orch->feature_branch[0] = '\0';

    orch->active_merge = NULL;
    orch->running = 0;
    orch->started_at = 0;

    LOG_INFO("Created orchestrator for session %s", session_id);
    return orch;
}

/* Destroy orchestrator */
void orchestrator_destroy(argo_orchestrator_t* orch) {
    if (!orch) return;

    if (orch->active_merge) {
        merge_negotiation_destroy(orch->active_merge);
    }

    if (orch->workflow) {
        workflow_destroy(orch->workflow);
    }

    if (orch->lifecycle) {
        lifecycle_manager_destroy(orch->lifecycle);
    }

    if (orch->registry) {
        registry_destroy(orch->registry);
    }

    LOG_INFO("Destroyed orchestrator for session %s", orch->session_id);
    free(orch);
}

/* Add CI to orchestrator */
int orchestrator_add_ci(argo_orchestrator_t* orch,
                       const char* name,
                       const char* role,
                       const char* provider) {
    if (!orch || !name || !role || !provider) {
        return E_INVALID_PARAMS;
    }

    /* Create in lifecycle manager (this also adds to registry) */
    int result = lifecycle_create_ci(orch->lifecycle, name, role, provider);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    LOG_INFO("Added CI '%s' (role: %s, provider: %s)", name, role, provider);
    return ARGO_SUCCESS;
}

/* Start a CI */
int orchestrator_start_ci(argo_orchestrator_t* orch, const char* name) {
    if (!orch || !name) {
        return E_INVALID_PARAMS;
    }

    int result = lifecycle_start_ci(orch->lifecycle, name);
    if (result == ARGO_SUCCESS) {
        registry_update_status(orch->registry, name, CI_STATUS_READY);
        LOG_INFO("Started CI '%s'", name);
    }

    return result;
}

/* Stop a CI */
int orchestrator_stop_ci(argo_orchestrator_t* orch, const char* name) {
    if (!orch || !name) {
        return E_INVALID_PARAMS;
    }

    int result = lifecycle_stop_ci(orch->lifecycle, name, 0);
    if (result == ARGO_SUCCESS) {
        registry_update_status(orch->registry, name, CI_STATUS_OFFLINE);
        LOG_INFO("Stopped CI '%s'", name);
    }

    return result;
}

/* Start workflow */
int orchestrator_start_workflow(argo_orchestrator_t* orch) {
    if (!orch) {
        return E_INVALID_PARAMS;
    }

    if (orch->running) {
        LOG_WARN("Workflow already running");
        return E_INVALID_STATE;
    }

    int result = workflow_start(orch->workflow, orch->base_branch);
    if (result == ARGO_SUCCESS) {
        orch->running = 1;
        orch->started_at = time(NULL);
        LOG_INFO("Started workflow for session %s", orch->session_id);
    }

    return result;
}

/* Advance workflow to next phase */
int orchestrator_advance_workflow(argo_orchestrator_t* orch) {
    if (!orch) {
        return E_INVALID_PARAMS;
    }

    if (!orch->running) {
        return E_INVALID_STATE;
    }

    return workflow_advance_phase(orch->workflow);
}

/* Pause workflow */
int orchestrator_pause_workflow(argo_orchestrator_t* orch) {
    if (!orch) {
        return E_INVALID_PARAMS;
    }

    return workflow_pause(orch->workflow);
}

/* Resume workflow */
int orchestrator_resume_workflow(argo_orchestrator_t* orch) {
    if (!orch) {
        return E_INVALID_PARAMS;
    }

    return workflow_resume(orch->workflow);
}

/* Create task */
int orchestrator_create_task(argo_orchestrator_t* orch,
                            const char* description,
                            workflow_phase_t phase) {
    if (!orch || !description) {
        return E_INVALID_PARAMS;
    }

    ci_task_t* task = workflow_create_task(orch->workflow, description, phase);
    return task ? ARGO_SUCCESS : E_SYSTEM_MEMORY;
}

/* Auto-assign all pending tasks */
int orchestrator_assign_all_tasks(argo_orchestrator_t* orch) {
    if (!orch) {
        return E_INVALID_PARAMS;
    }

    return workflow_auto_assign_tasks(orch->workflow);
}

/* Complete a task */
int orchestrator_complete_task(argo_orchestrator_t* orch,
                              const char* task_id,
                              const char* ci_name) {
    if (!orch || !task_id || !ci_name) {
        return E_INVALID_PARAMS;
    }

    /* Verify task exists and is assigned to this CI */
    ci_task_t* task = workflow_find_task(orch->workflow, task_id);
    if (!task) {
        return E_NOT_FOUND;
    }

    if (strcmp(task->assigned_to, ci_name) != 0) {
        LOG_WARN("Task %s not assigned to CI %s", task_id, ci_name);
        return E_INVALID_PARAMS;
    }

    /* Mark complete in workflow */
    int result = workflow_complete_task(orch->workflow, task_id);
    if (result == ARGO_SUCCESS) {
        /* Also complete in lifecycle manager */
        lifecycle_complete_task(orch->lifecycle, ci_name, task_id);
    }

    return result;
}

/* Send message between CIs */
int orchestrator_send_message(argo_orchestrator_t* orch,
                             const char* from_ci,
                             const char* to_ci,
                             const char* type,
                             const char* content) {
    if (!orch || !from_ci || !to_ci || !type || !content) {
        return E_INVALID_PARAMS;
    }

    /* Create message */
    ci_message_t* msg = message_create(from_ci, to_ci, type, content);
    if (!msg) {
        return E_SYSTEM_MEMORY;
    }

    /* Convert to JSON */
    char* json = message_to_json(msg);
    if (!json) {
        message_destroy(msg);
        return E_SYSTEM_MEMORY;
    }

    /* Send through registry */
    int result = registry_send_message(orch->registry, from_ci, to_ci, json);

    free(json);
    message_destroy(msg);

    return result;
}

/* Broadcast message to CIs by role */
int orchestrator_broadcast_message(argo_orchestrator_t* orch,
                                  const char* from_ci,
                                  const char* role_filter,
                                  const char* type,
                                  const char* content) {
    if (!orch || !from_ci || !type || !content) {
        return E_INVALID_PARAMS;
    }

    /* Create message */
    ci_message_t* msg = message_create(from_ci, "all", type, content);
    if (!msg) {
        return E_SYSTEM_MEMORY;
    }

    /* Convert to JSON */
    char* json = message_to_json(msg);
    if (!json) {
        message_destroy(msg);
        return E_SYSTEM_MEMORY;
    }

    /* Broadcast through registry */
    int result = registry_broadcast_message(orch->registry, from_ci, role_filter, json);

    free(json);
    message_destroy(msg);

    return result;
}

/* Start merge negotiation */
int orchestrator_start_merge(argo_orchestrator_t* orch,
                            const char* branch_a,
                            const char* branch_b) {
    if (!orch || !branch_a || !branch_b) {
        return E_INVALID_PARAMS;
    }

    if (orch->active_merge) {
        LOG_WARN("Merge negotiation already in progress");
        return E_INVALID_STATE;
    }

    orch->active_merge = merge_negotiation_create(branch_a, branch_b);
    if (!orch->active_merge) {
        return E_SYSTEM_MEMORY;
    }

    LOG_INFO("Started merge negotiation: %s <-> %s", branch_a, branch_b);
    return ARGO_SUCCESS;
}

/* Add conflict to active merge */
int orchestrator_add_conflict(argo_orchestrator_t* orch,
                             const char* file,
                             int line_start,
                             int line_end,
                             const char* content_a,
                             const char* content_b) {
    if (!orch || !file || !content_a || !content_b) {
        return E_INVALID_PARAMS;
    }

    if (!orch->active_merge) {
        LOG_ERROR("No active merge negotiation");
        return E_INVALID_STATE;
    }

    merge_conflict_t* conflict = merge_add_conflict(orch->active_merge,
                                                   file, line_start, line_end,
                                                   content_a, content_b);
    return conflict ? ARGO_SUCCESS : E_SYSTEM_MEMORY;
}

/* CI proposes resolution */
int orchestrator_propose_resolution(argo_orchestrator_t* orch,
                                   const char* ci_name,
                                   int conflict_index,
                                   const char* resolution,
                                   int confidence) {
    if (!orch || !ci_name || !resolution) {
        return E_INVALID_PARAMS;
    }

    if (!orch->active_merge) {
        return E_INVALID_STATE;
    }

    /* Find the conflict by index */
    merge_conflict_t* conflict = orch->active_merge->conflicts;
    for (int i = 0; i < conflict_index && conflict; i++) {
        conflict = conflict->next;
    }

    if (!conflict) {
        return E_NOT_FOUND;
    }

    return merge_propose_resolution(orch->active_merge, ci_name,
                                   conflict, resolution, confidence);
}

/* Finalize merge negotiation */
int orchestrator_finalize_merge(argo_orchestrator_t* orch) {
    if (!orch) {
        return E_INVALID_PARAMS;
    }

    if (!orch->active_merge) {
        return E_INVALID_STATE;
    }

    if (!merge_is_complete(orch->active_merge)) {
        LOG_WARN("Merge negotiation not complete - unresolved conflicts remain");
        return E_INVALID_STATE;
    }

    LOG_INFO("Merge negotiation complete - all conflicts resolved");

    /* Clean up merge */
    merge_negotiation_destroy(orch->active_merge);
    orch->active_merge = NULL;

    return ARGO_SUCCESS;
}

/* Check if workflow is complete */
int orchestrator_is_workflow_complete(argo_orchestrator_t* orch) {
    if (!orch || !orch->workflow) return 0;

    return (orch->workflow->current_phase == WORKFLOW_COMPLETE &&
            orch->workflow->state == WORKFLOW_STATE_DONE);
}

/* Check if can advance to next phase */
int orchestrator_can_advance_phase(argo_orchestrator_t* orch) {
    if (!orch || !orch->workflow) return 0;

    return workflow_can_advance(orch->workflow);
}

/* Get current phase name */
const char* orchestrator_current_phase_name(argo_orchestrator_t* orch) {
    if (!orch || !orch->workflow) return "Unknown";

    return workflow_phase_name(orch->workflow->current_phase);
}

/* Print status */
void orchestrator_print_status(argo_orchestrator_t* orch) {
    if (!orch) return;

    printf("\n========================================\n");
    printf("Orchestrator Status: %s\n", orch->session_id);
    printf("========================================\n");
    printf("Running:       %s\n", orch->running ? "Yes" : "No");
    printf("Base Branch:   %s\n", orch->base_branch);
    printf("Current Phase: %s\n", orchestrator_current_phase_name(orch));
    printf("CIs Registered: %d\n", orch->registry->count);

    if (orch->workflow) {
        printf("Tasks:         %d total, %d completed\n",
               orch->workflow->total_tasks,
               orch->workflow->completed_tasks);
    }

    if (orch->active_merge) {
        printf("Active Merge:  %d conflicts, %d resolved\n",
               orch->active_merge->conflict_count,
               orch->active_merge->resolved_count);
    }

    printf("========================================\n\n");
}

/* Get status as JSON */
char* orchestrator_get_status_json(argo_orchestrator_t* orch) {
    if (!orch) return NULL;

    char* json = malloc(4096);
    if (!json) return NULL;

    snprintf(json, 4096,
             "{\n"
             "  \"session_id\": \"%s\",\n"
             "  \"running\": %s,\n"
             "  \"base_branch\": \"%s\",\n"
             "  \"current_phase\": \"%s\",\n"
             "  \"ci_count\": %d,\n"
             "  \"tasks_total\": %d,\n"
             "  \"tasks_completed\": %d,\n"
             "  \"workflow_complete\": %s\n"
             "}",
             orch->session_id,
             orch->running ? "true" : "false",
             orch->base_branch,
             orchestrator_current_phase_name(orch),
             orch->registry->count,
             orch->workflow ? orch->workflow->total_tasks : 0,
             orch->workflow ? orch->workflow->completed_tasks : 0,
             orchestrator_is_workflow_complete(orch) ? "true" : "false");

    return json;
}

/* Complete workflow lifecycle - handles creation through destruction */
int run_workflow(const char* session_id,
                const char* base_branch,
                void (*setup_fn)(argo_orchestrator_t* orch, void* userdata),
                void* userdata) {
    int result = ARGO_SUCCESS;

    /* Create orchestrator */
    argo_orchestrator_t* orch = orchestrator_create(session_id, base_branch);
    if (!orch) {
        argo_report_error(E_SYSTEM_MEMORY, "run_workflow",
                         "Failed to create orchestrator");
        return E_SYSTEM_MEMORY;
    }

    LOG_INFO("=== Workflow Session Start: %s ===", session_id);

    /* Setup phase: caller adds CIs, creates tasks, etc */
    if (setup_fn) {
        setup_fn(orch, userdata);
    }

    /* Start workflow */
    result = orchestrator_start_workflow(orch);
    if (result != ARGO_SUCCESS) {
        argo_report_error(result, "run_workflow", "Failed to start workflow");
        goto cleanup;
    }

    /* Workflow execution happens here - in real implementation,
     * this would be an event loop processing CI messages and
     * advancing phases. For now, we just run to completion. */

    LOG_INFO("Workflow running in session %s", session_id);
    orchestrator_print_status(orch);

    /* Success */
    LOG_INFO("=== Workflow Session Complete: %s ===", session_id);

cleanup:
    /* Teardown: destroy orchestrator and all components */
    orchestrator_destroy(orch);
    LOG_INFO("Orchestrator destroyed for session %s", session_id);

    return result;
}

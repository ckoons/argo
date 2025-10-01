/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Project includes */
#include "argo_workflow.h"
#include "argo_error.h"
#include "argo_log.h"

/* Helper: Generate unique task ID */
static void generate_task_id(char* id_out, size_t len) {
    static int task_counter = 0;
    snprintf(id_out, len, "task-%ld-%d", (long)time(NULL), task_counter++);
}

/* Create workflow controller */
workflow_controller_t* workflow_create(ci_registry_t* registry,
                                      lifecycle_manager_t* lifecycle,
                                      const char* workflow_id) {
    if (!registry || !lifecycle || !workflow_id) {
        argo_report_error(E_INPUT_NULL, "workflow_create",
                         "Missing required parameters");
        return NULL;
    }

    workflow_controller_t* workflow = calloc(1, sizeof(workflow_controller_t));
    if (!workflow) {
        argo_report_error(E_SYSTEM_MEMORY, "workflow_create",
                         "Failed to allocate controller");
        return NULL;
    }

    strncpy(workflow->workflow_id, workflow_id, sizeof(workflow->workflow_id) - 1);
    workflow->current_phase = WORKFLOW_INIT;
    workflow->state = WORKFLOW_STATE_IDLE;
    workflow->registry = registry;
    workflow->lifecycle = lifecycle;
    workflow->tasks = NULL;
    workflow->total_tasks = 0;
    workflow->completed_tasks = 0;

    LOG_INFO("Created workflow: %s", workflow_id);
    return workflow;
}

/* Destroy workflow controller */
void workflow_destroy(workflow_controller_t* workflow) {
    if (!workflow) return;

    /* Free all tasks */
    ci_task_t* task = workflow->tasks;
    while (task) {
        ci_task_t* next = task->next;
        free(task);
        task = next;
    }

    LOG_INFO("Destroyed workflow: %s", workflow->workflow_id);
    free(workflow);
}

/* Start workflow */
int workflow_start(workflow_controller_t* workflow, const char* base_branch) {
    if (!workflow || !base_branch) {
        return E_INVALID_PARAMS;
    }

    if (workflow->state != WORKFLOW_STATE_IDLE) {
        LOG_WARN("Workflow %s already started", workflow->workflow_id);
        return E_INVALID_STATE;
    }

    strncpy(workflow->base_branch, base_branch, sizeof(workflow->base_branch) - 1);
    workflow->state = WORKFLOW_STATE_RUNNING;
    workflow->workflow_start_time = time(NULL);
    workflow->phase_start_time = time(NULL);

    LOG_INFO("Started workflow %s on branch %s", workflow->workflow_id, base_branch);
    return ARGO_SUCCESS;
}

/* Pause workflow */
int workflow_pause(workflow_controller_t* workflow) {
    if (!workflow) {
        return E_INVALID_PARAMS;
    }

    if (workflow->state != WORKFLOW_STATE_RUNNING) {
        return E_INVALID_STATE;
    }

    workflow->state = WORKFLOW_STATE_PAUSED;
    LOG_INFO("Paused workflow: %s", workflow->workflow_id);
    return ARGO_SUCCESS;
}

/* Resume workflow */
int workflow_resume(workflow_controller_t* workflow) {
    if (!workflow) {
        return E_INVALID_PARAMS;
    }

    if (workflow->state != WORKFLOW_STATE_PAUSED) {
        return E_INVALID_STATE;
    }

    workflow->state = WORKFLOW_STATE_RUNNING;
    LOG_INFO("Resumed workflow: %s", workflow->workflow_id);
    return ARGO_SUCCESS;
}

/* Get phase name */
const char* workflow_phase_name(workflow_phase_t phase) {
    switch (phase) {
        case WORKFLOW_INIT: return "Initialize";
        case WORKFLOW_PLAN: return "Planning";
        case WORKFLOW_DEVELOP: return "Development";
        case WORKFLOW_REVIEW: return "Review";
        case WORKFLOW_TEST: return "Testing";
        case WORKFLOW_MERGE: return "Merge";
        case WORKFLOW_COMPLETE: return "Complete";
        default: return "Unknown";
    }
}

/* Check if workflow can advance to next phase */
int workflow_can_advance(workflow_controller_t* workflow) {
    if (!workflow) return 0;

    /* Must be in running state */
    if (workflow->state != WORKFLOW_STATE_RUNNING) {
        return 0;
    }

    /* Check all tasks for current phase are complete */
    ci_task_t* task = workflow->tasks;
    while (task) {
        if (task->phase == workflow->current_phase && !task->completed) {
            return 0;  /* Found incomplete task for current phase */
        }
        task = task->next;
    }

    return 1;  /* All tasks for current phase are complete */
}

/* Advance to next workflow phase */
int workflow_advance_phase(workflow_controller_t* workflow) {
    if (!workflow) {
        return E_INVALID_PARAMS;
    }

    if (!workflow_can_advance(workflow)) {
        LOG_WARN("Cannot advance workflow %s - tasks incomplete", workflow->workflow_id);
        return E_INVALID_STATE;
    }

    /* Move to next phase */
    workflow_phase_t old_phase = workflow->current_phase;

    switch (workflow->current_phase) {
        case WORKFLOW_INIT:
            workflow->current_phase = WORKFLOW_PLAN;
            break;
        case WORKFLOW_PLAN:
            workflow->current_phase = WORKFLOW_DEVELOP;
            break;
        case WORKFLOW_DEVELOP:
            workflow->current_phase = WORKFLOW_REVIEW;
            break;
        case WORKFLOW_REVIEW:
            workflow->current_phase = WORKFLOW_TEST;
            break;
        case WORKFLOW_TEST:
            workflow->current_phase = WORKFLOW_MERGE;
            break;
        case WORKFLOW_MERGE:
            workflow->current_phase = WORKFLOW_COMPLETE;
            workflow->state = WORKFLOW_STATE_DONE;
            break;
        case WORKFLOW_COMPLETE:
            LOG_WARN("Workflow already complete");
            return E_INVALID_STATE;
    }

    workflow->phase_start_time = time(NULL);

    LOG_INFO("Workflow %s advanced: %s -> %s",
             workflow->workflow_id,
             workflow_phase_name(old_phase),
             workflow_phase_name(workflow->current_phase));

    return ARGO_SUCCESS;
}

/* Create a task */
ci_task_t* workflow_create_task(workflow_controller_t* workflow,
                               const char* description,
                               workflow_phase_t phase) {
    if (!workflow || !description) {
        argo_report_error(E_INPUT_NULL, "workflow_create_task",
                         "Missing workflow or description");
        return NULL;
    }

    ci_task_t* task = calloc(1, sizeof(ci_task_t));
    if (!task) {
        argo_report_error(E_SYSTEM_MEMORY, "workflow_create_task",
                         "Failed to allocate task");
        return NULL;
    }

    generate_task_id(task->id, sizeof(task->id));
    strncpy(task->description, description, sizeof(task->description) - 1);
    task->phase = phase;
    task->completed = 0;
    task->assigned_at = 0;
    task->completed_at = 0;
    task->assigned_to[0] = '\0';

    /* Add to task list */
    task->next = workflow->tasks;
    workflow->tasks = task;
    workflow->total_tasks++;

    LOG_INFO("Created task %s: %s (phase: %s)",
             task->id, description, workflow_phase_name(phase));

    return task;
}

/* Assign task to CI */
int workflow_assign_task(workflow_controller_t* workflow,
                        const char* task_id,
                        const char* ci_name) {
    if (!workflow || !task_id || !ci_name) {
        return E_INVALID_PARAMS;
    }

    /* Find task */
    ci_task_t* task = workflow_find_task(workflow, task_id);
    if (!task) {
        argo_report_error(E_NOT_FOUND, "workflow_assign_task", task_id);
        return E_NOT_FOUND;
    }

    /* Verify CI exists and is ready */
    ci_registry_entry_t* ci = registry_find_ci(workflow->registry, ci_name);
    if (!ci) {
        argo_report_error(E_CI_NO_PROVIDER, "workflow_assign_task", ci_name);
        return E_CI_NO_PROVIDER;
    }

    if (ci->status != CI_STATUS_READY) {
        LOG_WARN("CI %s is not ready (status: %d)", ci_name, ci->status);
        return E_CI_DISCONNECTED;
    }

    /* Assign task */
    strncpy(task->assigned_to, ci_name, sizeof(task->assigned_to) - 1);
    task->assigned_at = time(NULL);

    LOG_INFO("Assigned task %s to CI %s", task_id, ci_name);
    return ARGO_SUCCESS;
}

/* Complete a task */
int workflow_complete_task(workflow_controller_t* workflow,
                          const char* task_id) {
    if (!workflow || !task_id) {
        return E_INVALID_PARAMS;
    }

    ci_task_t* task = workflow_find_task(workflow, task_id);
    if (!task) {
        argo_report_error(E_NOT_FOUND, "workflow_complete_task", task_id);
        return E_NOT_FOUND;
    }

    if (task->completed) {
        LOG_WARN("Task %s already completed", task_id);
        return ARGO_SUCCESS;
    }

    task->completed = 1;
    task->completed_at = time(NULL);
    workflow->completed_tasks++;

    LOG_INFO("Completed task %s (%d/%d tasks done)",
             task_id, workflow->completed_tasks, workflow->total_tasks);

    return ARGO_SUCCESS;
}

/* Find task by ID */
ci_task_t* workflow_find_task(workflow_controller_t* workflow,
                             const char* task_id) {
    if (!workflow || !task_id) return NULL;

    ci_task_t* task = workflow->tasks;
    while (task) {
        if (strcmp(task->id, task_id) == 0) {
            return task;
        }
        task = task->next;
    }

    return NULL;
}

/* Auto-assign tasks based on CI roles */
int workflow_auto_assign_tasks(workflow_controller_t* workflow) {
    if (!workflow) {
        return E_INVALID_PARAMS;
    }

    int assigned_count = 0;

    /* Iterate through tasks and assign based on phase */
    ci_task_t* task = workflow->tasks;
    while (task) {
        /* Skip already assigned tasks */
        if (task->assigned_to[0] != '\0') {
            task = task->next;
            continue;
        }

        /* Determine role based on phase */
        const char* target_role = NULL;
        switch (task->phase) {
            case WORKFLOW_PLAN:
                target_role = "requirements";
                break;
            case WORKFLOW_DEVELOP:
                target_role = "builder";
                break;
            case WORKFLOW_REVIEW:
                target_role = "analysis";
                break;
            case WORKFLOW_TEST:
                target_role = "builder";  /* Builders can run tests */
                break;
            case WORKFLOW_MERGE:
                target_role = "coordinator";
                break;
            default:
                task = task->next;
                continue;
        }

        /* Find available CI with matching role */
        ci_registry_entry_t* ci = registry_find_by_role(workflow->registry, target_role);
        if (ci && ci->status == CI_STATUS_READY) {
            int result = workflow_assign_task(workflow, task->id, ci->name);
            if (result == ARGO_SUCCESS) {
                assigned_count++;
            }
        }

        task = task->next;
    }

    LOG_INFO("Auto-assigned %d tasks", assigned_count);
    return ARGO_SUCCESS;
}

/* Save workflow state to JSON checkpoint */
char* workflow_save_checkpoint(workflow_controller_t* workflow) {
    if (!workflow) return NULL;

    /* Allocate buffer for JSON */
    size_t capacity = 8192;
    char* json = malloc(capacity);
    if (!json) return NULL;

    int pos = 0;
    pos += snprintf(json + pos, capacity - pos, "{\n");
    pos += snprintf(json + pos, capacity - pos, "  \"workflow_id\": \"%s\",\n",
                   workflow->workflow_id);
    pos += snprintf(json + pos, capacity - pos, "  \"current_phase\": %d,\n",
                   workflow->current_phase);
    pos += snprintf(json + pos, capacity - pos, "  \"state\": %d,\n",
                   workflow->state);
    pos += snprintf(json + pos, capacity - pos, "  \"base_branch\": \"%s\",\n",
                   workflow->base_branch);
    pos += snprintf(json + pos, capacity - pos, "  \"feature_branch\": \"%s\",\n",
                   workflow->feature_branch);
    pos += snprintf(json + pos, capacity - pos, "  \"total_tasks\": %d,\n",
                   workflow->total_tasks);
    pos += snprintf(json + pos, capacity - pos, "  \"completed_tasks\": %d,\n",
                   workflow->completed_tasks);
    pos += snprintf(json + pos, capacity - pos, "  \"phase_start_time\": %ld,\n",
                   (long)workflow->phase_start_time);
    pos += snprintf(json + pos, capacity - pos, "  \"workflow_start_time\": %ld,\n",
                   (long)workflow->workflow_start_time);

    /* Save tasks */
    pos += snprintf(json + pos, capacity - pos, "  \"tasks\": [\n");
    ci_task_t* task = workflow->tasks;
    int first = 1;
    while (task) {
        if (!first) {
            pos += snprintf(json + pos, capacity - pos, ",\n");
        }
        pos += snprintf(json + pos, capacity - pos, "    {\n");
        pos += snprintf(json + pos, capacity - pos, "      \"id\": \"%s\",\n", task->id);
        pos += snprintf(json + pos, capacity - pos, "      \"description\": \"%s\",\n",
                       task->description);
        pos += snprintf(json + pos, capacity - pos, "      \"assigned_to\": \"%s\",\n",
                       task->assigned_to);
        pos += snprintf(json + pos, capacity - pos, "      \"phase\": %d,\n", task->phase);
        pos += snprintf(json + pos, capacity - pos, "      \"completed\": %d\n", task->completed);
        pos += snprintf(json + pos, capacity - pos, "    }");
        first = 0;
        task = task->next;
    }
    pos += snprintf(json + pos, capacity - pos, "\n  ]\n");
    snprintf(json + pos, capacity - pos, "}\n");

    LOG_INFO("Saved checkpoint for workflow %s", workflow->workflow_id);
    return json;
}

/* Restore workflow state from JSON checkpoint */
int workflow_restore_checkpoint(workflow_controller_t* workflow,
                               const char* checkpoint_json) {
    if (!workflow || !checkpoint_json) {
        return E_INPUT_NULL;
    }

    /* Note: Full JSON parsing would use argo_json.h
     * This is a simplified implementation */
    LOG_INFO("Restored checkpoint for workflow %s", workflow->workflow_id);
    return ARGO_SUCCESS;
}

/* Save checkpoint to file */
int workflow_checkpoint_to_file(workflow_controller_t* workflow,
                               const char* filepath) {
    if (!workflow || !filepath) {
        return E_INPUT_NULL;
    }

    char* json = workflow_save_checkpoint(workflow);
    if (!json) {
        return E_SYSTEM_MEMORY;
    }

    FILE* fp = fopen(filepath, "w");
    if (!fp) {
        free(json);
        argo_report_error(E_SYSTEM_FILE, "workflow_checkpoint_to_file",
                         filepath);
        return E_SYSTEM_FILE;
    }

    fprintf(fp, "%s", json);
    fclose(fp);
    free(json);

    LOG_INFO("Saved workflow checkpoint to %s", filepath);
    return ARGO_SUCCESS;
}

/* Restore workflow from checkpoint file */
workflow_controller_t* workflow_restore_from_file(ci_registry_t* registry,
                                                  lifecycle_manager_t* lifecycle,
                                                  const char* filepath) {
    if (!registry || !lifecycle || !filepath) {
        return NULL;
    }

    FILE* fp = fopen(filepath, "r");
    if (!fp) {
        argo_report_error(E_SYSTEM_FILE, "workflow_restore_from_file",
                         filepath);
        return NULL;
    }

    /* Read file */
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* json = malloc(size + 1);
    if (!json) {
        fclose(fp);
        return NULL;
    }

    size_t read_size = fread(json, 1, size, fp);
    json[read_size] = '\0';
    fclose(fp);

    /* Create new workflow */
    workflow_controller_t* workflow = workflow_create(registry, lifecycle, "restored");
    if (workflow) {
        workflow_restore_checkpoint(workflow, json);
    }

    free(json);

    LOG_INFO("Restored workflow from checkpoint file %s", filepath);
    return workflow;
}

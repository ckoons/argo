/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Project includes */
#include "argo_lifecycle.h"
#include "argo_error.h"
#include "argo_error_messages.h"
#include "argo_log.h"

#define DEFAULT_HEARTBEAT_TIMEOUT 60
#define DEFAULT_MAX_MISSED 3
#define INITIAL_CAPACITY 16

/* Status name strings for display */
static const char* STATUS_NAMES[] = {
    "OFFLINE", "STARTING", "READY", "BUSY", "ERROR", "SHUTDOWN"
};

/* Create lifecycle manager */
lifecycle_manager_t* lifecycle_manager_create(ci_registry_t* registry) {
    lifecycle_manager_t* manager = calloc(1, sizeof(lifecycle_manager_t));
    if (!manager) {
        argo_report_error(E_SYSTEM_MEMORY, "lifecycle_manager_create", ERR_MSG_MEMORY_ALLOC_FAILED);
        return NULL;
    }

    manager->cis = calloc(INITIAL_CAPACITY, sizeof(ci_lifecycle_t*));
    if (!manager->cis) {
        free(manager);
        return NULL;
    }

    manager->count = 0;
    manager->capacity = INITIAL_CAPACITY;
    manager->registry = registry;
    manager->heartbeat_timeout_seconds = DEFAULT_HEARTBEAT_TIMEOUT;
    manager->max_missed_heartbeats = DEFAULT_MAX_MISSED;
    manager->auto_restart_on_error = false;

    LOG_INFO("Lifecycle manager created");
    return manager;
}

/* Destroy lifecycle manager */
void lifecycle_manager_destroy(lifecycle_manager_t* manager) {
    if (!manager) return;

    for (int i = 0; i < manager->count; i++) {
        if (manager->cis[i]) {
            lifecycle_clear_history(manager->cis[i]);
            free(manager->cis[i]->current_task);
            free(manager->cis[i]->last_error);
            free(manager->cis[i]);
        }
    }

    free(manager->cis);
    free(manager);
}

/* Find CI lifecycle by name */
static ci_lifecycle_t* find_ci_lifecycle(lifecycle_manager_t* manager,
                                        const char* ci_name) {
    for (int i = 0; i < manager->count; i++) {
        if (manager->cis[i] && strcmp(manager->cis[i]->ci_name, ci_name) == 0) {
            return manager->cis[i];
        }
    }
    return NULL;
}

/* Add transition to history */
static int add_transition(ci_lifecycle_t* ci,
                         lifecycle_event_t event,
                         ci_status_t from_status,
                         ci_status_t to_status,
                         const char* reason) {
    lifecycle_transition_t* trans = calloc(1, sizeof(lifecycle_transition_t));
    if (!trans) {
        return E_SYSTEM_MEMORY;
    }

    trans->timestamp = time(NULL);
    trans->from_status = from_status;
    trans->to_status = to_status;
    trans->event = event;
    trans->reason = reason ? strdup(reason) : NULL;

    /* Add to front of list */
    trans->next = ci->transitions;
    ci->transitions = trans;
    ci->transition_count++;

    return ARGO_SUCCESS;
}

/* Create CI lifecycle */
int lifecycle_create_ci(lifecycle_manager_t* manager,
                       const char* ci_name,
                       const char* role,
                       const char* model) {
    ARGO_CHECK_NULL(manager);
    ARGO_CHECK_NULL(ci_name);
    ARGO_CHECK_NULL(role);
    ARGO_CHECK_NULL(model);

    /* Check if already exists */
    ci_lifecycle_t* existing = find_ci_lifecycle(manager, ci_name);
    if (existing) {
        argo_report_error(E_INPUT_INVALID, "lifecycle_create_ci", ERR_MSG_CI_ALREADY_EXISTS);
        return E_INPUT_INVALID;
    }

    /* Expand array if needed */
    if (manager->count >= manager->capacity) {
        int new_capacity = manager->capacity * 2;
        ci_lifecycle_t** new_cis = realloc(manager->cis,
                                          new_capacity * sizeof(ci_lifecycle_t*));
        if (!new_cis) {
            return E_SYSTEM_MEMORY;
        }
        manager->cis = new_cis;
        manager->capacity = new_capacity;
    }

    /* Create lifecycle */
    ci_lifecycle_t* ci = calloc(1, sizeof(ci_lifecycle_t));
    if (!ci) {
        return E_SYSTEM_MEMORY;
    }

    strncpy(ci->ci_name, ci_name, REGISTRY_NAME_MAX - 1);
    ci->current_status = CI_STATUS_OFFLINE;
    ci->created = time(NULL);
    ci->last_transition = ci->created;
    ci->heartbeat_interval_seconds = manager->heartbeat_timeout_seconds;

    /* Add to registry */
    int port = registry_allocate_port(manager->registry, role);
    if (port < 0) {
        free(ci);
        return E_PROTOCOL_QUEUE;
    }

    int result = registry_add_ci(manager->registry, ci_name, role, model, port);
    if (result != ARGO_SUCCESS) {
        free(ci);
        return result;
    }

    /* Add transition */
    add_transition(ci, LIFECYCLE_EVENT_CREATED,
                  CI_STATUS_OFFLINE, CI_STATUS_OFFLINE, "Created");

    manager->cis[manager->count++] = ci;

    LOG_INFO("Created CI lifecycle: %s (role=%s, model=%s)", ci_name, role, model);
    return ARGO_SUCCESS;
}

/* Start CI */
int lifecycle_start_ci(lifecycle_manager_t* manager, const char* ci_name) {
    ARGO_CHECK_NULL(manager);
    ARGO_CHECK_NULL(ci_name);

    ci_lifecycle_t* ci = find_ci_lifecycle(manager, ci_name);
    if (!ci) {
        return E_INPUT_INVALID;
    }

    if (ci->current_status != CI_STATUS_OFFLINE) {
        LOG_WARN("CI %s already started (status=%d)", ci_name, ci->current_status);
        return ARGO_SUCCESS;
    }

    /* Transition to STARTING */
    ci_status_t old_status = ci->current_status;
    ci->current_status = CI_STATUS_STARTING;
    ci->last_transition = time(NULL);

    add_transition(ci, LIFECYCLE_EVENT_INITIALIZING,
                  old_status, ci->current_status, "Starting");

    registry_update_status(manager->registry, ci_name, CI_STATUS_STARTING);

    LOG_INFO("Starting CI: %s", ci_name);
    return ARGO_SUCCESS;
}

/* Stop CI */
int lifecycle_stop_ci(lifecycle_manager_t* manager,
                     const char* ci_name,
                     bool graceful) {
    ARGO_CHECK_NULL(manager);
    ARGO_CHECK_NULL(ci_name);

    ci_lifecycle_t* ci = find_ci_lifecycle(manager, ci_name);
    if (!ci) {
        return E_INPUT_INVALID;
    }

    ci_status_t old_status = ci->current_status;

    if (graceful) {
        /* Graceful shutdown - wait for current task */
        ci->current_status = CI_STATUS_SHUTDOWN;
        add_transition(ci, LIFECYCLE_EVENT_SHUTDOWN_REQ,
                      old_status, ci->current_status, "Graceful shutdown requested");
    } else {
        /* Immediate shutdown */
        ci->current_status = CI_STATUS_OFFLINE;
        add_transition(ci, LIFECYCLE_EVENT_TERMINATED,
                      old_status, ci->current_status, "Forced shutdown");
    }

    ci->last_transition = time(NULL);
    registry_update_status(manager->registry, ci_name, ci->current_status);

    LOG_INFO("Stopping CI: %s (%s)", ci_name, graceful ? "graceful" : "forced");
    return ARGO_SUCCESS;
}

/* Restart CI */
int lifecycle_restart_ci(lifecycle_manager_t* manager, const char* ci_name) {
    ARGO_CHECK_NULL(manager);
    ARGO_CHECK_NULL(ci_name);

    int result = lifecycle_stop_ci(manager, ci_name, true);
    if (result != ARGO_SUCCESS) {
        return result;
    }

    /* Wait a moment (in real implementation) */
    return lifecycle_start_ci(manager, ci_name);
}

/* Transition to new state */
int lifecycle_transition(lifecycle_manager_t* manager,
                        const char* ci_name,
                        lifecycle_event_t event,
                        const char* reason) {
    ARGO_CHECK_NULL(manager);
    ARGO_CHECK_NULL(ci_name);

    ci_lifecycle_t* ci = find_ci_lifecycle(manager, ci_name);
    if (!ci) {
        return E_INPUT_INVALID;
    }

    ci_status_t old_status = ci->current_status;
    ci_status_t new_status = old_status;

    /* Determine new status based on event */
    switch (event) {
        case LIFECYCLE_EVENT_INITIALIZING:
            new_status = CI_STATUS_STARTING;
            break;
        case LIFECYCLE_EVENT_READY:
            new_status = CI_STATUS_READY;
            break;
        case LIFECYCLE_EVENT_TASK_ASSIGNED:
            new_status = CI_STATUS_BUSY;
            break;
        case LIFECYCLE_EVENT_TASK_COMPLETE:
            new_status = CI_STATUS_READY;
            break;
        case LIFECYCLE_EVENT_ERROR:
            new_status = CI_STATUS_ERROR;
            break;
        case LIFECYCLE_EVENT_SHUTDOWN_REQ:
        case LIFECYCLE_EVENT_SHUTDOWN:
            new_status = CI_STATUS_SHUTDOWN;
            break;
        case LIFECYCLE_EVENT_TERMINATED:
            new_status = CI_STATUS_OFFLINE;
            break;
        default:
            break;
    }

    if (new_status != old_status) {
        ci->current_status = new_status;
        ci->last_transition = time(NULL);
        registry_update_status(manager->registry, ci_name, new_status);

        add_transition(ci, event, old_status, new_status, reason);
        LOG_INFO("CI %s: %d → %d (event=%d)", ci_name, old_status, new_status, event);
    }

    return ARGO_SUCCESS;
}

/* Assign task */
int lifecycle_assign_task(lifecycle_manager_t* manager,
                         const char* ci_name,
                         const char* task_description) {
    ARGO_CHECK_NULL(manager);
    ARGO_CHECK_NULL(ci_name);

    ci_lifecycle_t* ci = find_ci_lifecycle(manager, ci_name);
    if (!ci) {
        return E_INPUT_INVALID;
    }

    if (ci->current_status != CI_STATUS_READY) {
        LOG_WARN("CI %s not ready for task (status=%d)", ci_name, ci->current_status);
        return E_CI_INVALID;
    }

    free(ci->current_task);
    ci->current_task = task_description ? strdup(task_description) : NULL;
    ci->task_start_time = time(NULL);

    return lifecycle_transition(manager, ci_name,
                               LIFECYCLE_EVENT_TASK_ASSIGNED,
                               task_description);
}

/* Complete task */
int lifecycle_complete_task(lifecycle_manager_t* manager,
                           const char* ci_name,
                           bool success) {
    ARGO_CHECK_NULL(manager);
    ARGO_CHECK_NULL(ci_name);

    ci_lifecycle_t* ci = find_ci_lifecycle(manager, ci_name);
    if (!ci) {
        return E_INPUT_INVALID;
    }

    free(ci->current_task);
    ci->current_task = NULL;

    return lifecycle_transition(manager, ci_name,
                               LIFECYCLE_EVENT_TASK_COMPLETE,
                               success ? "Task completed successfully" : "Task failed");
}

/* Record heartbeat */
int lifecycle_heartbeat(lifecycle_manager_t* manager, const char* ci_name) {
    ARGO_CHECK_NULL(manager);
    ARGO_CHECK_NULL(ci_name);

    ci_lifecycle_t* ci = find_ci_lifecycle(manager, ci_name);
    if (!ci) {
        return E_INPUT_INVALID;
    }

    ci->last_heartbeat = time(NULL);
    ci->missed_heartbeats = 0;

    registry_heartbeat(manager->registry, ci_name);
    return ARGO_SUCCESS;
}

/* Check all heartbeats */
int lifecycle_check_heartbeats(lifecycle_manager_t* manager) {
    ARGO_CHECK_NULL(manager);

    time_t now = time(NULL);
    int stale_count = 0;

    for (int i = 0; i < manager->count; i++) {
        ci_lifecycle_t* ci = manager->cis[i];
        if (ci->current_status == CI_STATUS_OFFLINE) {
            continue;
        }

        time_t age = now - ci->last_heartbeat;
        if (age > manager->heartbeat_timeout_seconds) {
            ci->missed_heartbeats++;
            LOG_WARN("CI %s heartbeat stale (%lds ago, missed=%d)",
                    ci->ci_name, (long)age, ci->missed_heartbeats);
            stale_count++;

            if (ci->missed_heartbeats >= manager->max_missed_heartbeats) {
                lifecycle_report_error(manager, ci->ci_name,
                                      "Max missed heartbeats exceeded");
            }
        }
    }

    return stale_count;
}

/* Report error */
int lifecycle_report_error(lifecycle_manager_t* manager,
                          const char* ci_name,
                          const char* error_message) {
    ARGO_CHECK_NULL(manager);
    ARGO_CHECK_NULL(ci_name);

    ci_lifecycle_t* ci = find_ci_lifecycle(manager, ci_name);
    if (!ci) {
        return E_INPUT_INVALID;
    }

    ci->error_count++;
    free(ci->last_error);
    ci->last_error = error_message ? strdup(error_message) : NULL;

    argo_report_error(E_CI_INVALID, ci_name, "%s", error_message ? error_message : "unknown");

    return lifecycle_transition(manager, ci_name,
                               LIFECYCLE_EVENT_ERROR,
                               error_message);
}

/* Get CI lifecycle */
ci_lifecycle_t* lifecycle_get_ci(lifecycle_manager_t* manager,
                                const char* ci_name) {
    if (!manager || !ci_name) return NULL;
    return find_ci_lifecycle(manager, ci_name);
}

/* Get transition history */
lifecycle_transition_t* lifecycle_get_history(lifecycle_manager_t* manager,
                                             const char* ci_name) {
    ci_lifecycle_t* ci = lifecycle_get_ci(manager, ci_name);
    return ci ? ci->transitions : NULL;
}

/* Health check */
int lifecycle_health_check(lifecycle_manager_t* manager) {
    ARGO_CHECK_NULL(manager);

    int unhealthy = 0;

    for (int i = 0; i < manager->count; i++) {
        ci_lifecycle_t* ci = manager->cis[i];

        if (ci->current_status == CI_STATUS_ERROR) {
            unhealthy++;
        }

        if (ci->missed_heartbeats > 0) {
            unhealthy++;
        }
    }

    return unhealthy;
}

/* Print status */
void lifecycle_print_status(lifecycle_manager_t* manager) {
    if (!manager) return;

    printf("\n");
    printf("Lifecycle Manager Status: %d CIs\n", manager->count);
    printf("=========================================\n");

    for (int i = 0; i < manager->count; i++) {
        lifecycle_print_ci(manager->cis[i]);
    }
}

/* Print CI */
void lifecycle_print_ci(ci_lifecycle_t* ci) {
    if (!ci) return;

    printf("  %s: %s\n", ci->ci_name, STATUS_NAMES[ci->current_status]);
    printf("    Transitions: %d\n", ci->transition_count);
    printf("    Errors: %d\n", ci->error_count);

    if (ci->current_task) {
        printf("    Current task: %s\n", ci->current_task);
    }

    if (ci->last_error) {
        printf("    Last error: %s\n", ci->last_error);
    }

    printf("\n");
}

/* Print timeline */
void lifecycle_print_timeline(ci_lifecycle_t* ci) {
    if (!ci) return;

    printf("\n");
    printf("Lifecycle Timeline: %s\n", ci->ci_name);
    printf("=========================================\n");

    lifecycle_transition_t* trans = ci->transitions;
    while (trans) {
        char time_str[LIFECYCLE_TIME_BUFFER_SIZE];
        struct tm* tm_info = localtime(&trans->timestamp);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

        printf("  %s  %s → %s",
               time_str,
               STATUS_NAMES[trans->from_status],
               STATUS_NAMES[trans->to_status]);

        if (trans->reason) {
            printf("  (%s)", trans->reason);
        }

        printf("\n");
        trans = trans->next;
    }

    printf("\n");
}

/* Clear history */
void lifecycle_clear_history(ci_lifecycle_t* ci) {
    if (!ci) return;

    lifecycle_transition_t* trans = ci->transitions;
    while (trans) {
        lifecycle_transition_t* next = trans->next;
        free(trans->reason);
        free(trans);
        trans = next;
    }

    ci->transitions = NULL;
    ci->transition_count = 0;
}
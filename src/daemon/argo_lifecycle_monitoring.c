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

/* Forward declaration of internal function from argo_lifecycle.c */
extern ci_lifecycle_t* lifecycle_find_ci_internal(lifecycle_manager_t* manager, const char* ci_name);
extern int lifecycle_transition_internal(lifecycle_manager_t* manager, const char* ci_name, lifecycle_event_t event, const char* reason);

/* Assign task */
int lifecycle_assign_task(lifecycle_manager_t* manager,
                         const char* ci_name,
                         const char* task_description) {
    int result = ARGO_SUCCESS;
    char* new_task = NULL;

    ARGO_CHECK_NULL(manager);
    ARGO_CHECK_NULL(ci_name);

    ci_lifecycle_t* ci = lifecycle_find_ci_internal(manager, ci_name);
    if (!ci) {
        return E_INPUT_INVALID;
    }

    if (ci->current_status != CI_STATUS_READY) {
        LOG_WARN("CI %s not ready for task (status=%d)", ci_name, ci->current_status);
        return E_CI_INVALID;
    }

    if (task_description) {
        new_task = strdup(task_description);
        if (!new_task) {
            result = E_SYSTEM_MEMORY;
            goto cleanup;
        }
    }

    free(ci->current_task);
    ci->current_task = new_task;
    new_task = NULL;  /* Transfer ownership */
    ci->task_start_time = time(NULL);

    return lifecycle_transition_internal(manager, ci_name,
                               LIFECYCLE_EVENT_TASK_ASSIGNED,
                               task_description);

cleanup:
    free(new_task);
    return result;
}

/* Complete task */
int lifecycle_complete_task(lifecycle_manager_t* manager,
                           const char* ci_name,
                           bool success) {
    ARGO_CHECK_NULL(manager);
    ARGO_CHECK_NULL(ci_name);

    ci_lifecycle_t* ci = lifecycle_find_ci_internal(manager, ci_name);
    if (!ci) {
        return E_INPUT_INVALID;
    }

    free(ci->current_task);
    ci->current_task = NULL;

    /* GUIDELINE_APPROVED - Task completion status strings */
    return lifecycle_transition_internal(manager, ci_name,
                               LIFECYCLE_EVENT_TASK_COMPLETE,
                               success ? "Task completed successfully" : "Task failed");
    /* GUIDELINE_APPROVED_END */
}

/* Record heartbeat */
int lifecycle_heartbeat(lifecycle_manager_t* manager, const char* ci_name) {
    ARGO_CHECK_NULL(manager);
    ARGO_CHECK_NULL(ci_name);

    ci_lifecycle_t* ci = lifecycle_find_ci_internal(manager, ci_name);
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

            /* GUIDELINE_APPROVED - Lifecycle monitoring error message */
            if (ci->missed_heartbeats >= manager->max_missed_heartbeats) {
                lifecycle_report_error(manager, ci->ci_name,
                                      "Max missed heartbeats exceeded");
            }
            /* GUIDELINE_APPROVED_END */
        }
    }

    return stale_count;
}

/* Report error */
int lifecycle_report_error(lifecycle_manager_t* manager,
                          const char* ci_name,
                          const char* error_message) {
    int result = ARGO_SUCCESS;
    char* new_error = NULL;

    ARGO_CHECK_NULL(manager);
    ARGO_CHECK_NULL(ci_name);

    ci_lifecycle_t* ci = lifecycle_find_ci_internal(manager, ci_name);
    if (!ci) {
        return E_INPUT_INVALID;
    }

    if (error_message) {
        new_error = strdup(error_message);
        if (!new_error) {
            result = E_SYSTEM_MEMORY;
            goto cleanup;
        }
    }

    ci->error_count++;
    free(ci->last_error);
    ci->last_error = new_error;
    new_error = NULL;  /* Transfer ownership */

    argo_report_error(E_CI_INVALID, ci_name, "%s", error_message ? error_message : "unknown");

    return lifecycle_transition_internal(manager, ci_name,
                               LIFECYCLE_EVENT_ERROR,
                               error_message);

cleanup:
    free(new_error);
    return result;
}

/* Get CI lifecycle */
ci_lifecycle_t* lifecycle_get_ci(lifecycle_manager_t* manager,
                                const char* ci_name) {
    if (!manager || !ci_name) return NULL;
    return lifecycle_find_ci_internal(manager, ci_name);
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

/* GUIDELINE_APPROVED - Lifecycle status display formatting */
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

/* Status name strings for display */
static const char* STATUS_NAMES[] = {
    "OFFLINE", "STARTING", "READY", "BUSY", "ERROR", "SHUTDOWN"
};

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
/* GUIDELINE_APPROVED_END */

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

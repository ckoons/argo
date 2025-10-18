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

    /* Future work: Optional graceful shutdown tracking (not required for daemon model) */
    /* argo_register_lifecycle(manager); */

    LOG_INFO("Lifecycle manager created");
    return manager;
}

/* Destroy lifecycle manager */
void lifecycle_manager_destroy(lifecycle_manager_t* manager) {
    if (!manager) return;

    /* Future work: Optional shutdown tracking cleanup (not required for daemon model) */
    /* argo_unregister_lifecycle(manager); */

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

/* Find CI lifecycle by name (internal helper, exposed for monitoring module) */
ci_lifecycle_t* lifecycle_find_ci_internal(lifecycle_manager_t* manager,
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
    int result = ARGO_SUCCESS;
    lifecycle_transition_t* trans = NULL;

    trans = calloc(1, sizeof(lifecycle_transition_t));
    if (!trans) {
        result = E_SYSTEM_MEMORY;
        goto cleanup;
    }

    trans->timestamp = time(NULL);
    trans->from_status = from_status;
    trans->to_status = to_status;
    trans->event = event;

    if (reason) {
        trans->reason = strdup(reason);
        if (!trans->reason) {
            result = E_SYSTEM_MEMORY;
            goto cleanup;
        }
    }

    /* Add to front of list */
    trans->next = ci->transitions;
    ci->transitions = trans;
    ci->transition_count++;

    return ARGO_SUCCESS;

cleanup:
    if (trans) {
        free(trans->reason);
        free(trans);
    }
    return result;
}

/* Create CI lifecycle */
int lifecycle_create_ci(lifecycle_manager_t* manager,
                       const char* ci_name,
                       const char* role,
                       const char* model) {
    int result = ARGO_SUCCESS;
    ci_lifecycle_t* ci = NULL;

    ARGO_CHECK_NULL(manager);
    ARGO_CHECK_NULL(ci_name);
    ARGO_CHECK_NULL(role);
    ARGO_CHECK_NULL(model);

    /* Check if already exists */
    ci_lifecycle_t* existing = lifecycle_find_ci_internal(manager, ci_name);
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
    ci = calloc(1, sizeof(ci_lifecycle_t));
    if (!ci) {
        result = E_SYSTEM_MEMORY;
        goto cleanup;
    }

    strncpy(ci->ci_name, ci_name, REGISTRY_NAME_MAX - 1);
    ci->current_status = CI_STATUS_OFFLINE;
    ci->created = time(NULL);
    ci->last_transition = ci->created;
    ci->heartbeat_interval_seconds = manager->heartbeat_timeout_seconds;

    /* Add to registry */
    int port = registry_allocate_port(manager->registry, role);
    if (port < 0) {
        result = E_PROTOCOL_QUEUE;
        goto cleanup;
    }

    result = registry_add_ci(manager->registry, ci_name, role, model, port);
    if (result != ARGO_SUCCESS) {
        goto cleanup;
    }

    /* GUIDELINE_APPROVED - Lifecycle transition status messages */
    /* Add transition */
    add_transition(ci, LIFECYCLE_EVENT_CREATED,
                  CI_STATUS_OFFLINE, CI_STATUS_OFFLINE, "Created");

    manager->cis[manager->count++] = ci;

    LOG_INFO("Created CI lifecycle: %s (role=%s, model=%s)", ci_name, role, model);
    return ARGO_SUCCESS;

cleanup:
    free(ci);
    return result;
}

/* Start CI */
int lifecycle_start_ci(lifecycle_manager_t* manager, const char* ci_name) {
    ARGO_CHECK_NULL(manager);
    ARGO_CHECK_NULL(ci_name);

    ci_lifecycle_t* ci = lifecycle_find_ci_internal(manager, ci_name);
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

    ci_lifecycle_t* ci = lifecycle_find_ci_internal(manager, ci_name);
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
    /* GUIDELINE_APPROVED_END */

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

/* Transition to new state (internal helper, exposed for monitoring module) */
int lifecycle_transition_internal(lifecycle_manager_t* manager,
                        const char* ci_name,
                        lifecycle_event_t event,
                        const char* reason) {
    ARGO_CHECK_NULL(manager);
    ARGO_CHECK_NULL(ci_name);

    ci_lifecycle_t* ci = lifecycle_find_ci_internal(manager, ci_name);
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
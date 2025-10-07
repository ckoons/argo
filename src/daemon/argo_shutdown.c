/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>

/* Project includes */
#include "argo_shutdown.h"
#include "argo_workflow.h"
#include "argo_registry.h"
#include "argo_lifecycle.h"
#include "argo_shared_services.h"
#include "argo_workflow_registry.h"
#include "argo_init.h"
#include "argo_log.h"
#include "argo_limits.h"

/* Shutdown tracking structures */
static struct {
    workflow_controller_t* workflows[MAX_TRACKED_WORKFLOWS];
    int workflow_count;
    ci_registry_t* registries[MAX_TRACKED_REGISTRIES];
    int registry_count;
    lifecycle_manager_t* lifecycles[MAX_TRACKED_LIFECYCLES];
    int lifecycle_count;
    shared_services_t* shared_services;
    workflow_registry_t* workflow_registry;
    pthread_mutex_t mutex;
    int initialized;
} shutdown_tracker = {
    .workflow_count = 0,
    .registry_count = 0,
    .lifecycle_count = 0,
    .shared_services = NULL,
    .workflow_registry = NULL,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .initialized = 0
};

/* Signal handler for graceful shutdown */
static void signal_handler(int signum) {
    const char* signame = (signum == SIGTERM) ? "SIGTERM" : "SIGINT";
    LOG_INFO("Received %s - initiating graceful shutdown", signame);
    argo_exit();
    exit(0);
}

/* Register workflow for cleanup tracking */
void argo_register_workflow(workflow_controller_t* workflow) {
    if (!workflow) return;

    pthread_mutex_lock(&shutdown_tracker.mutex);
    if (shutdown_tracker.workflow_count < MAX_TRACKED_WORKFLOWS) {
        shutdown_tracker.workflows[shutdown_tracker.workflow_count++] = workflow;
        LOG_DEBUG("Registered workflow for cleanup tracking (%d active)",
                 shutdown_tracker.workflow_count);
    } else {
        LOG_WARN("Maximum workflows registered - cannot track for cleanup");
    }
    pthread_mutex_unlock(&shutdown_tracker.mutex);
}

/* Unregister workflow (called by workflow_destroy()) */
void argo_unregister_workflow(workflow_controller_t* workflow) {
    if (!workflow) return;

    pthread_mutex_lock(&shutdown_tracker.mutex);
    for (int i = 0; i < shutdown_tracker.workflow_count; i++) {
        if (shutdown_tracker.workflows[i] == workflow) {
            /* Shift remaining workflows down */
            for (int j = i; j < shutdown_tracker.workflow_count - 1; j++) {
                shutdown_tracker.workflows[j] = shutdown_tracker.workflows[j + 1];
            }
            shutdown_tracker.workflow_count--;
            LOG_DEBUG("Unregistered workflow (%d active)", shutdown_tracker.workflow_count);
            break;
        }
    }
    pthread_mutex_unlock(&shutdown_tracker.mutex);
}

/* Register registry for cleanup tracking */
void argo_register_registry(ci_registry_t* registry) {
    if (!registry) return;

    pthread_mutex_lock(&shutdown_tracker.mutex);
    if (shutdown_tracker.registry_count < MAX_TRACKED_REGISTRIES) {
        shutdown_tracker.registries[shutdown_tracker.registry_count++] = registry;
        LOG_DEBUG("Registered registry for cleanup tracking (%d active)",
                 shutdown_tracker.registry_count);
    } else {
        LOG_WARN("Maximum registries registered - cannot track for cleanup");
    }
    pthread_mutex_unlock(&shutdown_tracker.mutex);
}

/* Unregister registry (called by registry_destroy()) */
void argo_unregister_registry(ci_registry_t* registry) {
    if (!registry) return;

    pthread_mutex_lock(&shutdown_tracker.mutex);
    for (int i = 0; i < shutdown_tracker.registry_count; i++) {
        if (shutdown_tracker.registries[i] == registry) {
            /* Shift remaining registries down */
            for (int j = i; j < shutdown_tracker.registry_count - 1; j++) {
                shutdown_tracker.registries[j] = shutdown_tracker.registries[j + 1];
            }
            shutdown_tracker.registry_count--;
            LOG_DEBUG("Unregistered registry (%d active)", shutdown_tracker.registry_count);
            break;
        }
    }
    pthread_mutex_unlock(&shutdown_tracker.mutex);
}

/* Register lifecycle manager for cleanup tracking */
void argo_register_lifecycle(lifecycle_manager_t* lifecycle) {
    if (!lifecycle) return;

    pthread_mutex_lock(&shutdown_tracker.mutex);
    if (shutdown_tracker.lifecycle_count < MAX_TRACKED_LIFECYCLES) {
        shutdown_tracker.lifecycles[shutdown_tracker.lifecycle_count++] = lifecycle;
        LOG_DEBUG("Registered lifecycle for cleanup tracking (%d active)",
                 shutdown_tracker.lifecycle_count);
    } else {
        LOG_WARN("Maximum lifecycles registered - cannot track for cleanup");
    }
    pthread_mutex_unlock(&shutdown_tracker.mutex);
}

/* Unregister lifecycle manager (called by lifecycle_manager_destroy()) */
void argo_unregister_lifecycle(lifecycle_manager_t* lifecycle) {
    if (!lifecycle) return;

    pthread_mutex_lock(&shutdown_tracker.mutex);
    for (int i = 0; i < shutdown_tracker.lifecycle_count; i++) {
        if (shutdown_tracker.lifecycles[i] == lifecycle) {
            /* Shift remaining lifecycles down */
            for (int j = i; j < shutdown_tracker.lifecycle_count - 1; j++) {
                shutdown_tracker.lifecycles[j] = shutdown_tracker.lifecycles[j + 1];
            }
            shutdown_tracker.lifecycle_count--;
            LOG_DEBUG("Unregistered lifecycle (%d active)", shutdown_tracker.lifecycle_count);
            break;
        }
    }
    pthread_mutex_unlock(&shutdown_tracker.mutex);
}

/* Set shared services for cleanup tracking */
void argo_set_shared_services(shared_services_t* services) {
    pthread_mutex_lock(&shutdown_tracker.mutex);
    shutdown_tracker.shared_services = services;
    if (services) {
        LOG_DEBUG("Registered shared services for cleanup tracking");
    }
    pthread_mutex_unlock(&shutdown_tracker.mutex);
}

/* Set workflow registry for cleanup tracking */
void argo_set_workflow_registry(workflow_registry_t* registry) {
    pthread_mutex_lock(&shutdown_tracker.mutex);
    shutdown_tracker.workflow_registry = registry;
    if (registry) {
        LOG_DEBUG("Registered workflow registry for cleanup tracking");
    }
    pthread_mutex_unlock(&shutdown_tracker.mutex);
}

/* Cleanup all tracked objects (called by argo_exit()) */
void argo_shutdown_cleanup(void) {
    /* Copy tracked objects and clear counts while holding mutex */
    workflow_controller_t* workflows_to_cleanup[MAX_TRACKED_WORKFLOWS];
    lifecycle_manager_t* lifecycles_to_cleanup[MAX_TRACKED_LIFECYCLES];
    ci_registry_t* registries_to_cleanup[MAX_TRACKED_REGISTRIES];
    shared_services_t* shared_services_to_cleanup = NULL;
    workflow_registry_t* workflow_registry_to_cleanup = NULL;
    int workflow_count, lifecycle_count, registry_count;

    pthread_mutex_lock(&shutdown_tracker.mutex);

    /* Copy shared services pointer and clear tracker */
    shared_services_to_cleanup = shutdown_tracker.shared_services;
    shutdown_tracker.shared_services = NULL;

    /* Copy workflow registry pointer and clear tracker */
    workflow_registry_to_cleanup = shutdown_tracker.workflow_registry;
    shutdown_tracker.workflow_registry = NULL;

    /* Copy workflow pointers and clear tracker */
    workflow_count = shutdown_tracker.workflow_count;
    for (int i = 0; i < workflow_count; i++) {
        workflows_to_cleanup[i] = shutdown_tracker.workflows[i];
    }
    shutdown_tracker.workflow_count = 0;

    /* Copy lifecycle pointers and clear tracker */
    lifecycle_count = shutdown_tracker.lifecycle_count;
    for (int i = 0; i < lifecycle_count; i++) {
        lifecycles_to_cleanup[i] = shutdown_tracker.lifecycles[i];
    }
    shutdown_tracker.lifecycle_count = 0;

    /* Copy registry pointers and clear tracker */
    registry_count = shutdown_tracker.registry_count;
    for (int i = 0; i < registry_count; i++) {
        registries_to_cleanup[i] = shutdown_tracker.registries[i];
    }
    shutdown_tracker.registry_count = 0;

    pthread_mutex_unlock(&shutdown_tracker.mutex);

    /* Now destroy objects without holding mutex (avoids deadlock) */

    /* Cleanup workflow registry first (saves any pending changes) */
    if (workflow_registry_to_cleanup) {
        LOG_INFO("Cleaning up workflow registry");
        workflow_registry_destroy(workflow_registry_to_cleanup);
    }

    /* Cleanup shared services (stops background thread) */
    if (shared_services_to_cleanup) {
        LOG_INFO("Stopping shared services background thread");
        shared_services_stop(shared_services_to_cleanup);
        shared_services_destroy(shared_services_to_cleanup);
    }

    /* Cleanup workflows */
    if (workflow_count > 0) {
        LOG_INFO("Cleaning up %d active workflows", workflow_count);
        for (int i = workflow_count - 1; i >= 0; i--) {
            workflow_destroy(workflows_to_cleanup[i]);
        }
    }

    /* Cleanup lifecycle managers */
    if (lifecycle_count > 0) {
        LOG_INFO("Cleaning up %d active lifecycle managers", lifecycle_count);
        for (int i = lifecycle_count - 1; i >= 0; i--) {
            lifecycle_manager_destroy(lifecycles_to_cleanup[i]);
        }
    }

    /* Cleanup registries */
    if (registry_count > 0) {
        LOG_INFO("Cleaning up %d active registries", registry_count);
        for (int i = registry_count - 1; i >= 0; i--) {
            registry_destroy(registries_to_cleanup[i]);
        }
    }
}

/* Install signal handlers for graceful shutdown */
void argo_install_signal_handlers(void) {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        LOG_WARN("Failed to install SIGTERM handler");
    } else {
        LOG_DEBUG("Installed SIGTERM handler for graceful shutdown");
    }

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        LOG_WARN("Failed to install SIGINT handler");
    } else {
        LOG_DEBUG("Installed SIGINT handler for graceful shutdown");
    }

    shutdown_tracker.initialized = 1;
}

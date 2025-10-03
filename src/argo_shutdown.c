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
#include "argo_init.h"
#include "argo_log.h"

/* Maximum tracked objects */
#define MAX_WORKFLOWS 32
#define MAX_REGISTRIES 8
#define MAX_LIFECYCLES 8

/* Shutdown tracking structures */
static struct {
    workflow_controller_t* workflows[MAX_WORKFLOWS];
    int workflow_count;
    ci_registry_t* registries[MAX_REGISTRIES];
    int registry_count;
    lifecycle_manager_t* lifecycles[MAX_LIFECYCLES];
    int lifecycle_count;
    pthread_mutex_t mutex;
    int initialized;
} shutdown_tracker = {
    .workflow_count = 0,
    .registry_count = 0,
    .lifecycle_count = 0,
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
    if (shutdown_tracker.workflow_count < MAX_WORKFLOWS) {
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
    if (shutdown_tracker.registry_count < MAX_REGISTRIES) {
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
    if (shutdown_tracker.lifecycle_count < MAX_LIFECYCLES) {
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

/* Cleanup all tracked objects (called by argo_exit()) */
void argo_shutdown_cleanup(void) {
    pthread_mutex_lock(&shutdown_tracker.mutex);

    /* Cleanup in reverse order: workflows, lifecycles, registries */

    /* Cleanup workflows */
    if (shutdown_tracker.workflow_count > 0) {
        LOG_INFO("Cleaning up %d active workflows", shutdown_tracker.workflow_count);
        for (int i = shutdown_tracker.workflow_count - 1; i >= 0; i--) {
            workflow_destroy(shutdown_tracker.workflows[i]);
            shutdown_tracker.workflows[i] = NULL;
        }
        shutdown_tracker.workflow_count = 0;
    }

    /* Cleanup lifecycle managers */
    if (shutdown_tracker.lifecycle_count > 0) {
        LOG_INFO("Cleaning up %d active lifecycle managers", shutdown_tracker.lifecycle_count);
        for (int i = shutdown_tracker.lifecycle_count - 1; i >= 0; i--) {
            lifecycle_manager_destroy(shutdown_tracker.lifecycles[i]);
            shutdown_tracker.lifecycles[i] = NULL;
        }
        shutdown_tracker.lifecycle_count = 0;
    }

    /* Cleanup registries */
    if (shutdown_tracker.registry_count > 0) {
        LOG_INFO("Cleaning up %d active registries", shutdown_tracker.registry_count);
        for (int i = shutdown_tracker.registry_count - 1; i >= 0; i--) {
            registry_destroy(shutdown_tracker.registries[i]);
            shutdown_tracker.registries[i] = NULL;
        }
        shutdown_tracker.registry_count = 0;
    }

    pthread_mutex_unlock(&shutdown_tracker.mutex);
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

/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>

/* Project includes */
#include "argo_init.h"
#include "argo_env_utils.h"
#include "argo_config.h"
#include "argo_globals.h"  /* Cleanup function declarations */
#include "argo_shutdown.h"
#include "argo_shared_services.h"
#include "argo_workflow_registry.h"
#include "argo_error.h"
#include "argo_log.h"
#include "argo_limits.h"

/* Registry path */
#define WORKFLOW_REGISTRY_PATH ".argo/workflows/registry/active_workflow_registry.json"

/* Periodic cleanup task - runs every 30 minutes */
static void periodic_workflow_cleanup(void* context) {
    workflow_registry_t* registry = (workflow_registry_t*)context;
    if (registry) {
        LOG_DEBUG("Running periodic workflow cleanup");
        workflow_registry_cleanup_dead_workflows(registry);
    }
}

/* Initialize Argo library */
int argo_init(void) {
    int result = ARGO_SUCCESS;

    LOG_INFO("Initializing Argo library");

    /* Step 1: Load environment */
    result = argo_loadenv();
    if (result != ARGO_SUCCESS) {
        argo_exit();  /* Cleanup previously initialized subsystems */
        return result;
    }

    /* Step 2: Load configuration */
    result = argo_config();
    if (result != ARGO_SUCCESS) {
        argo_exit();  /* Cleanup previously initialized subsystems */
        return result;
    }

    /* Step 3: Initialize shared services */
    shared_services_t* services = shared_services_create();
    if (!services) {
        LOG_ERROR("Failed to create shared services");
        argo_exit();
        return E_SYSTEM_MEMORY;
    }

    result = shared_services_start(services);
    if (result != ARGO_SUCCESS) {
        LOG_ERROR("Failed to start shared services");
        shared_services_destroy(services);
        argo_exit();
        return result;
    }

    /* Register for cleanup on shutdown */
    argo_set_shared_services(services);

    /* Step 4: Initialize workflow registry */
    workflow_registry_t* registry = workflow_registry_create(WORKFLOW_REGISTRY_PATH);
    if (!registry) {
        LOG_ERROR("Failed to create workflow registry");
        argo_exit();
        return E_SYSTEM_MEMORY;
    }

    /* Load existing workflows if file exists */
    result = workflow_registry_load(registry);
    if (result != ARGO_SUCCESS) {
        LOG_WARN("Failed to load workflow registry (file may not exist yet)");
        /* Not fatal - continue with empty registry */
    }

    /* Register for cleanup on shutdown */
    argo_set_workflow_registry(registry);

    /* Register periodic cleanup task */
    shared_services_register_task(services, periodic_workflow_cleanup, registry,
                                  WORKFLOW_CLEANUP_INTERVAL_SECONDS);

    LOG_INFO("Argo initialization complete");
    return ARGO_SUCCESS;
}

/* Cleanup Argo library */
void argo_exit(void) {
    LOG_INFO("Shutting down Argo library");

    /* Cleanup in reverse order of initialization */

    /* Shutdown tracked objects (workflows, registries, lifecycles) */
    argo_shutdown_cleanup();

    /* Config subsystem */
    argo_config_cleanup();

    /* Environment subsystem */
    argo_freeenv();

    LOG_DEBUG("Argo shutdown complete");
}

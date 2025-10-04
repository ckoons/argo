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
#include "argo_error.h"
#include "argo_log.h"

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

/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>

/* Project includes */
#include "argo_init.h"
#include "argo_env_utils.h"
#include "argo_config.h"
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

    /* TODO: Unix pivot - shared services and workflow registry initialization removed */
    /* These will be reintroduced after Unix workflow pivot is complete */

    LOG_INFO("Argo initialization complete");
    return ARGO_SUCCESS;
}

/* Cleanup Argo library */
void argo_exit(void) {
    LOG_INFO("Shutting down Argo library");

    /* Cleanup in reverse order of initialization */

    /* Config subsystem */
    argo_config_cleanup();

    /* Environment subsystem */
    argo_freeenv();

    LOG_DEBUG("Argo shutdown complete");
}

/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arc_context.h"
#include "arc_commands.h"

/* Get current workflow context from environment */
const char* arc_context_get(void) {
    return getenv(ARC_CONTEXT_ENV_VAR);
}

/* Get effective environment (--env flag, then ARC_ENV, then NULL for all) */
const char* arc_get_effective_environment(int argc, char** argv) {
    /* Check for --env flag */
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--env") == 0 && i + 1 < argc) {
            return argv[i + 1];
        }
    }

    /* Check ARC_ENV environment variable */
    const char* arc_env = getenv("ARC_ENV");
    if (arc_env && arc_env[0] != '\0') {
        return arc_env;
    }

    /* No filter - show all environments */
    return NULL;
}

/* Get environment for workflow creation (defaults to 'dev' if not set) */
const char* arc_get_environment_for_creation(int argc, char** argv) {
    const char* env = arc_get_effective_environment(argc, argv);
    return env ? env : "dev";
}

/* Set workflow context (outputs special directive for shell wrapper) */
void arc_context_set(const char* workflow_name) {
    if (!workflow_name) {
        return;
    }
    /* Shell wrapper will parse this and set environment variable */
    printf("ARGO_SET_ENV:%s\n", workflow_name);
}

/* Clear workflow context (outputs special directive for shell wrapper) */
void arc_context_clear(void) {
    /* Shell wrapper will parse this and unset environment variable */
    printf("ARGO_CLEAR_ENV\n");
}

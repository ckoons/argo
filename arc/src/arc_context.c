/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include "arc_context.h"

/* Get current workflow context from environment */
const char* arc_context_get(void) {
    return getenv(ARC_CONTEXT_ENV_VAR);
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

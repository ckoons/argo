/* © 2025 Casey Koons All rights reserved */

#ifndef ARC_CONTEXT_H
#define ARC_CONTEXT_H

/* Context environment variable name */
#define ARC_CONTEXT_ENV_VAR "ARGO_ACTIVE_WORKFLOW"

/* Get current workflow context from environment */
const char* arc_context_get(void);

/* Set workflow context (outputs special directive for shell wrapper) */
void arc_context_set(const char* workflow_name);

/* Clear workflow context (outputs special directive for shell wrapper) */
void arc_context_clear(void);

#endif /* ARC_CONTEXT_H */

/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_WORKFLOW_CHECKPOINT_H
#define ARGO_WORKFLOW_CHECKPOINT_H

#include "argo_workflow.h"
#include "argo_registry.h"
#include "argo_lifecycle.h"

/*
 * Workflow Checkpoint Management
 *
 * Handles saving and restoring workflow state for:
 * - Crash recovery
 * - Pause/resume workflows
 * - Workflow migration
 * - State debugging
 */

/**
 * Save workflow state to JSON checkpoint string.
 * Caller must free the returned string.
 *
 * @param workflow Workflow to save
 * @return JSON string (caller must free), or NULL on error
 */
char* workflow_save_checkpoint(workflow_controller_t* workflow);

/**
 * Restore workflow state from JSON checkpoint string.
 * Updates the workflow in-place with saved state.
 *
 * @param workflow Workflow to restore into
 * @param checkpoint_json JSON checkpoint string
 * @return ARGO_SUCCESS or error code
 */
int workflow_restore_checkpoint(workflow_controller_t* workflow,
                                const char* checkpoint_json);

/**
 * Save workflow checkpoint to file.
 *
 * @param workflow Workflow to save
 * @param filepath Path to checkpoint file
 * @return ARGO_SUCCESS or error code
 */
int workflow_checkpoint_to_file(workflow_controller_t* workflow,
                                const char* filepath);

/**
 * Restore workflow from checkpoint file.
 * Creates a new workflow instance from saved state.
 *
 * @param registry CI registry for restored workflow
 * @param lifecycle Lifecycle manager for restored workflow
 * @param filepath Path to checkpoint file
 * @return New workflow instance, or NULL on error
 */
workflow_controller_t* workflow_restore_from_file(ci_registry_t* registry,
                                                  lifecycle_manager_t* lifecycle,
                                                  const char* filepath);

#endif /* ARGO_WORKFLOW_CHECKPOINT_H */

/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_WORKFLOW_EXECUTOR_H
#define ARGO_WORKFLOW_EXECUTOR_H

/* Checkpoint and state paths */
#define CHECKPOINT_DIR ".argo/workflows/checkpoints"
#define CHECKPOINT_PATH_MAX 512
#define TEMPLATE_BUFFER_SIZE 8192
#define MAX_WORKFLOW_STEPS 32

/* JSON field name lengths */
#define JSON_FIELD_STEPS_LEN 5
#define JSON_FIELD_STEP_LEN 4
#define JSON_FIELD_TYPE_LEN 4
#define JSON_FIELD_PROMPT_LEN 6

/* JSON parsing constants */
#define JSON_CURRENT_STEP_FIELD "\"current_step\":"
#define JSON_CURRENT_STEP_OFFSET 15
#define JSON_TOTAL_STEPS_FIELD "\"total_steps\":"
#define JSON_TOTAL_STEPS_OFFSET 15
#define JSON_IS_PAUSED_FIELD "\"is_paused\": true"

/* Workflow execution timing */
#define STEP_EXECUTION_DELAY_SEC 2
#define PAUSE_POLL_DELAY_SEC 1

/* Buffer sizes for workflow structures */
#define WORKFLOW_ID_MAX 128
#define TEMPLATE_PATH_MAX 256
#define BRANCH_NAME_MAX 128
#define STEP_NAME_MAX 128
#define STEP_TYPE_MAX 64
#define STEP_PROMPT_MAX 512

#endif /* ARGO_WORKFLOW_EXECUTOR_H */

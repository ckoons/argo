/* © 2025 Casey Koons All rights reserved */

/* Test Harness: Workflow Executor
 *
 * Purpose: Test workflow execution with simple_test.json
 * Tests:
 *   - Load workflow JSON
 *   - Execute workflow steps
 *   - User interaction (user_ask)
 *   - Variable substitution (display)
 *   - File output (save_file)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* External library - header only for type definitions */
#define JSMN_HEADER
#include "jsmn.h"

#include "argo_init.h"
#include "argo_workflow_executor.h"
#include "argo_workflow_json.h"
#include "argo_workflow_steps.h"
#include "argo_error.h"

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("WORKFLOW EXECUTOR TEST\n");
    printf("========================================\n");
    printf("\n");

    /* Initialize Argo */
    if (argo_init() != ARGO_SUCCESS) {
        fprintf(stderr, "FAIL: argo_init() failed\n");
        return 1;
    }

    /* Load workflow */
    printf("Loading workflow: workflows/test/simple_test.json\n\n");
    const char* workflow_path = "workflows/test/simple_test.json";

    size_t json_size;
    char* json = workflow_json_load_file(workflow_path, &json_size);
    if (!json) {
        fprintf(stderr, "FAIL: Failed to load workflow file\n");
        argo_exit();
        return 1;
    }

    /* Parse JSON */
    jsmntok_t tokens[WORKFLOW_JSON_MAX_TOKENS];
    int token_count = workflow_json_parse(json, tokens, WORKFLOW_JSON_MAX_TOKENS);
    if (token_count < 0) {
        fprintf(stderr, "FAIL: Failed to parse workflow JSON\n");
        free(json);
        argo_exit();
        return 1;
    }

    /* Display workflow info */
    int name_idx = workflow_json_find_field(json, tokens, 0, WORKFLOW_JSON_FIELD_WORKFLOW_NAME);
    if (name_idx >= 0) {
        char name[EXECUTOR_NAME_BUFFER_SIZE];
        workflow_json_extract_string(json, &tokens[name_idx], name, sizeof(name));
        printf("Workflow: %s\n", name);
    }

    int desc_idx = workflow_json_find_field(json, tokens, 0, WORKFLOW_JSON_FIELD_DESCRIPTION);
    if (desc_idx >= 0) {
        char desc[STEP_SAVE_TO_BUFFER_SIZE];
        workflow_json_extract_string(json, &tokens[desc_idx], desc, sizeof(desc));
        printf("Description: %s\n", desc);
    }

    printf("\n");
    printf("----------------------------------------\n");
    printf("Starting Workflow Execution\n");
    printf("----------------------------------------\n");
    printf("\n");

    /* Execute workflow */
    int result = workflow_execute(json, tokens, token_count);

    printf("\n");
    printf("----------------------------------------\n");

    if (result == ARGO_SUCCESS) {
        printf("Workflow Execution: SUCCESS\n");
    } else {
        printf("Workflow Execution: FAILED (error: %d)\n", result);
    }

    printf("----------------------------------------\n");
    printf("\n");

    /* Cleanup */
    free(json);
    argo_exit();

    if (result == ARGO_SUCCESS) {
        printf("========================================\n");
        printf("WORKFLOW EXECUTOR TEST PASSED\n");
        printf("========================================\n");
        printf("\n");
        printf("Check /tmp/workflow_test.json for output\n");
        printf("\n");
        return 0;
    } else {
        printf("========================================\n");
        printf("WORKFLOW EXECUTOR TEST FAILED\n");
        printf("========================================\n");
        printf("\n");
        return 1;
    }
}

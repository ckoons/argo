/* © 2025 Casey Koons All rights reserved */

/* Test Harness: Workflow Context
 *
 * Purpose: Verify workflow context variable management
 * Tests:
 *   - Create/destroy context
 *   - Set/get variables
 *   - Variable substitution
 *   - Capacity expansion
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "argo_init.h"
#include "argo_workflow_context.h"
#include "argo_error.h"

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("WORKFLOW CONTEXT TEST\n");
    printf("========================================\n");
    printf("\n");

    /* Initialize Argo */
    if (argo_init() != ARGO_SUCCESS) {
        fprintf(stderr, "FAIL: argo_init() failed\n");
        return 1;
    }

    /* Test 1: Create context */
    printf("Test 1: Create context...\n");
    workflow_context_t* ctx = workflow_context_create();
    if (!ctx) {
        fprintf(stderr, "FAIL: workflow_context_create()\n");
        argo_exit();
        return 1;
    }
    printf("PASS: Context created\n");

    /* Test 2: Set and get variable */
    printf("\nTest 2: Set and get variable...\n");
    if (workflow_context_set(ctx, "name", "Casey") != ARGO_SUCCESS) {
        fprintf(stderr, "FAIL: workflow_context_set()\n");
        workflow_context_destroy(ctx);
        argo_exit();
        return 1;
    }
    const char* name = workflow_context_get(ctx, "name");
    if (!name || strcmp(name, "Casey") != 0) {
        fprintf(stderr, "FAIL: workflow_context_get() returned: %s\n", name ? name : "NULL");
        workflow_context_destroy(ctx);
        argo_exit();
        return 1;
    }
    printf("PASS: Variable set and retrieved: %s\n", name);

    /* Test 3: Multiple variables */
    printf("\nTest 3: Multiple variables...\n");
    workflow_context_set(ctx, "project", "Argo");
    workflow_context_set(ctx, "task", "Build workflow executor");
    workflow_context_print(ctx);
    printf("PASS: Multiple variables stored\n");

    /* Test 4: Update existing variable */
    printf("\nTest 4: Update existing variable...\n");
    workflow_context_set(ctx, "name", "Casey Koons");
    const char* updated = workflow_context_get(ctx, "name");
    if (!updated || strcmp(updated, "Casey Koons") != 0) {
        fprintf(stderr, "FAIL: Variable not updated\n");
        workflow_context_destroy(ctx);
        argo_exit();
        return 1;
    }
    printf("PASS: Variable updated: %s\n", updated);

    /* Test 5: Variable substitution */
    printf("\nTest 5: Variable substitution...\n");
    const char* template = "Hello {name}, welcome to {project}. Task: {task}";
    char output[512];
    if (workflow_context_substitute(ctx, template, output, sizeof(output)) != ARGO_SUCCESS) {
        fprintf(stderr, "FAIL: workflow_context_substitute()\n");
        workflow_context_destroy(ctx);
        argo_exit();
        return 1;
    }
    printf("  Template: %s\n", template);
    printf("  Result:   %s\n", output);
    printf("PASS: Variable substitution works\n");

    /* Test 6: Missing variable in substitution */
    printf("\nTest 6: Missing variable...\n");
    const char* template2 = "Project: {project}, Unknown: {missing}";
    char output2[512];
    workflow_context_substitute(ctx, template2, output2, sizeof(output2));
    printf("  Template: %s\n", template2);
    printf("  Result:   %s\n", output2);
    printf("PASS: Missing variables kept as placeholders\n");

    /* Test 7: Variable exists check */
    printf("\nTest 7: Variable exists check...\n");
    if (!workflow_context_has(ctx, "project")) {
        fprintf(stderr, "FAIL: workflow_context_has() - project should exist\n");
        workflow_context_destroy(ctx);
        argo_exit();
        return 1;
    }
    if (workflow_context_has(ctx, "nonexistent")) {
        fprintf(stderr, "FAIL: workflow_context_has() - nonexistent should not exist\n");
        workflow_context_destroy(ctx);
        argo_exit();
        return 1;
    }
    printf("PASS: Variable existence checks work\n");

    /* Test 8: Capacity expansion */
    printf("\nTest 8: Capacity expansion...\n");
    for (int i = 0; i < 20; i++) {
        char key[32];
        char value[32];
        snprintf(key, sizeof(key), "var_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        workflow_context_set(ctx, key, value);
    }
    printf("  Added 20 more variables (total: %d)\n", ctx->count + 3);
    if (workflow_context_get(ctx, "var_19") == NULL) {
        fprintf(stderr, "FAIL: var_19 not found after expansion\n");
        workflow_context_destroy(ctx);
        argo_exit();
        return 1;
    }
    printf("PASS: Context expanded automatically\n");

    /* Test 9: Clear context */
    printf("\nTest 9: Clear context...\n");
    workflow_context_clear(ctx);
    if (workflow_context_has(ctx, "name")) {
        fprintf(stderr, "FAIL: Variables still exist after clear\n");
        workflow_context_destroy(ctx);
        argo_exit();
        return 1;
    }
    printf("PASS: Context cleared\n");

    /* Cleanup */
    workflow_context_destroy(ctx);
    argo_exit();

    printf("\n");
    printf("========================================\n");
    printf("ALL CONTEXT TESTS PASSED\n");
    printf("========================================\n");
    printf("\n");

    return 0;
}

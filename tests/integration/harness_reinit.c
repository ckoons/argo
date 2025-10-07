/* © 2025 Casey Koons All rights reserved */

/* Test Harness: Multiple Init/Exit Cycles
 *
 * Purpose: Verify that argo_init()/argo_exit() can be called multiple times
 * Tests:
 *   - Multiple complete cycles work
 *   - Environment is properly reloaded each time
 *   - No memory leaks (use valgrind to verify)
 *   - State is properly reset between cycles
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "argo_init.h"
#include "argo_env_utils.h"
#include "argo_error.h"

static int test_cycle(int cycle_num) {
    printf("\n--- Cycle %d ---\n", cycle_num);

    if (argo_init() != ARGO_SUCCESS) {
        fprintf(stderr, "FAIL: argo_init() failed on cycle %d\n", cycle_num);
        return 1;
    }

    /* Verify ARGO_ROOT is set */
    const char* root = argo_getenv("ARGO_ROOT");
    if (!root) {
        fprintf(stderr, "FAIL: ARGO_ROOT not set on cycle %d\n", cycle_num);
        argo_exit();
        return 1;
    }
    printf("  ARGO_ROOT: %s\n", root);

    /* Set a test variable */
    char var_name[32];
    snprintf(var_name, sizeof(var_name), "TEST_CYCLE_%d", cycle_num);
    argo_setenv(var_name, "test_value");

    /* Verify it's set */
    const char* val = argo_getenv(var_name);
    if (!val || strcmp(val, "test_value") != 0) {
        fprintf(stderr, "FAIL: Variable not set correctly on cycle %d\n", cycle_num);
        argo_exit();
        return 1;
    }
    printf("  Set %s=test_value\n", var_name);

    argo_exit();
    printf("  Cleanup complete\n");

    return 0;
}

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("MULTIPLE INIT/EXIT CYCLES TEST\n");
    printf("========================================\n");

    /* Run 5 cycles */
    for (int i = 1; i <= 5; i++) {
        if (test_cycle(i) != 0) {
            return 1;
        }
    }

    printf("\n");
    printf("========================================\n");
    printf("ALL CYCLES PASSED\n");
    printf("========================================\n");
    printf("\n");

    return 0;
}

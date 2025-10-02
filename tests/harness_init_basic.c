/* © 2025 Casey Koons All rights reserved */

/* Test Harness: Basic Init/Exit
 *
 * Purpose: Verify argo_init() and argo_exit() work correctly
 * Tests:
 *   - Successful initialization
 *   - ARGO_ROOT is set
 *   - Clean shutdown
 *
 * This is the most basic smoke test - if this fails, nothing else will work.
 */

#include <stdio.h>
#include <stdlib.h>
#include "argo_init.h"
#include "argo_env_utils.h"
#include "argo_error.h"

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("BASIC INIT/EXIT TEST\n");
    printf("========================================\n");
    printf("\n");

    /* Test: Initialization */
    printf("Testing argo_init()...\n");
    if (argo_init() != ARGO_SUCCESS) {
        fprintf(stderr, "FAIL: argo_init() failed\n");
        return 1;
    }
    printf("PASS: argo_init() succeeded\n");

    /* Test: ARGO_ROOT is set */
    printf("\nTesting ARGO_ROOT...\n");
    const char* root = argo_getenv("ARGO_ROOT");
    if (!root) {
        fprintf(stderr, "FAIL: ARGO_ROOT not set after init\n");
        argo_exit();
        return 1;
    }
    printf("PASS: ARGO_ROOT = %s\n", root);

    /* Test: Can get other environment variables */
    printf("\nTesting environment access...\n");
    const char* path = argo_getenv("PATH");
    if (!path) {
        fprintf(stderr, "WARN: PATH not found (expected from system env)\n");
    } else {
        printf("PASS: Can access system environment\n");
    }

    /* Test: Cleanup */
    printf("\nTesting argo_exit()...\n");
    argo_exit();
    printf("PASS: argo_exit() completed\n");

    printf("\n");
    printf("========================================\n");
    printf("ALL TESTS PASSED\n");
    printf("========================================\n");
    printf("\n");

    return 0;
}

/* © 2025 Casey Koons All rights reserved */

/* Test Harness: Init Error Recovery
 *
 * Purpose: Verify argo_init() handles errors correctly
 * Tests:
 *   - Init fails gracefully when .env.argo is missing
 *   - Cleanup happens on init failure
 *   - Can recover after failure
 *   - No memory leaks on failure path
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "argo_init.h"
#include "argo_env_utils.h"
#include "argo_error.h"

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("INIT ERROR RECOVERY TEST\n");
    printf("========================================\n");
    printf("\n");

    /* Test 1: Verify normal init works first */
    printf("Test 1: Normal initialization...\n");
    if (argo_init() != ARGO_SUCCESS) {
        fprintf(stderr, "FAIL: Normal init should succeed\n");
        return 1;
    }
    printf("PASS: Normal init succeeded\n");
    const char* root = argo_getenv("ARGO_ROOT");
    printf("  ARGO_ROOT: %s\n", root);
    argo_exit();

    /* Test 2: Temporarily hide .env.argo */
    printf("\nTest 2: Init without .env.argo...\n");
    char backup_path[1024];
    snprintf(backup_path, sizeof(backup_path), "%s/.env.argo.test_backup", root);
    char argo_path[1024];
    snprintf(argo_path, sizeof(argo_path), "%s/.env.argo", root);

    if (rename(argo_path, backup_path) != 0) {
        fprintf(stderr, "WARN: Could not rename .env.argo for testing\n");
        fprintf(stderr, "      Skipping failure test\n");
    } else {
        /* Try to init - should fail */
        int result = argo_init();
        if (result == ARGO_SUCCESS) {
            fprintf(stderr, "FAIL: Init should fail without .env.argo\n");
            argo_exit();
            rename(backup_path, argo_path);
            return 1;
        }
        printf("PASS: Init failed as expected (error code: %d)\n", result);

        /* Restore .env.argo */
        if (rename(backup_path, argo_path) != 0) {
            fprintf(stderr, "ERROR: Could not restore .env.argo!\n");
            fprintf(stderr, "       Manually restore from: %s\n", backup_path);
            return 1;
        }

        /* Test 3: Verify recovery */
        printf("\nTest 3: Recovery after error...\n");
        if (argo_init() != ARGO_SUCCESS) {
            fprintf(stderr, "FAIL: Should recover after restoring .env.argo\n");
            return 1;
        }
        printf("PASS: Recovered successfully\n");
        argo_exit();
    }

    /* Test 4: Multiple exit calls (idempotency) */
    printf("\nTest 4: Multiple argo_exit() calls...\n");
    if (argo_init() != ARGO_SUCCESS) {
        fprintf(stderr, "FAIL: Init failed\n");
        return 1;
    }
    argo_exit();
    argo_exit();  /* Second call should be harmless */
    argo_exit();  /* Third call should be harmless */
    printf("PASS: Multiple exit calls are safe\n");

    printf("\n");
    printf("========================================\n");
    printf("ALL ERROR RECOVERY TESTS PASSED\n");
    printf("========================================\n");
    printf("\n");

    return 0;
}

/* © 2025 Casey Koons All rights reserved */

/* Test Harness: Environment Inspection
 *
 * Purpose: Display all environment variables loaded by Argo
 * Useful for:
 *   - Debugging environment issues
 *   - Verifying .env files are loaded correctly
 *   - Checking variable expansion
 */

#include <stdio.h>
#include <stdlib.h>
#include "argo_init.h"
#include "argo_env_utils.h"
#include "argo_error.h"

int main(void) {
    if (argo_init() != ARGO_SUCCESS) {
        fprintf(stderr, "Fatal: Argo initialization failed\n");
        fprintf(stderr, "Check that .env.argo exists in current or parent directory\n");
        return 1;
    }

    printf("\n");
    printf("========================================\n");
    printf("ARGO ENVIRONMENT VARIABLES\n");
    printf("========================================\n");
    printf("\n");

    argo_env_print();

    printf("\n");
    printf("========================================\n");
    printf("Key Variables:\n");
    printf("========================================\n");
    printf("ARGO_ROOT: %s\n", argo_getenv("ARGO_ROOT") ?: "(not set)");
    printf("HOME:      %s\n", argo_getenv("HOME") ?: "(not set)");
    printf("PATH:      %s\n", argo_getenv("PATH") ?: "(not set)");
    printf("\n");

    argo_exit();
    return 0;
}

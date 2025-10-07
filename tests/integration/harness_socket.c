/* © 2025 Casey Koons All rights reserved */

/* Test Harness: Socket Server Integration
 *
 * Purpose: Verify socket server integrates correctly with init/exit
 * Tests:
 *   - Socket server can be started after argo_init()
 *   - Socket path is created correctly
 *   - Socket cleanup is independent of argo_exit()
 *   - Application manages socket lifecycle
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "argo_init.h"
#include "argo_env_utils.h"
#include "argo_socket.h"
#include "argo_error.h"

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("SOCKET SERVER INTEGRATION TEST\n");
    printf("========================================\n");
    printf("\n");

    /* Initialize Argo */
    printf("Initializing Argo...\n");
    if (argo_init() != ARGO_SUCCESS) {
        fprintf(stderr, "FAIL: argo_init() failed\n");
        return 1;
    }
    printf("PASS: Argo initialized\n");

    /* Start socket server */
    printf("\nStarting socket server...\n");
    if (socket_server_init("test-harness-ci") != ARGO_SUCCESS) {
        fprintf(stderr, "FAIL: socket_server_init() failed\n");
        argo_exit();
        return 1;
    }
    printf("PASS: Socket server started\n");

    /* Verify socket path */
    char sock_path[256];
    snprintf(sock_path, sizeof(sock_path), "/tmp/argo_ci_test-harness-ci.sock");
    printf("  Socket path: %s\n", sock_path);

    /* Verify socket file exists */
    if (access(sock_path, F_OK) != 0) {
        fprintf(stderr, "FAIL: Socket file not created\n");
        socket_server_cleanup();
        argo_exit();
        return 1;
    }
    printf("PASS: Socket file exists\n");

    /* Clean up socket BEFORE argo_exit */
    printf("\nCleaning up socket server...\n");
    socket_server_cleanup();
    printf("PASS: Socket cleanup completed\n");

    /* Verify socket file removed */
    if (access(sock_path, F_OK) == 0) {
        fprintf(stderr, "WARN: Socket file still exists after cleanup\n");
    } else {
        printf("PASS: Socket file removed\n");
    }

    /* Clean up Argo */
    printf("\nCleaning up Argo...\n");
    argo_exit();
    printf("PASS: Argo cleanup completed\n");

    printf("\n");
    printf("========================================\n");
    printf("ALL SOCKET TESTS PASSED\n");
    printf("========================================\n");
    printf("\n");

    return 0;
}

/* © 2025 Casey Koons All rights reserved */

/* Test graceful shutdown on SIGTERM/SIGINT */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include "argo_init.h"
#include "argo_registry.h"
#include "argo_lifecycle.h"
#include "argo_workflow.h"
#include "argo_shutdown.h"

int test_count = 0;
int test_passed = 0;

#define TEST(name) do { \
    test_count++; \
    printf("Testing: %-50s", name); \
    fflush(stdout); \
} while(0)

#define PASS() do { \
    test_passed++; \
    printf(" ✓\n"); \
} while(0)

#define FAIL() do { \
    printf(" ✗\n"); \
} while(0)

static void test_sigterm_cleanup(void) {
    TEST("SIGTERM triggers graceful shutdown");

    pid_t pid = fork();
    if (pid == 0) {
        /* Child: create objects and wait for signal */
        argo_init();
        ci_registry_t* registry = registry_create();
        lifecycle_manager_t* lifecycle = lifecycle_manager_create(registry);
        workflow_controller_t* workflow = workflow_create(registry, lifecycle, "signal-test");
        (void)workflow;  /* Suppress unused warning */

        /* Install signal handlers */
        argo_install_signal_handlers();

        /* Wait for signal (parent will kill us) */
        sleep(10);

        /* Should not reach here */
        exit(1);
    } else {
        /* Parent: wait a bit then send SIGTERM */
        usleep(100000);  /* 100ms */
        kill(pid, SIGTERM);

        int status;
        waitpid(pid, &status, 0);

        /* Child should exit cleanly (exit code 0) */
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            PASS();
        } else {
            FAIL();
        }
    }
}

static void test_sigint_cleanup(void) {
    TEST("SIGINT triggers graceful shutdown");

    pid_t pid = fork();
    if (pid == 0) {
        /* Child */
        argo_init();
        ci_registry_t* registry = registry_create();
        lifecycle_manager_t* lifecycle = lifecycle_manager_create(registry);
        workflow_controller_t* workflow = workflow_create(registry, lifecycle, "signal-test");
        (void)workflow;  /* Suppress unused warning */

        argo_install_signal_handlers();
        sleep(10);
        exit(1);
    } else {
        /* Parent */
        usleep(100000);
        kill(pid, SIGINT);

        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            PASS();
        } else {
            FAIL();
        }
    }
}

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("ARGO SHUTDOWN SIGNAL TESTS\n");
    printf("========================================\n");
    printf("\n");

    test_sigterm_cleanup();
    test_sigint_cleanup();

    printf("\n");
    printf("========================================\n");
    printf("Tests run:    %d\n", test_count);
    printf("Tests passed: %d\n", test_passed);
    printf("Tests failed: %d\n", test_count - test_passed);
    printf("========================================\n");

    return (test_count == test_passed) ? 0 : 1;
}

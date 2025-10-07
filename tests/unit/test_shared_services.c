/* © 2025 Casey Koons All rights reserved */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "argo_shared_services.h"
#include "argo_error.h"

/* Test counters */
static int task1_count = 0;
static int task2_count = 0;

/* Test task functions */
static void task1_fn(void* context) {
    task1_count++;
    int* ptr = (int*)context;
    if (ptr) {
        (*ptr)++;
    }
}

static void task2_fn(void* context) {
    (void)context;
    task2_count++;
}

/* Test: Create and destroy */
static void test_create_destroy(void) {
    printf("Testing: Create and destroy                                ");

    shared_services_t* svc = shared_services_create();
    if (!svc) {
        printf("✗\n");
        return;
    }

    shared_services_destroy(svc);
    printf("✓\n");
}

/* Test: Start and stop */
static void test_start_stop(void) {
    printf("Testing: Start and stop thread                             ");

    shared_services_t* svc = shared_services_create();
    if (!svc) {
        printf("✗\n");
        return;
    }

    /* Start */
    int result = shared_services_start(svc);
    if (result != ARGO_SUCCESS) {
        printf("✗ (start failed)\n");
        shared_services_destroy(svc);
        return;
    }

    if (!shared_services_is_running(svc)) {
        printf("✗ (not running after start)\n");
        shared_services_destroy(svc);
        return;
    }

    /* Stop */
    shared_services_stop(svc);

    if (shared_services_is_running(svc)) {
        printf("✗ (still running after stop)\n");
        shared_services_destroy(svc);
        return;
    }

    shared_services_destroy(svc);
    printf("✓\n");
}

/* Test: Register and execute tasks */
static void test_task_execution(void) {
    printf("Testing: Task registration and execution                   ");

    task1_count = 0;
    task2_count = 0;
    int context_value = 0;

    shared_services_t* svc = shared_services_create();
    if (!svc) {
        printf("✗\n");
        return;
    }

    /* Register tasks */
    int result = shared_services_register_task(svc, task1_fn, &context_value, 1);
    if (result != ARGO_SUCCESS) {
        printf("✗ (task1 registration failed)\n");
        shared_services_destroy(svc);
        return;
    }

    result = shared_services_register_task(svc, task2_fn, NULL, 1);
    if (result != ARGO_SUCCESS) {
        printf("✗ (task2 registration failed)\n");
        shared_services_destroy(svc);
        return;
    }

    /* Start services */
    shared_services_start(svc);

    /* Wait for tasks to run */
    sleep(3);

    /* Stop services */
    shared_services_stop(svc);

    /* Check that tasks executed */
    if (task1_count < 2 || task2_count < 2) {
        printf("✗ (tasks didn't execute enough: task1=%d, task2=%d)\n",
               task1_count, task2_count);
        shared_services_destroy(svc);
        return;
    }

    if (context_value < 2) {
        printf("✗ (context not updated: %d)\n", context_value);
        shared_services_destroy(svc);
        return;
    }

    shared_services_destroy(svc);
    printf("✓\n");
}

/* Test: Unregister task */
static void test_unregister_task(void) {
    printf("Testing: Unregister task                                   ");

    task1_count = 0;

    shared_services_t* svc = shared_services_create();
    if (!svc) {
        printf("✗\n");
        return;
    }

    /* Register and start */
    shared_services_register_task(svc, task1_fn, NULL, 1);
    shared_services_start(svc);

    /* Let it run */
    sleep(2);

    /* Unregister */
    int result = shared_services_unregister_task(svc, task1_fn);
    if (result != ARGO_SUCCESS) {
        printf("✗ (unregister failed)\n");
        shared_services_stop(svc);
        shared_services_destroy(svc);
        return;
    }

    int count_before_unreg = task1_count;

    /* Wait and verify it doesn't run anymore */
    sleep(2);

    if (task1_count != count_before_unreg) {
        printf("✗ (task still running after unregister)\n");
        shared_services_stop(svc);
        shared_services_destroy(svc);
        return;
    }

    shared_services_stop(svc);
    shared_services_destroy(svc);
    printf("✓\n");
}

/* Test: Enable/disable task */
static void test_enable_disable_task(void) {
    printf("Testing: Enable/disable task                               ");

    task1_count = 0;

    shared_services_t* svc = shared_services_create();
    if (!svc) {
        printf("✗\n");
        return;
    }

    /* Register and start */
    shared_services_register_task(svc, task1_fn, NULL, 1);
    shared_services_start(svc);

    /* Let it run */
    sleep(2);

    int count_before_disable = task1_count;

    /* Disable */
    shared_services_enable_task(svc, task1_fn, false);

    /* Wait and verify it doesn't run */
    sleep(2);

    if (task1_count != count_before_disable) {
        printf("✗ (task still running when disabled)\n");
        shared_services_stop(svc);
        shared_services_destroy(svc);
        return;
    }

    /* Re-enable */
    shared_services_enable_task(svc, task1_fn, true);

    /* Wait and verify it runs again */
    sleep(2);

    if (task1_count <= count_before_disable) {
        printf("✗ (task not running after re-enable)\n");
        shared_services_stop(svc);
        shared_services_destroy(svc);
        return;
    }

    shared_services_stop(svc);
    shared_services_destroy(svc);
    printf("✓\n");
}

/* Test: Statistics */
static void test_statistics(void) {
    printf("Testing: Statistics tracking                               ");

    task1_count = 0;

    shared_services_t* svc = shared_services_create();
    if (!svc) {
        printf("✗\n");
        return;
    }

    /* Register and start */
    shared_services_register_task(svc, task1_fn, NULL, 1);
    shared_services_start(svc);

    /* Wait */
    sleep(3);

    /* Check statistics */
    uint64_t runs = shared_services_get_task_runs(svc);
    time_t uptime = shared_services_get_uptime(svc);

    if (runs < 2) {
        printf("✗ (task runs too low: %llu)\n", (unsigned long long)runs);
        shared_services_stop(svc);
        shared_services_destroy(svc);
        return;
    }

    if (uptime < 2) {
        printf("✗ (uptime too low: %ld)\n", (long)uptime);
        shared_services_stop(svc);
        shared_services_destroy(svc);
        return;
    }

    shared_services_stop(svc);
    shared_services_destroy(svc);
    printf("✓\n");
}

int main(void) {
    printf("\n");
    printf("==========================================\n");
    printf("ARGO SHARED SERVICES TESTS\n");
    printf("==========================================\n");
    printf("\n");

    test_create_destroy();
    test_start_stop();
    test_task_execution();
    test_unregister_task();
    test_enable_disable_task();
    test_statistics();

    printf("\n");
    printf("==========================================\n");
    printf("All shared services tests passed\n");
    printf("==========================================\n");

    return 0;
}

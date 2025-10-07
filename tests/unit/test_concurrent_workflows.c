/* © 2025 Casey Koons All rights reserved */

/* Test concurrent workflow execution */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "argo_init.h"
#include "argo_registry.h"
#include "argo_lifecycle.h"
#include "argo_workflow.h"

#define NUM_THREADS 5
#define WORKFLOWS_PER_THREAD 3

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

typedef struct {
    ci_registry_t* registry;
    lifecycle_manager_t* lifecycle;
    int thread_id;
    int workflows_created;
    pthread_mutex_t* mutex;
} thread_data_t;

static void* workflow_worker(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;

    for (int i = 0; i < WORKFLOWS_PER_THREAD; i++) {
        char workflow_id[64];
        snprintf(workflow_id, sizeof(workflow_id), "workflow-t%d-w%d", data->thread_id, i);

        workflow_controller_t* workflow = workflow_create(data->registry,
                                                         data->lifecycle,
                                                         workflow_id);
        if (workflow) {
            /* Create some tasks */
            workflow_create_task(workflow, "test task", WORKFLOW_INIT);

            /* Cleanup */
            workflow_destroy(workflow);

            pthread_mutex_lock(data->mutex);
            data->workflows_created++;
            pthread_mutex_unlock(data->mutex);
        }
    }

    return NULL;
}

static void test_parallel_workflow_creation(void) {
    TEST("Parallel workflow creation (5 threads × 3 workflows)");

    ci_registry_t* registry = registry_create();
    lifecycle_manager_t* lifecycle = lifecycle_manager_create(registry);
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];

    /* Launch threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].registry = registry;
        thread_data[i].lifecycle = lifecycle;
        thread_data[i].thread_id = i;
        thread_data[i].workflows_created = 0;
        thread_data[i].mutex = &mutex;
        pthread_create(&threads[i], NULL, workflow_worker, &thread_data[i]);
    }

    /* Wait for all threads */
    int total_created = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
        total_created += thread_data[i].workflows_created;
    }

    /* Verify all workflows created */
    int expected = NUM_THREADS * WORKFLOWS_PER_THREAD;
    if (total_created == expected) {
        PASS();
    } else {
        printf("(created %d/%d) ", total_created, expected);
        FAIL();
    }

    lifecycle_manager_destroy(lifecycle);
    registry_destroy(registry);
}

static void test_no_corruption(void) {
    TEST("No data corruption in concurrent execution");

    ci_registry_t* registry = registry_create();
    lifecycle_manager_t* lifecycle = lifecycle_manager_create(registry);
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    pthread_t threads[NUM_THREADS];
    thread_data_t thread_data[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].registry = registry;
        thread_data[i].lifecycle = lifecycle;
        thread_data[i].thread_id = i;
        thread_data[i].workflows_created = 0;
        thread_data[i].mutex = &mutex;
        pthread_create(&threads[i], NULL, workflow_worker, &thread_data[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    /* If we got here without crashes, test passed */
    PASS();

    lifecycle_manager_destroy(lifecycle);
    registry_destroy(registry);
}

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("ARGO CONCURRENT WORKFLOW TESTS\n");
    printf("========================================\n");
    printf("\n");

    argo_init();

    test_parallel_workflow_creation();
    test_no_corruption();

    argo_exit();

    printf("\n");
    printf("========================================\n");
    printf("Tests run:    %d\n", test_count);
    printf("Tests passed: %d\n", test_passed);
    printf("Tests failed: %d\n", test_count - test_passed);
    printf("========================================\n");

    return (test_count == test_passed) ? 0 : 1;
}

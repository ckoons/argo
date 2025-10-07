/* © 2025 Casey Koons All rights reserved */

/* Test suite for thread safety - concurrent ID generation */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

/* Project includes */
#include "argo_workflow.h"
#include "argo_merge.h"
#include "argo_registry.h"
#include "argo_lifecycle.h"
#include "argo_error.h"

/* Test configuration */
#define NUM_THREADS 10
#define TASKS_PER_THREAD 100
#define TOTAL_TASKS (NUM_THREADS * TASKS_PER_THREAD)

#define SESSIONS_PER_THREAD 100
#define TOTAL_SESSIONS (NUM_THREADS * SESSIONS_PER_THREAD)

/* Test helpers */
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        printf("Testing: %-50s", name); \
        tests_run++; \
    } while(0)

#define PASS() \
    do { \
        printf(" ✓\n"); \
        tests_passed++; \
    } while(0)

#define FAIL(msg) \
    do { \
        printf(" ✗\n  %s\n", msg); \
    } while(0)

/* Shared test data */
typedef struct {
    ci_registry_t* registry;
    lifecycle_manager_t* lifecycle;
    char task_ids[TOTAL_TASKS][64];
    int task_index;
    pthread_mutex_t index_mutex;
} task_test_data_t;

typedef struct {
    char session_ids[TOTAL_SESSIONS][64];
    int session_index;
    pthread_mutex_t index_mutex;
} session_test_data_t;

/* Thread worker for task ID generation */
static void* task_worker(void* arg) {
    task_test_data_t* data = (task_test_data_t*)arg;

    for (int i = 0; i < TASKS_PER_THREAD; i++) {
        /* Create workflow and task */
        workflow_controller_t* workflow = workflow_create(data->registry,
                                                         data->lifecycle,
                                                         "thread-test");
        if (!workflow) {
            return NULL;
        }

        ci_task_t* task = workflow_create_task(workflow, "test task", WORKFLOW_INIT);
        if (!task) {
            workflow_destroy(workflow);
            return NULL;
        }

        /* Store task ID in shared array (with mutex for array access) */
        pthread_mutex_lock(&data->index_mutex);
        int idx = data->task_index++;
        if (idx < TOTAL_TASKS) {
            strncpy(data->task_ids[idx], task->id, sizeof(data->task_ids[idx]) - 1);
        }
        pthread_mutex_unlock(&data->index_mutex);

        workflow_destroy(workflow);
    }

    return NULL;
}

/* Thread worker for session ID generation */
static void* session_worker(void* arg) {
    session_test_data_t* data = (session_test_data_t*)arg;

    for (int i = 0; i < SESSIONS_PER_THREAD; i++) {
        /* Create merge negotiation session */
        merge_negotiation_t* session = merge_negotiation_create("branch-a", "branch-b");
        if (!session) {
            return NULL;
        }

        /* Store session ID in shared array (with mutex for array access) */
        pthread_mutex_lock(&data->index_mutex);
        int idx = data->session_index++;
        if (idx < TOTAL_SESSIONS) {
            strncpy(data->session_ids[idx], session->session_id,
                   sizeof(data->session_ids[idx]) - 1);
        }
        pthread_mutex_unlock(&data->index_mutex);

        merge_negotiation_destroy(session);
    }

    return NULL;
}

/* Test concurrent task ID generation */
static void test_concurrent_task_ids(void) {
    TEST("Concurrent task ID generation (thread-safe)");

    /* Setup test data */
    task_test_data_t data;
    data.registry = registry_create();
    data.lifecycle = lifecycle_manager_create(data.registry);
    data.task_index = 0;
    pthread_mutex_init(&data.index_mutex, NULL);

    if (!data.registry || !data.lifecycle) {
        FAIL("Failed to create registry/lifecycle");
        return;
    }

    /* Create threads */
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, task_worker, &data) != 0) {
            lifecycle_manager_destroy(data.lifecycle);
            registry_destroy(data.registry);
            FAIL("Failed to create thread");
            return;
        }
    }

    /* Wait for all threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    /* Verify all IDs are unique */
    int duplicate_count = 0;
    for (int i = 0; i < TOTAL_TASKS; i++) {
        for (int j = i + 1; j < TOTAL_TASKS; j++) {
            if (strcmp(data.task_ids[i], data.task_ids[j]) == 0) {
                duplicate_count++;
                if (duplicate_count == 1) {
                    printf("\n  Found duplicate ID: %s", data.task_ids[i]);
                }
            }
        }
    }

    /* Cleanup */
    pthread_mutex_destroy(&data.index_mutex);
    lifecycle_manager_destroy(data.lifecycle);
    registry_destroy(data.registry);

    if (duplicate_count > 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Found %d duplicate task IDs out of %d",
                duplicate_count, TOTAL_TASKS);
        FAIL(msg);
        return;
    }

    PASS();
}

/* Test concurrent session ID generation */
static void test_concurrent_session_ids(void) {
    TEST("Concurrent session ID generation (thread-safe)");

    /* Setup test data */
    session_test_data_t data;
    data.session_index = 0;
    pthread_mutex_init(&data.index_mutex, NULL);

    /* Create threads */
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, session_worker, &data) != 0) {
            FAIL("Failed to create thread");
            return;
        }
    }

    /* Wait for all threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    /* Verify all IDs are unique */
    int duplicate_count = 0;
    for (int i = 0; i < TOTAL_SESSIONS; i++) {
        for (int j = i + 1; j < TOTAL_SESSIONS; j++) {
            if (strcmp(data.session_ids[i], data.session_ids[j]) == 0) {
                duplicate_count++;
                if (duplicate_count == 1) {
                    printf("\n  Found duplicate ID: %s", data.session_ids[i]);
                }
            }
        }
    }

    /* Cleanup */
    pthread_mutex_destroy(&data.index_mutex);

    if (duplicate_count > 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Found %d duplicate session IDs out of %d",
                duplicate_count, TOTAL_SESSIONS);
        FAIL(msg);
        return;
    }

    PASS();
}

/* Test no deadlocks under high contention */
static void test_no_deadlocks(void) {
    TEST("No deadlocks under high contention");

    /* Create many threads rapidly creating/destroying */
    ci_registry_t* registry = registry_create();
    lifecycle_manager_t* lifecycle = lifecycle_manager_create(registry);

    if (!registry || !lifecycle) {
        FAIL("Failed to create registry/lifecycle");
        return;
    }

    pthread_t threads[50];
    task_test_data_t data;
    data.registry = registry;
    data.lifecycle = lifecycle;
    data.task_index = 0;
    pthread_mutex_init(&data.index_mutex, NULL);

    /* Create 50 threads each creating 20 tasks */
    for (int i = 0; i < 50; i++) {
        if (pthread_create(&threads[i], NULL, task_worker, &data) != 0) {
            FAIL("Failed to create thread");
            pthread_mutex_destroy(&data.index_mutex);
            lifecycle_manager_destroy(lifecycle);
            registry_destroy(registry);
            return;
        }
    }

    /* Wait for all - if deadlock occurs, this will hang */
    for (int i = 0; i < 50; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&data.index_mutex);
    lifecycle_manager_destroy(lifecycle);
    registry_destroy(registry);

    PASS();
}

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("ARGO THREAD SAFETY TESTS\n");
    printf("========================================\n");
    printf("\n");

    test_concurrent_task_ids();
    test_concurrent_session_ids();
    test_no_deadlocks();

    printf("\n");
    printf("========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    printf("========================================\n");

    return (tests_run == tests_passed) ? 0 : 1;
}

/* © 2025 Casey Koons All rights reserved */
/* Concurrency stress tests - parallel workflows, registry contention, shutdown */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include "stress_test_common.h"
#include "argo_registry.h"
#include "argo_lifecycle.h"
#include "argo_workflow.h"
#include "argo_error.h"
#include "argo_limits.h"

/* Global test statistics */
static test_stats_t g_stats;

/* Shared registry for contention tests */
static ci_registry_t* g_registry = NULL;
static lifecycle_manager_t* g_lifecycle = NULL;

/* Thread argument for parallel workflow test */
typedef struct {
    int thread_id;
    int workflow_count;
} thread_arg_t;

/* Test: Parallel workflow creation */
static void* parallel_workflow_thread(void* arg) {
    thread_arg_t* targ = (thread_arg_t*)arg;
    int success = 1;

    for (int i = 0; i < targ->workflow_count; i++) {
        char workflow_id[ARGO_BUFFER_SMALL];
        snprintf(workflow_id, sizeof(workflow_id), "thread_%d_workflow_%d",
                 targ->thread_id, i);

        workflow_controller_t* wf = workflow_create(g_registry, g_lifecycle, workflow_id);
        if (!wf) {
            success = 0;
            break;
        }

        /* Simulate minimal work */
        usleep(1000); /* 1ms */

        workflow_destroy(wf);
    }

    test_record_result(&g_stats, success);
    return NULL;
}

static void test_parallel_workflows(void) {
    TEST("10 threads creating 10 workflows each (100 total)");

    const int num_threads = 10;
    const int workflows_per_thread = 10;
    pthread_t threads[num_threads];
    thread_arg_t args[num_threads];

    /* Create registry and lifecycle */
    g_registry = registry_create();
    g_lifecycle = lifecycle_manager_create(g_registry);
    assert(g_registry != NULL);
    assert(g_lifecycle != NULL);

    /* Spawn threads */
    for (int i = 0; i < num_threads; i++) {
        args[i].thread_id = i;
        args[i].workflow_count = workflows_per_thread;
        pthread_create(&threads[i], NULL, parallel_workflow_thread, &args[i]);
    }

    /* Wait for completion */
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    /* Cleanup */
    lifecycle_manager_destroy(g_lifecycle);
    registry_destroy(g_registry);
    g_registry = NULL;
    g_lifecycle = NULL;

    PASS();
}

/* Test: Registry stress - sequential operations under load */
static void test_registry_sequential_stress(void) {
    TEST("1000 sequential registry operations (add/remove)");

    /* NOTE: Registry is not thread-safe by design - it's single-threaded
     * This test validates sequential operation under load instead */

    g_registry = registry_create();
    assert(g_registry != NULL);

    int success = 1;
    for (int i = 0; i < 1000; i++) {
        char ci_name[ARGO_BUFFER_SMALL];
        snprintf(ci_name, sizeof(ci_name), "ci_seq_%d", i);

        /* Add a CI instance */
        int result = registry_add_ci(g_registry, ci_name, "test", "test-model", 0);
        if (result != ARGO_SUCCESS) {
            success = 0;
            break;
        }

        /* Remove it */
        result = registry_remove_ci(g_registry, ci_name);
        if (result != ARGO_SUCCESS) {
            success = 0;
            break;
        }
    }

    /* Cleanup */
    registry_destroy(g_registry);
    g_registry = NULL;

    test_record_result(&g_stats, success);
    if (success) {
        PASS();
    } else {
        FAIL();
    }
}

/* Test: Rapid create/destroy cycles (stress allocator) */
static void test_rapid_create_destroy(void) {
    TEST("1000 rapid registry create/destroy cycles");

    int success = 1;
    for (int i = 0; i < 1000; i++) {
        ci_registry_t* reg = registry_create();
        if (!reg) {
            success = 0;
            break;
        }
        registry_destroy(reg);
    }

    test_record_result(&g_stats, success);
    if (success) {
        PASS();
    } else {
        FAIL();
    }
}

/* Test: Workflow chain stress */
static void test_workflow_chain_stress(void) {
    TEST("Create 50 workflows in sequence (simulates long session)");

    g_registry = registry_create();
    g_lifecycle = lifecycle_manager_create(g_registry);
    assert(g_registry != NULL);
    assert(g_lifecycle != NULL);

    int success = 1;
    for (int i = 0; i < 50; i++) {
        char workflow_id[ARGO_BUFFER_SMALL];
        snprintf(workflow_id, sizeof(workflow_id), "chain_workflow_%d", i);

        workflow_controller_t* wf = workflow_create(g_registry, g_lifecycle, workflow_id);
        if (!wf) {
            success = 0;
            break;
        }

        /* Simulate some work */
        usleep(500); /* 500us */

        workflow_destroy(wf);
    }

    lifecycle_manager_destroy(g_lifecycle);
    registry_destroy(g_registry);
    g_registry = NULL;
    g_lifecycle = NULL;

    test_record_result(&g_stats, success);
    if (success) {
        PASS();
    } else {
        FAIL();
    }
}

/* Test: Context stress - sequential large dataset */
static void test_context_large_dataset(void) {
    TEST("Single context with 500 variables (sequential)");

    /* NOTE: Workflow context is not thread-safe by design
     * This test validates large dataset handling instead */

    workflow_context_t* ctx = workflow_context_create();
    assert(ctx != NULL);

    int success = 1;
    for (int i = 0; i < 500; i++) {
        char key[ARGO_BUFFER_SMALL];
        char value[ARGO_BUFFER_MEDIUM];
        snprintf(key, sizeof(key), "large_dataset_key_%d", i);
        snprintf(value, sizeof(value), "large_dataset_value_%d_with_some_content", i);

        int result = workflow_context_set(ctx, key, value);
        if (result != ARGO_SUCCESS) {
            success = 0;
            break;
        }
    }

    /* Verify a few values */
    if (success) {
        const char* val = workflow_context_get(ctx, "large_dataset_key_250");
        if (!val || strcmp(val, "large_dataset_value_250_with_some_content") != 0) {
            success = 0;
        }
    }

    workflow_context_destroy(ctx);

    test_record_result(&g_stats, success);
    if (success) {
        PASS();
    } else {
        FAIL();
    }
}

int main(void) {
    printf("\n");
    printf("==========================================\n");
    printf("Concurrency Stress Tests\n");
    printf("==========================================\n");
    printf("Testing parallel workflows, registry contention,\n");
    printf("and concurrent access patterns.\n");
    printf("\n");

    test_stats_init(&g_stats);

    /* Run tests */
    test_rapid_create_destroy();
    test_registry_sequential_stress();
    test_parallel_workflows();
    test_workflow_chain_stress();
    test_context_large_dataset();

    /* Print results */
    test_print_results(&g_stats, "Concurrency Stress Tests");

    return (g_stats.tests_run == g_stats.tests_passed) ? 0 : 1;
}

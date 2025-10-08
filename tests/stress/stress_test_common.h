/* © 2025 Casey Koons All rights reserved */
/* Common utilities for stress testing */

#ifndef STRESS_TEST_COMMON_H
#define STRESS_TEST_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <time.h>

/* Test statistics */
typedef struct {
    int tests_run;
    int tests_passed;
    int tests_failed;
    pthread_mutex_t lock;
} test_stats_t;

/* Initialize test stats */
static inline void test_stats_init(test_stats_t* stats) {
    stats->tests_run = 0;
    stats->tests_passed = 0;
    stats->tests_failed = 0;
    pthread_mutex_init(&stats->lock, NULL);
}

/* Record test result (thread-safe) */
static inline void test_record_result(test_stats_t* stats, int passed) {
    pthread_mutex_lock(&stats->lock);
    stats->tests_run++;
    if (passed) {
        stats->tests_passed++;
    } else {
        stats->tests_failed++;
    }
    pthread_mutex_unlock(&stats->lock);
}

/* Print test results */
static inline void test_print_results(test_stats_t* stats, const char* suite_name) {
    printf("\n");
    printf("==========================================\n");
    printf("%s Results\n", suite_name);
    printf("==========================================\n");
    printf("Tests run:    %d\n", stats->tests_run);
    printf("Tests passed: %d\n", stats->tests_passed);
    printf("Tests failed: %d\n", stats->tests_failed);
    printf("==========================================\n");
    pthread_mutex_destroy(&stats->lock);
}

/* Test macros */
#define TEST(name) do { \
    printf("Testing: %-50s ", name); \
    fflush(stdout); \
} while(0)

#define PASS() do { \
    printf("✓\n"); \
} while(0)

#define FAIL() do { \
    printf("✗\n"); \
} while(0)

/* Timing utilities */
static inline double get_elapsed_seconds(struct timespec* start) {
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);
    return (end.tv_sec - start->tv_sec) +
           (end.tv_nsec - start->tv_nsec) / 1000000000.0;
}

#endif /* STRESS_TEST_COMMON_H */

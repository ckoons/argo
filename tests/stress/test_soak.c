/* © 2025 Casey Koons All rights reserved */
/* Soak test - runs for extended period to detect memory leaks and stability issues */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/resource.h>
#include "stress_test_common.h"
#include "argo_registry.h"
#include "argo_lifecycle.h"
#include "argo_workflow.h"
#include "argo_workflow_context.h"
#include "argo_error.h"
#include "argo_limits.h"

/* Soak test configuration */
#define SOAK_DURATION_SECONDS 60    /* Default: 1 minute (use 86400 for 24 hours) */
#define WORKFLOWS_PER_CYCLE 10      /* Workflows to create per iteration */
#define CONTEXT_VARS_PER_WF 50      /* Context variables per workflow */

/* Memory statistics */
typedef struct {
    long rss_kb;           /* Resident set size in KB */
    long vm_size_kb;       /* Virtual memory size in KB */
    int open_files;        /* Number of open file descriptors */
} mem_stats_t;

/* Get current memory usage */
static int get_memory_stats(mem_stats_t* stats) {
    FILE* fp = fopen("/proc/self/status", "r");
    if (!fp) {
        /* Try macOS alternative */
        struct rusage usage;
        if (getrusage(RUSAGE_SELF, &usage) == 0) {
            stats->rss_kb = usage.ru_maxrss / 1024;  /* macOS reports in bytes */
            stats->vm_size_kb = 0;  /* Not available via rusage */
            stats->open_files = 0;  /* Would need lsof */
            return ARGO_SUCCESS;
        }
        return E_SYSTEM_FILE;
    }

    char line[256];
    stats->rss_kb = 0;
    stats->vm_size_kb = 0;
    stats->open_files = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%ld", &stats->rss_kb);
        } else if (strncmp(line, "VmSize:", 7) == 0) {
            sscanf(line + 7, "%ld", &stats->vm_size_kb);
        }
    }

    fclose(fp);
    return ARGO_SUCCESS;
}

/* Single workflow cycle */
static int run_workflow_cycle(ci_registry_t* registry, lifecycle_manager_t* lifecycle, int cycle_num) {
    for (int i = 0; i < WORKFLOWS_PER_CYCLE; i++) {
        char workflow_id[ARGO_BUFFER_SMALL];
        snprintf(workflow_id, sizeof(workflow_id), "soak_cycle_%d_wf_%d", cycle_num, i);

        /* Create workflow */
        workflow_controller_t* wf = workflow_create(registry, lifecycle, workflow_id);
        if (!wf) {
            return E_SYSTEM_MEMORY;
        }

        /* Populate context */
        workflow_context_t* ctx = workflow_context_create();
        if (!ctx) {
            workflow_destroy(wf);
            return E_SYSTEM_MEMORY;
        }

        for (int j = 0; j < CONTEXT_VARS_PER_WF; j++) {
            char key[ARGO_BUFFER_SMALL];
            char value[ARGO_BUFFER_MEDIUM];
            snprintf(key, sizeof(key), "var_%d", j);
            snprintf(value, sizeof(value), "value_%d_cycle_%d", j, cycle_num);
            workflow_context_set(ctx, key, value);
        }

        /* Cleanup */
        workflow_context_destroy(ctx);
        workflow_destroy(wf);
    }

    return ARGO_SUCCESS;
}

int main(int argc, char** argv) {
    int duration_seconds = SOAK_DURATION_SECONDS;
    int report_interval = 10; /* Report every 10 seconds */

    /* Parse command line */
    if (argc > 1) {
        duration_seconds = atoi(argv[1]);
        if (duration_seconds <= 0) {
            fprintf(stderr, "Usage: %s [duration_seconds]\n", argv[0]);
            fprintf(stderr, "Example: %s 3600  # Run for 1 hour\n", argv[0]);
            fprintf(stderr, "         %s 86400 # Run for 24 hours\n", argv[0]);
            return 1;
        }
    }

    printf("\n");
    printf("==========================================\n");
    printf("Soak Test\n");
    printf("==========================================\n");
    printf("Duration:           %d seconds (%.1f hours)\n",
           duration_seconds, duration_seconds / 3600.0);
    printf("Workflows/cycle:    %d\n", WORKFLOWS_PER_CYCLE);
    printf("Context vars/WF:    %d\n", CONTEXT_VARS_PER_WF);
    printf("Report interval:    %d seconds\n", report_interval);
    printf("\n");

    /* Create registry and lifecycle */
    ci_registry_t* registry = registry_create();
    lifecycle_manager_t* lifecycle = lifecycle_manager_create(registry);
    if (!registry || !lifecycle) {
        fprintf(stderr, "Failed to initialize test infrastructure\n");
        return 1;
    }

    /* Get baseline memory */
    mem_stats_t baseline_mem, current_mem;
    get_memory_stats(&baseline_mem);
    printf("Baseline memory: RSS=%ld KB, VM=%ld KB\n\n",
           baseline_mem.rss_kb, baseline_mem.vm_size_kb);

    /* Run soak test */
    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    int cycle_count = 0;
    int total_workflows = 0;
    time_t last_report = time(NULL);

    printf("Starting soak test...\n");
    printf("Time(s)  Cycles   Workflows   RSS(KB)    Delta(KB)   Status\n");
    printf("-------  -------  ----------  ---------  ----------  ------\n");

    while (1) {
        double elapsed = get_elapsed_seconds(&start_time);
        if (elapsed >= duration_seconds) {
            break;
        }

        /* Run workflow cycle */
        int result = run_workflow_cycle(registry, lifecycle, cycle_count);
        if (result != ARGO_SUCCESS) {
            fprintf(stderr, "\nError in cycle %d: %d\n", cycle_count, result);
            break;
        }

        cycle_count++;
        total_workflows += WORKFLOWS_PER_CYCLE;

        /* Report progress */
        time_t now = time(NULL);
        if (now - last_report >= report_interval) {
            get_memory_stats(&current_mem);
            long delta_kb = current_mem.rss_kb - baseline_mem.rss_kb;

            printf("%-7.0f  %-7d  %-10d  %-9ld  %-10ld  %s\n",
                   elapsed, cycle_count, total_workflows,
                   current_mem.rss_kb, delta_kb,
                   (delta_kb > 10000) ? "WARNING" : "OK");
            fflush(stdout);

            last_report = now;
        }

        /* Small delay to prevent tight loop */
        usleep(10000); /* 10ms */
    }

    /* Final report */
    double total_time = get_elapsed_seconds(&start_time);
    get_memory_stats(&current_mem);

    printf("\n");
    printf("==========================================\n");
    printf("Soak Test Complete\n");
    printf("==========================================\n");
    printf("Total runtime:      %.1f seconds\n", total_time);
    printf("Cycles completed:   %d\n", cycle_count);
    printf("Total workflows:    %d\n", total_workflows);
    printf("Workflows/second:   %.1f\n", total_workflows / total_time);
    printf("\n");
    printf("Memory Usage:\n");
    printf("  Baseline RSS:     %ld KB\n", baseline_mem.rss_kb);
    printf("  Final RSS:        %ld KB\n", current_mem.rss_kb);
    printf("  Delta RSS:        %ld KB\n", current_mem.rss_kb - baseline_mem.rss_kb);
    printf("  Baseline VM:      %ld KB\n", baseline_mem.vm_size_kb);
    printf("  Final VM:         %ld KB\n", current_mem.vm_size_kb);
    printf("  Delta VM:         %ld KB\n", current_mem.vm_size_kb - baseline_mem.vm_size_kb);
    printf("\n");

    /* Check for memory growth */
    long rss_delta = current_mem.rss_kb - baseline_mem.rss_kb;
    int status = 0;

    if (rss_delta > 10000) {
        printf("WARNING: RSS grew by %ld KB (>10MB)\n", rss_delta);
        printf("Possible memory leak detected!\n");
        status = 1;
    } else {
        printf("PASS: RSS growth within acceptable limits\n");
    }

    printf("==========================================\n");

    /* Cleanup */
    lifecycle_manager_destroy(lifecycle);
    registry_destroy(registry);

    return status;
}

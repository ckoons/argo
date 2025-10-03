/* © 2025 Casey Koons All rights reserved */

/* Workflow execution tracer - logs each step execution with timing */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Project includes */
#include "argo_workflow.h"
#include "argo_init.h"
#include "argo_log.h"

/* Trace entry */
typedef struct trace_entry {
    char step_id[WORKFLOW_STEP_ID_MAX];
    char step_type[64];
    time_t start_time;
    time_t end_time;
    int result;
    struct trace_entry* next;
} trace_entry_t;

static trace_entry_t* trace_head = NULL;
static trace_entry_t* trace_tail = NULL;

/* Add trace entry */
static void add_trace(const char* step_id, const char* step_type,
                      time_t start, time_t end, int result) {
    trace_entry_t* entry = calloc(1, sizeof(trace_entry_t));
    if (!entry) return;

    strncpy(entry->step_id, step_id, sizeof(entry->step_id) - 1);
    strncpy(entry->step_type, step_type, sizeof(entry->step_type) - 1);
    entry->start_time = start;
    entry->end_time = end;
    entry->result = result;
    entry->next = NULL;

    if (trace_tail) {
        trace_tail->next = entry;
        trace_tail = entry;
    } else {
        trace_head = trace_tail = entry;
    }
}

/* Print trace as text */
static void print_trace_text(void) {
    printf("\n");
    printf("========================================\n");
    printf("Workflow Execution Trace\n");
    printf("========================================\n\n");

    trace_entry_t* entry = trace_head;
    while (entry) {
        long duration = entry->end_time - entry->start_time;
        printf("Step %s: %s (%lds) - %s\n",
               entry->step_id,
               entry->step_type,
               duration,
               entry->result == 0 ? "SUCCESS" : "FAILED");
        entry = entry->next;
    }
    printf("\n");
}

/* Print trace as DOT graph */
static void print_trace_dot(void) {
    printf("digraph workflow {\n");
    printf("  rankdir=LR;\n");
    printf("  node [shape=box];\n\n");

    trace_entry_t* entry = trace_head;
    while (entry && entry->next) {
        const char* color = entry->result == 0 ? "green" : "red";
        printf("  \"%s\" [color=%s];\n", entry->step_id, color);
        printf("  \"%s\" -> \"%s\";\n", entry->step_id, entry->next->step_id);
        entry = entry->next;
    }

    if (entry) {
        const char* color = entry->result == 0 ? "green" : "red";
        printf("  \"%s\" [color=%s];\n", entry->step_id, color);
    }

    printf("}\n");
}

/* Main */
int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <workflow.json> [--dot]\n", argv[0]);
        return 1;
    }

    const char* workflow_file = argv[1];
    int output_dot = (argc > 2 && strcmp(argv[2], "--dot") == 0);

    /* Initialize Argo */
    int result = argo_init();
    if (result != ARGO_SUCCESS) {
        fprintf(stderr, "Failed to initialize Argo\n");
        return 1;
    }

    /* Create minimal registry and lifecycle (not used for tracing) */
    ci_registry_t* registry = registry_create();
    lifecycle_manager_t* lifecycle = lifecycle_manager_create(registry);

    /* Create workflow */
    workflow_controller_t* workflow = workflow_create(registry, lifecycle, "tracer");
    if (!workflow) {
        fprintf(stderr, "Failed to create workflow\n");
        goto cleanup;
    }

    /* Load workflow JSON */
    result = workflow_load_json(workflow, workflow_file);
    if (result != ARGO_SUCCESS) {
        fprintf(stderr, "Failed to load workflow: %s\n", workflow_file);
        goto cleanup;
    }

    /* Enable dry-run mode for tracing without execution */
#if ARGO_HAS_DRYRUN
    workflow_set_dryrun(workflow, 1);
#endif

    /* Trace execution */
    printf("Tracing workflow: %s\n", workflow_file);

    while (strcmp(workflow->current_step_id, EXECUTOR_STEP_EXIT) != 0) {
        time_t start = time(NULL);
        result = workflow_execute_current_step(workflow);
        time_t end = time(NULL);

        add_trace(workflow->previous_step_id, "step", start, end, result);

        if (result != ARGO_SUCCESS) {
            fprintf(stderr, "Step failed: %s\n", workflow->current_step_id);
            break;
        }
    }

    /* Output trace */
    if (output_dot) {
        print_trace_dot();
    } else {
        print_trace_text();
    }

cleanup:
    if (workflow) workflow_destroy(workflow);
    if (lifecycle) lifecycle_manager_destroy(lifecycle);
    if (registry) registry_destroy(registry);
    argo_exit();

    return 0;
}

/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>

/* Project includes */
#include "argo_metrics.h"

#if ARGO_HAS_METRICS

/* Global metrics */
argo_metrics_t argo_metrics;

/* Initialize metrics */
void argo_metrics_init(void) {
    atomic_store(&argo_metrics.workflows_started, 0);
    atomic_store(&argo_metrics.workflows_completed, 0);
    atomic_store(&argo_metrics.workflows_failed, 0);
    atomic_store(&argo_metrics.tasks_assigned, 0);
    atomic_store(&argo_metrics.tasks_completed, 0);
    atomic_store(&argo_metrics.api_calls_made, 0);
    atomic_store(&argo_metrics.api_failures, 0);
    atomic_store(&argo_metrics.registry_searches, 0);
    atomic_store(&argo_metrics.heartbeats_received, 0);
    atomic_store(&argo_metrics.messages_sent, 0);
}

/* Reset metrics */
void argo_metrics_reset(void) {
    argo_metrics_init();
}

/* GUIDELINE_APPROVED - Metrics display formatting */
/* Print metrics */
void argo_metrics_print(void) {
    printf("\n");
    printf("========================================\n");
    printf("Argo Runtime Metrics\n");
    printf("========================================\n");
    printf("\n");
    printf("Workflows:\n");
    printf("  Started:    %d\n", ARGO_METRIC_GET(workflows_started));
    printf("  Completed:  %d\n", ARGO_METRIC_GET(workflows_completed));
    printf("  Failed:     %d\n", ARGO_METRIC_GET(workflows_failed));
    printf("\n");
    printf("Tasks:\n");
    printf("  Assigned:   %d\n", ARGO_METRIC_GET(tasks_assigned));
    printf("  Completed:  %d\n", ARGO_METRIC_GET(tasks_completed));
    printf("\n");
    printf("API:\n");
    printf("  Calls:      %d\n", ARGO_METRIC_GET(api_calls_made));
    printf("  Failures:   %d\n", ARGO_METRIC_GET(api_failures));
    printf("\n");
    printf("Registry:\n");
    printf("  Searches:   %d\n", ARGO_METRIC_GET(registry_searches));
    printf("  Heartbeats: %d\n", ARGO_METRIC_GET(heartbeats_received));
    printf("\n");
    printf("Messaging:\n");
    printf("  Sent:       %d\n", ARGO_METRIC_GET(messages_sent));
    printf("\n");
}
/* GUIDELINE_APPROVED_END */

#endif /* ARGO_HAS_METRICS */

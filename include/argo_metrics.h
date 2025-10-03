/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_METRICS_H
#define ARGO_METRICS_H

#include "argo_optional.h"

#if ARGO_HAS_METRICS

#include <stdatomic.h>

/* Runtime metrics for monitoring and debugging */
typedef struct {
    atomic_int workflows_started;
    atomic_int workflows_completed;
    atomic_int workflows_failed;
    atomic_int tasks_assigned;
    atomic_int tasks_completed;
    atomic_int api_calls_made;
    atomic_int api_failures;
    atomic_int registry_searches;
    atomic_int heartbeats_received;
    atomic_int messages_sent;
} argo_metrics_t;

/* Global metrics instance */
extern argo_metrics_t argo_metrics;

/* Initialize metrics system */
void argo_metrics_init(void);

/* Reset all metrics to zero */
void argo_metrics_reset(void);

/* Print metrics summary */
void argo_metrics_print(void);

/* Metric increment macros */
#define ARGO_METRIC_INC(counter) atomic_fetch_add(&argo_metrics.counter, 1)
#define ARGO_METRIC_DEC(counter) atomic_fetch_sub(&argo_metrics.counter, 1)
#define ARGO_METRIC_GET(counter) atomic_load(&argo_metrics.counter)

#else

/* No-op when metrics disabled */
#define ARGO_METRIC_INC(counter) ((void)0)
#define ARGO_METRIC_DEC(counter) ((void)0)
#define ARGO_METRIC_GET(counter) (0)

static inline void argo_metrics_init(void) {}
static inline void argo_metrics_reset(void) {}
static inline void argo_metrics_print(void) {}

#endif /* ARGO_HAS_METRICS */

#endif /* ARGO_METRICS_H */

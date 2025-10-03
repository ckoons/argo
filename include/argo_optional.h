/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_OPTIONAL_H
#define ARGO_OPTIONAL_H

/* Optional features configuration
 *
 * Features are enabled via Makefile flags:
 *   make ENABLE_DRYRUN=1    - Enable dry-run mode
 *   make ENABLE_METRICS=1   - Enable metrics collection
 *
 * Multiple features can be combined:
 *   make ENABLE_DRYRUN=1 ENABLE_METRICS=1
 */

/* Feature detection macros */
#ifdef ARGO_ENABLE_DRYRUN
  #define ARGO_HAS_DRYRUN 1
#else
  #define ARGO_HAS_DRYRUN 0
#endif

#ifdef ARGO_ENABLE_METRICS
  #define ARGO_HAS_METRICS 1
#else
  #define ARGO_HAS_METRICS 0
#endif

/* Feature availability check (for runtime queries) */
typedef struct {
    int has_dryrun;
    int has_metrics;
} argo_features_t;

/* Get enabled features */
static inline argo_features_t argo_get_features(void) {
    argo_features_t features = {
        .has_dryrun = ARGO_HAS_DRYRUN,
        .has_metrics = ARGO_HAS_METRICS
    };
    return features;
}

#endif /* ARGO_OPTIONAL_H */

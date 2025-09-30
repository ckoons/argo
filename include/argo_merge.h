/* © 2025 Casey Koons All rights reserved */

#ifndef ARGO_MERGE_H
#define ARGO_MERGE_H

#include "argo_registry.h"
#include "argo_error.h"

/* Merge conflict structure */
typedef struct merge_conflict {
    char file[256];
    int line_start;
    int line_end;
    char* content_a;
    char* content_b;
    char* resolution;  /* Proposed resolution */
    struct merge_conflict* next;
} merge_conflict_t;

/* Merge proposal from CI */
typedef struct merge_proposal {
    char ci_name[REGISTRY_NAME_MAX];
    char* proposed_resolution;
    int confidence;  /* 0-100 */
    time_t proposed_at;
    struct merge_proposal* next;
} merge_proposal_t;

/* Merge negotiation session */
typedef struct merge_negotiation {
    char session_id[64];
    char branch_a[128];
    char branch_b[128];

    /* Conflicts */
    merge_conflict_t* conflicts;
    int conflict_count;
    int resolved_count;

    /* Proposals from CIs */
    merge_proposal_t* proposals;
    int proposal_count;

    /* Status */
    int completed;
    time_t started_at;
    time_t completed_at;

} merge_negotiation_t;

/* Create negotiation session */
merge_negotiation_t* merge_negotiation_create(const char* branch_a,
                                              const char* branch_b);
void merge_negotiation_destroy(merge_negotiation_t* negotiation);

/* Add conflict to negotiation */
merge_conflict_t* merge_add_conflict(merge_negotiation_t* negotiation,
                                    const char* file,
                                    int line_start,
                                    int line_end,
                                    const char* content_a,
                                    const char* content_b);

/* CI proposes resolution */
int merge_propose_resolution(merge_negotiation_t* negotiation,
                            const char* ci_name,
                            merge_conflict_t* conflict,
                            const char* resolution,
                            int confidence);

/* Select best proposal (highest confidence) */
merge_proposal_t* merge_select_best_proposal(merge_negotiation_t* negotiation);

/* Check if all conflicts resolved */
int merge_is_complete(merge_negotiation_t* negotiation);

/* Generate JSON for CI to review */
char* merge_conflict_to_json(merge_conflict_t* conflict);
char* merge_negotiation_to_json(merge_negotiation_t* negotiation);

#endif /* ARGO_MERGE_H */

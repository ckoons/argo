/* © 2025 Casey Koons All rights reserved */

/* System includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Project includes */
#include "argo_merge.h"
#include "argo_error_messages.h"
#include "argo_log.h"

/* Generate session ID */
static void generate_session_id(char* id_out, size_t len) {
    snprintf(id_out, len, "merge-%ld", (long)time(NULL));
}

/* Create merge negotiation session */
merge_negotiation_t* merge_negotiation_create(const char* branch_a,
                                              const char* branch_b) {
    if (!branch_a || !branch_b) {
        argo_report_error(E_INPUT_NULL, "merge_negotiation_create", ERR_MSG_NULL_POINTER);
        return NULL;
    }

    merge_negotiation_t* negotiation = calloc(1, sizeof(merge_negotiation_t));
    if (!negotiation) {
        argo_report_error(E_SYSTEM_MEMORY, "merge_negotiation_create", ERR_MSG_MEMORY_ALLOC_FAILED);
        return NULL;
    }

    generate_session_id(negotiation->session_id, sizeof(negotiation->session_id));
    strncpy(negotiation->branch_a, branch_a, sizeof(negotiation->branch_a) - 1);
    strncpy(negotiation->branch_b, branch_b, sizeof(negotiation->branch_b) - 1);

    negotiation->conflicts = NULL;
    negotiation->proposals = NULL;
    negotiation->conflict_count = 0;
    negotiation->resolved_count = 0;
    negotiation->proposal_count = 0;
    negotiation->completed = 0;
    negotiation->started_at = time(NULL);
    negotiation->completed_at = 0;

    LOG_INFO("Created merge negotiation %s: %s <-> %s",
             negotiation->session_id, branch_a, branch_b);

    return negotiation;
}

/* Destroy merge negotiation */
void merge_negotiation_destroy(merge_negotiation_t* negotiation) {
    if (!negotiation) return;

    /* Free conflicts */
    merge_conflict_t* conflict = negotiation->conflicts;
    while (conflict) {
        merge_conflict_t* next = conflict->next;
        free(conflict->content_a);
        free(conflict->content_b);
        free(conflict->resolution);
        free(conflict);
        conflict = next;
    }

    /* Free proposals */
    merge_proposal_t* proposal = negotiation->proposals;
    while (proposal) {
        merge_proposal_t* next = proposal->next;
        free(proposal->proposed_resolution);
        free(proposal);
        proposal = next;
    }

    LOG_INFO("Destroyed merge negotiation: %s", negotiation->session_id);
    free(negotiation);
}

/* Add conflict to negotiation */
merge_conflict_t* merge_add_conflict(merge_negotiation_t* negotiation,
                                    const char* file,
                                    int line_start,
                                    int line_end,
                                    const char* content_a,
                                    const char* content_b) {
    if (!negotiation || !file || !content_a || !content_b) {
        argo_report_error(E_INPUT_NULL, "merge_add_conflict", ERR_MSG_NULL_POINTER);
        return NULL;
    }

    merge_conflict_t* conflict = calloc(1, sizeof(merge_conflict_t));
    if (!conflict) {
        argo_report_error(E_SYSTEM_MEMORY, "merge_add_conflict", ERR_MSG_MEMORY_ALLOC_FAILED);
        return NULL;
    }

    strncpy(conflict->file, file, sizeof(conflict->file) - 1);
    conflict->line_start = line_start;
    conflict->line_end = line_end;
    conflict->content_a = strdup(content_a);
    conflict->content_b = strdup(content_b);
    conflict->resolution = NULL;

    /* Add to list */
    conflict->next = negotiation->conflicts;
    negotiation->conflicts = conflict;
    negotiation->conflict_count++;

    LOG_INFO("Added conflict in %s (lines %d-%d)",
             file, line_start, line_end);

    return conflict;
}

/* CI proposes resolution for conflict */
int merge_propose_resolution(merge_negotiation_t* negotiation,
                            const char* ci_name,
                            merge_conflict_t* conflict,
                            const char* resolution,
                            int confidence) {
    if (!negotiation || !ci_name || !conflict || !resolution) {
        return E_INVALID_PARAMS;
    }

    if (confidence < 0 || confidence > 100) {
        LOG_WARN("Invalid confidence value: %d", confidence);
        confidence = 50;  /* Default to medium confidence */
    }

    merge_proposal_t* proposal = calloc(1, sizeof(merge_proposal_t));
    if (!proposal) {
        argo_report_error(E_SYSTEM_MEMORY, "merge_propose_resolution", ERR_MSG_MEMORY_ALLOC_FAILED);
        return E_SYSTEM_MEMORY;
    }

    strncpy(proposal->ci_name, ci_name, sizeof(proposal->ci_name) - 1);
    proposal->proposed_resolution = strdup(resolution);
    proposal->confidence = confidence;
    proposal->proposed_at = time(NULL);

    /* Add to proposals */
    proposal->next = negotiation->proposals;
    negotiation->proposals = proposal;
    negotiation->proposal_count++;

    /* If this is the first or highest confidence proposal, update conflict resolution */
    if (!conflict->resolution ||
        (negotiation->proposals->next &&
         proposal->confidence > ((merge_proposal_t*)negotiation->proposals->next)->confidence)) {
        free(conflict->resolution);
        conflict->resolution = strdup(resolution);
        negotiation->resolved_count++;
    }

    LOG_INFO("CI %s proposed resolution (confidence: %d%%)", ci_name, confidence);

    return ARGO_SUCCESS;
}

/* Select best proposal based on confidence */
merge_proposal_t* merge_select_best_proposal(merge_negotiation_t* negotiation) {
    if (!negotiation || !negotiation->proposals) {
        return NULL;
    }

    merge_proposal_t* best = negotiation->proposals;
    merge_proposal_t* current = negotiation->proposals->next;

    while (current) {
        if (current->confidence > best->confidence) {
            best = current;
        }
        current = current->next;
    }

    return best;
}

/* Check if negotiation is complete */
int merge_is_complete(merge_negotiation_t* negotiation) {
    if (!negotiation) return 0;

    /* All conflicts must have resolutions */
    merge_conflict_t* conflict = negotiation->conflicts;
    while (conflict) {
        if (!conflict->resolution) {
            return 0;  /* Found unresolved conflict */
        }
        conflict = conflict->next;
    }

    return 1;  /* All conflicts resolved */
}

/* Convert conflict to JSON for CI review */
char* merge_conflict_to_json(merge_conflict_t* conflict) {
    if (!conflict) return NULL;

    /* Allocate buffer for JSON */
    size_t max_size = 4096;
    char* json = malloc(max_size);
    if (!json) {
        argo_report_error(E_SYSTEM_MEMORY, "merge_conflict_to_json", ERR_MSG_MEMORY_ALLOC_FAILED);
        return NULL;
    }

    /* Build JSON */
    snprintf(json, max_size,
             "{\n"
             "  \"file\": \"%s\",\n"
             "  \"line_start\": %d,\n"
             "  \"line_end\": %d,\n"
             "  \"content_a\": \"%s\",\n"
             "  \"content_b\": \"%s\"\n"
             "}",
             conflict->file,
             conflict->line_start,
             conflict->line_end,
             conflict->content_a ? conflict->content_a : "",
             conflict->content_b ? conflict->content_b : "");

    return json;
}

/* Convert negotiation to JSON */
char* merge_negotiation_to_json(merge_negotiation_t* negotiation) {
    if (!negotiation) return NULL;

    /* Allocate buffer */
    size_t max_size = 16384;
    char* json = malloc(max_size);
    if (!json) {
        argo_report_error(E_SYSTEM_MEMORY, "merge_negotiation_to_json", ERR_MSG_MEMORY_ALLOC_FAILED);
        return NULL;
    }

    /* Start JSON object */
    int len = snprintf(json, max_size,
                      "{\n"
                      "  \"session_id\": \"%s\",\n"
                      "  \"branch_a\": \"%s\",\n"
                      "  \"branch_b\": \"%s\",\n"
                      "  \"conflict_count\": %d,\n"
                      "  \"resolved_count\": %d,\n"
                      "  \"conflicts\": [\n",
                      negotiation->session_id,
                      negotiation->branch_a,
                      negotiation->branch_b,
                      negotiation->conflict_count,
                      negotiation->resolved_count);

    /* Add conflicts */
    merge_conflict_t* conflict = negotiation->conflicts;
    int first = 1;
    while (conflict && len < (int)max_size - 1024) {
        if (!first) {
            len += snprintf(json + len, max_size - len, ",\n");
        }
        first = 0;

        len += snprintf(json + len, max_size - len,
                       "    {\n"
                       "      \"file\": \"%s\",\n"
                       "      \"lines\": [%d, %d],\n"
                       "      \"resolved\": %s\n"
                       "    }",
                       conflict->file,
                       conflict->line_start,
                       conflict->line_end,
                       conflict->resolution ? "true" : "false");

        conflict = conflict->next;
    }

    /* Close JSON */
    snprintf(json + len, max_size - len,
             "\n  ]\n"
             "}");

    return json;
}
